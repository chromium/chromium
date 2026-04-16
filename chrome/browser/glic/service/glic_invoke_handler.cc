// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_handler.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_invoke_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_handle.h"
namespace glic {

namespace {
constexpr base::TimeDelta kDefaultTimeout = base::Minutes(1);
}  // namespace

static tabs::TabInterface* CreateBrowserAndGetActiveTab(Profile* profile) {
  BrowserWindowCreateParams params(*profile, /*from_user_gesture=*/false);
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
  if (!browser) {
    return nullptr;
  }
  tabs::TabInterface* tab = TabListInterface::From(browser)->GetActiveTab();
  if (!tab) {
    tab = TabListInterface::From(browser)->OpenTab(
        GURL(chrome::kChromeUINewTabURL), -1);
  }
  return tab;
}

// static
GlicInvokeHandler::ResolvedTarget GlicInvokeHandler::ResolveTargetSurface(
    Profile* profile,
    const Target& target) {
  if (const auto* default_surface =
          std::get_if<DefaultSurface>(&target.surface)) {
    BrowserWindowInterface* browser = default_surface->browser;
    if (browser) {
      tabs::TabInterface* tab = TabListInterface::From(browser)->GetActiveTab();
      if (tab) {
        return {tab, /*is_new=*/false};
      }
    }

    tabs::TabInterface* tab = CreateBrowserAndGetActiveTab(profile);
    if (tab) {
      return {tab, /*is_new=*/true};
    }

    return {nullptr, /*is_new=*/false};
  } else if (const auto* new_tab_opt = std::get_if<NewTab>(&target.surface)) {
    BrowserWindowInterface* browser = new_tab_opt->window;
    if (!browser) {
      tabs::TabInterface* tab = CreateBrowserAndGetActiveTab(profile);
      if (tab) {
        return {tab, /*is_new=*/true};
      }
      return {nullptr, /*is_new=*/false};
    }
    tabs::TabInterface* tab = TabListInterface::From(browser)->OpenTab(
        GURL(chrome::kChromeUINewTabURL), -1);
    if (tab) {
      return {tab, /*is_new=*/true};
    }
    return {nullptr, /*is_new=*/false};
  }

  if (const auto* tab_ptr =
          std::get_if<raw_ptr<tabs::TabInterface>>(&target.surface)) {
    return {tab_ptr->get(), /*is_new=*/false};
  }

  return {nullptr, /*is_new=*/false};
}

GlicInvokeHandler::GlicInvokeHandler(
    GlicInstanceImpl& instance,
    ResolvedTarget resolved_target,
    GlicInvokeOptions options,
    std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey,
    CompletionCallback completion_callback)
    : instance_(instance),
      tab_(resolved_target.tab),
      options_(std::move(options)),
      auto_submit_passkey_(auto_submit_passkey),
      completion_callback_(std::move(completion_callback)) {
  if (tab_ && GlicInstanceHelper::From(tab_)) {
    tab_destruction_subscription_ =
        GlicInstanceHelper::From(tab_)->SubscribeToDestruction(
            base::BindRepeating(&GlicInvokeHandler::OnTabClosed,
                                weak_ptr_factory_.GetWeakPtr()));
  }
  if (resolved_target.is_new && tab_ && tab_->GetContents() &&
      tab_->GetContents()->HasUncommittedNavigationInPrimaryMainFrame()) {
    // NOTE: This simple check won't do the right thing for chained navigations
    // or potentially redirects, as the first navigation will finish and we will
    // proceed, but then another navigation will start.
    should_wait_for_load_ = true;
  }
}

GlicInvokeHandler::~GlicInvokeHandler() = default;

void GlicInvokeHandler::Invoke() {
  timeout_timer_.Start(FROM_HERE, options_.timeout.value_or(kDefaultTimeout),
                       base::BindOnce(&GlicInvokeHandler::OnError,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      GlicInvokeError::kTimeout));

  if (!tab_destruction_subscription_ || !tab_) {
    OnError(GlicInvokeError::kInvalidTab);
    return;
  }

  std::vector<std::unique_ptr<GlicInvokeTask>> tasks;

  if (should_wait_for_load_) {
    tasks.push_back(
        std::make_unique<WaitForNavigationTask>(tab_->GetContents()));
  }

  auto show_options = ShowOptions::ForSidePanel(
      *tab_, GlicPinTrigger::kInstanceCreation, options_.invocation_source);
  if (options_.fre_override != mojom::FreOverride::kUnspecified) {
    if (RequiresOverrideIncompatibleFre()) {
      OnError(GlicInvokeError::kInvalidConfiguration);
      return;
    }

    show_options.fre_override = options_.fre_override;
  }

  if (auto_submit_passkey_ && RequiresAutoSubmitIncompatibleFre()) {
    OnError(GlicInvokeError::kInvalidConfiguration);
    return;
  }

  tasks.push_back(
      std::make_unique<ShowInstanceTask>(&*instance_, show_options));
  tasks.push_back(
      std::make_unique<WaitForClientConnectedTask>(&instance_->host()));
  if (options_.wait_for_panel_open) {
    tasks.push_back(std::make_unique<StabilizationTask>(tab_->GetContents()));
  }

  tasks.push_back(std::make_unique<WaitForFreCompletionTask>(
      instance_->profile(), options_.fre_override));

  main_task_ = std::make_unique<SequentialTaskGroup>(std::move(tasks));

  main_task_->Start(base::BindOnce(&GlicInvokeHandler::SendToClient,
                                   weak_ptr_factory_.GetWeakPtr()));
}

bool GlicInvokeHandler::RequiresAutoSubmitIncompatibleFre() const {
  if (GlicEnabling::HasConsentedForProfile(instance_->profile())) {
    return false;
  }
  if (options_.fre_override != mojom::FreOverride::kUnspecified) {
    return options_.fre_override != mojom::FreOverride::kTrustFirstInline &&
           options_.fre_override != mojom::FreOverride::kTrustFirstClick;
  }
  return GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(
             instance_->profile()) &&
         features::kGlicTrustFirstOnboardingArmParam.Get() == 1;
}

bool GlicInvokeHandler::RequiresOverrideIncompatibleFre() const {
  if (GlicEnabling::HasConsentedForProfile(instance_->profile())) {
    return false;
  }
  return !GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(
      instance_->profile());
}

void GlicInvokeHandler::SendToClient() {
  if (!instance_->host().IsWebClientConnected()) {
    OnError(GlicInvokeError::kTimeout);
    return;
  }

  if (auto_submit_passkey_) {
    instance_->host().InvokeWithAutoSubmit(
        *auto_submit_passkey_, CreateMojoOptions(),
        base::BindOnce(&GlicInvokeHandler::OnSuccess,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    instance_->host().Invoke(CreateMojoOptions(),
                             base::BindOnce(&GlicInvokeHandler::OnSuccess,
                                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void GlicInvokeHandler::OnTabClosed(tabs::TabInterface* tab) {
  tab_ = nullptr;
  OnError(GlicInvokeError::kTabClosed);
}

void GlicInvokeHandler::OnSuccess() {
  timeout_timer_.Stop();

  if (options_.on_success) {
    std::move(options_.on_success).Run();
  }
  if (completion_callback_) {
    std::move(completion_callback_).Run(&*instance_, this);
  }
}

void GlicInvokeHandler::OnError(GlicInvokeError error) {
  timeout_timer_.Stop();

  if (options_.on_error) {
    std::move(options_.on_error).Run(error);
  }
  if (completion_callback_) {
    std::move(completion_callback_).Run(&*instance_, this);
  }
}

mojom::InvokeOptionsPtr GlicInvokeHandler::CreateMojoOptions() {
  auto mojo_options = mojom::InvokeOptions::New();
  mojo_options->invocation_source = options_.invocation_source;

  if (!options_.prompts.empty()) {
    mojo_options->prompts = options_.prompts;
  }

  if (options_.additional_context) {
    mojo_options->context = std::move(options_.additional_context);
  }

  mojo_options->auto_submit = auto_submit_passkey_.has_value();
  mojo_options->feature_mode =
      options_.feature_mode.value_or(mojom::FeatureMode::kUnspecified);
  mojo_options->disable_zero_state_suggestions = options_.disable_zss;

  if (options_.skill_id) {
    mojo_options->skill_id = *options_.skill_id;
  }

  return mojo_options;
}

}  // namespace glic
