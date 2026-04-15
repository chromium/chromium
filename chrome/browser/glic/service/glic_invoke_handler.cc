// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_handler.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_handle.h"
namespace glic {

constexpr base::TimeDelta kDefaultTimeout = base::Minutes(1);

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
    waiting_for_load_ = true;
  }
}

GlicInvokeHandler::~GlicInvokeHandler() = default;

void GlicInvokeHandler::Invoke() {
  timeout_timer_.Start(FROM_HERE, options_.timeout.value_or(kDefaultTimeout),
                       base::BindOnce(&GlicInvokeHandler::OnError,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      GlicInvokeError::kTimeout));

  // If we weren't able to set up tab destruction subscription, we should
  // treat this as an error.
  if (!tab_destruction_subscription_ || !tab_) {
    OnError(GlicInvokeError::kInvalidTab);
    return;
  }

  if (waiting_for_load_) {
    Observe(tab_->GetContents());
    return;
  }

  ContinueInvoke();
}

void GlicInvokeHandler::ContinueInvoke() {
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

  instance_->Show(show_options);

  MaybeWaitForWebClientReady();
}

void GlicInvokeHandler::MaybeWaitForWebClientReady() {
  if (instance_->host().IsWebClientConnected()) {
    OnWebClientReady();
  } else {
    host_observation_.Observe(&instance_->host());
  }
}

void GlicInvokeHandler::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }
  Observe(nullptr);
  waiting_for_load_ = false;
  ContinueInvoke();
}

void GlicInvokeHandler::WebClientConnected() {
  host_observation_.Reset();
  OnWebClientReady();
}

void GlicInvokeHandler::OnWebClientReady() {
  MaybeWaitForPanelOpen();
}

void GlicInvokeHandler::MaybeWaitForPanelOpen() {
  if (options_.wait_for_panel_open) {
    MaybeWaitForStableWidth();
  } else {
    MaybeWaitForFreCompletion();
  }
}

void GlicInvokeHandler::MaybeWaitForStableWidth() {
  if (tab_ && tab_->GetContents()) {
    Observe(tab_->GetContents());
  }

  stabilization_timer_.Start(FROM_HERE, base::Milliseconds(300),
                             base::BindOnce(&GlicInvokeHandler::OnStabilized,
                                            weak_ptr_factory_.GetWeakPtr()));
}

void GlicInvokeHandler::PrimaryMainFrameWasResized(bool width_changed) {
  if (stabilization_timer_.IsRunning()) {
    stabilization_timer_.Reset();
  }
}

void GlicInvokeHandler::OnStabilized() {
  Observe(nullptr);
  MaybeWaitForFreCompletion();
}

bool GlicInvokeHandler::RequiresAutoSubmitIncompatibleFre() const {
  if (GlicEnabling::HasConsentedForProfile(instance_->profile())) {
    return false;
  }
  if (options_.fre_override != mojom::FreOverride::kUnspecified) {
    return options_.fre_override != mojom::FreOverride::kTrustFirstInline;
  }
  return GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(
             instance_->profile()) &&
         (features::kGlicTrustFirstOnboardingArmParam.Get() == 1 ||
          features::kGlicTrustFirstOnboardingArmParam.Get() == 2);
}

bool GlicInvokeHandler::RequiresOverrideIncompatibleFre() const {
  if (GlicEnabling::HasConsentedForProfile(instance_->profile())) {
    return false;
  }
  return !GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(
      instance_->profile());
}

bool GlicInvokeHandler::ShouldWaitForFreCompletion() const {
  if (GlicEnabling::HasConsentedForProfile(instance_->profile())) {
    return false;
  }
  if (options_.fre_override == mojom::FreOverride::kTrustFirstClick) {
    return true;
  }
  if (options_.fre_override == mojom::FreOverride::kUnspecified) {
    return GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(
               instance_->profile()) &&
           features::kGlicTrustFirstOnboardingArmParam.Get() == 2;
  }
  return false;
}

void GlicInvokeHandler::MaybeWaitForFreCompletion() {
  if (ShouldWaitForFreCompletion()) {
    if (!profile_ready_state_subscription_) {
      profile_ready_state_subscription_ =
          GlicKeyedService::Get(instance_->profile())
              ->enabling()
              .RegisterProfileReadyStateChanged(base::BindRepeating(
                  &GlicInvokeHandler::OnProfileReadyStateChanged,
                  weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }
  SendToClient();
}

void GlicInvokeHandler::OnProfileReadyStateChanged() {
  if (GlicEnabling::HasConsentedForProfile(instance_->profile())) {
    profile_ready_state_subscription_ = {};
    SendToClient();
  }
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
