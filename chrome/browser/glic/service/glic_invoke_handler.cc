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
#include "build/build_config.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_invoke_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"

namespace glic {

namespace {
constexpr base::TimeDelta kDefaultTimeout = base::Minutes(1);

}  // namespace

#if !BUILDFLAG(IS_ANDROID)
static tabs::TabInterface* CreateBrowserAndGetActiveTab(Profile* profile) {
  BrowserWindowInterface* browser = chrome::OpenEmptyWindow(profile);
  if (!browser) {
    return nullptr;
  }
  tabs::TabInterface* tab = TabListInterface::From(browser)->GetActiveTab();
  if (!tab) {
    tab = TabListInterface::From(browser)->OpenTab(
        chrome::ChromeUINewTabURLAsGURL(), -1);
  }
  return tab;
}
#endif

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

#if !BUILDFLAG(IS_ANDROID)
    tabs::TabInterface* tab = CreateBrowserAndGetActiveTab(profile);
    if (tab) {
      return {tab, /*is_new=*/true};
    }
#endif

    return {nullptr, /*is_new=*/false};
  } else if (const auto* new_tab_opt = std::get_if<NewTab>(&target.surface)) {
    BrowserWindowInterface* browser = new_tab_opt->window;
    if (!browser) {
#if !BUILDFLAG(IS_ANDROID)
      tabs::TabInterface* tab = CreateBrowserAndGetActiveTab(profile);
      if (tab) {
        return {tab, /*is_new=*/true};
      }
#endif
      return {nullptr, /*is_new=*/false};
    }
    tabs::TabInterface* tab = TabListInterface::From(browser)->OpenTab(
        chrome::ChromeUINewTabURLAsGURL(), -1, new_tab_opt->open_in_foreground);
    if (tab) {
      // TODO(b/503310855): Test/handle Android invocations to see if same
      // issue is present.
#if !BUILDFLAG(IS_ANDROID)
      if (!new_tab_opt->open_in_foreground) {
        // Force the background tab to start loading. Chromium defers loading
        // for background tabs to save resources. Calling WasShown() and then
        // WasHidden() tricks the WebContents into thinking it was shown,
        // triggering the load.
        content::WebContents* contents = tab->GetContents();
        if (contents) {
          contents->WasShown();
          contents->WasHidden();
        }
      }
#endif
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
    GlicInvokeWithAutoSubmitOptions auto_submit_options,
    std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey,
    CompletionCallback completion_callback)
    : instance_(instance),
      tab_(resolved_target.tab),
      options_(std::move(options)),
      auto_submit_passkey_(auto_submit_passkey),
      auto_submit_options_(std::move(auto_submit_options)),
      completion_callback_(std::move(completion_callback)) {
  if (resolved_target.is_new && tab_ && tab_->GetContents() &&
      tab_->GetContents()->HasUncommittedNavigationInPrimaryMainFrame()) {
    // NOTE: This simple check won't do the right thing for chained navigations
    // or potentially redirects, as the first navigation will finish and we will
    // proceed, but then another navigation will start.
    should_wait_for_load_ = true;
  }

  CHECK(tab_);

  // As the handler holds a raw_ptr to GlicInstanceImpl and TabInterface, it
  // must listen to destruction of both.
  tab_destruction_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
      &GlicInvokeHandler::OnTabWillDetach, weak_ptr_factory_.GetWeakPtr()));

  instance_destruction_subscription_ = instance_->RegisterWillBeDestroyed(
      base::BindOnce(&GlicInvokeHandler::OnInstanceWillBeDestroyed,
                     weak_ptr_factory_.GetWeakPtr()));
}

GlicInvokeHandler::~GlicInvokeHandler() = default;

void GlicInvokeHandler::Invoke() {
  timeout_timer_.Start(FROM_HERE, options_.timeout.value_or(kDefaultTimeout),
                       base::BindOnce(&GlicInvokeHandler::OnError,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      GlicInvokeError::kTimeout));

  if (auto_submit_options_.on_conversation_id_ready) {
    if (auto conv_id = instance_->conversation_id()) {
      std::move(auto_submit_options_.on_conversation_id_ready).Run(*conv_id);
    } else {
      conversation_subscription_ =
          instance_->AddConversationInfoChangedCallback(
              base::BindRepeating(&GlicInvokeHandler::OnConversationInfoChanged,
                                  weak_ptr_factory_.GetWeakPtr()));
    }
  }

  if (!options_.tab_sharing.tabs_to_pin.empty()) {
    CHECK(options_.tab_sharing.pin_trigger != GlicPinTrigger::kUnknown);
    instance_->sharing_manager().PinTabs(options_.tab_sharing.tabs_to_pin,
                                         options_.tab_sharing.pin_trigger);
  }

  std::vector<std::unique_ptr<GlicInvokeTask>> tasks;

  if (should_wait_for_load_) {
    tasks.push_back(
        std::make_unique<WaitForNavigationTask>(tab_->GetContents()));
  }

  if (options_.additional_context.has_value() &&
      options_.additional_context->policy_check == PolicyCheck::kClipboard) {
    tasks.push_back(std::make_unique<CopyPolicyTask>(
        &*instance_, options_,
        base::BindOnce(&GlicInvokeHandler::OnError,
                       weak_ptr_factory_.GetWeakPtr())));
  }

  auto show_options = ShowOptions::ForSidePanel(
      *tab_, GlicPinTrigger::kInstanceCreation, options_.invocation_source);

  if (options_.fre_override != mojom::FreOverride::kUnspecified) {
    show_options.fre_override = options_.fre_override;
  }

  if (!auto_submit_passkey_.has_value() || auto_submit_options_.show_panel) {
    tasks.push_back(
        std::make_unique<ShowInstanceTask>(&*instance_, show_options));
  } else {
    tasks.push_back(std::make_unique<SetupHiddenPanelTask>(&*instance_, tab_));
  }
  tasks.push_back(std::make_unique<MaybeInitializeHiddenClientTask>(
      &*instance_, options_.invocation_source, options_.fre_override));
  tasks.push_back(
      std::make_unique<WaitForClientConnectedTask>(&instance_->host()));
  if (options_.on_client_connected) {
    tasks.push_back(std::make_unique<PostCallbackTask>(base::BindOnce(
        [](base::WeakPtr<GlicInstanceImpl> instance,
           base::OnceCallback<void(base::WeakPtr<GlicInstance>)> cb) {
          if (instance) {
            std::move(cb).Run(instance);
          }
        },
        instance_->GetWeakPtr(), std::move(options_.on_client_connected))));
  }
  // TODO(b/505086089): Handle client disconnects.
  tasks.push_back(std::make_unique<NotifyIsInvokingTask>(&instance_->host()));

  if (options_.wait_for_panel_open) {
    tasks.push_back(std::make_unique<StabilizationTask>(tab_->GetContents()));
  }

  if (options_.additional_context.has_value() &&
      options_.additional_context->policy_check == PolicyCheck::kClipboard) {
    tasks.push_back(std::make_unique<PastePolicyCheckTask>(
        tab_->GetContents(), &*instance_, options_,
        base::BindOnce(&GlicInvokeHandler::OnError,
                       weak_ptr_factory_.GetWeakPtr())));
  }

  tasks.push_back(std::make_unique<WaitForFreCompletionTask>(
      instance_->profile(), options_.fre_override));

  tasks.push_back(std::make_unique<SendToClientTask>(
      &*instance_, CreateMojoOptions(), auto_submit_passkey_));

  if (IsActuatingFeatureMode()) {
    auto on_actuation_started = base::BindOnce(
        [](base::WeakPtr<GlicInvokeHandler> handler) {
          if (handler) {
            handler->timeout_timer_.Stop();
          }
        },
        weak_ptr_factory_.GetWeakPtr());

    tasks.push_back(std::make_unique<WaitForActuationTask>(
        &*instance_, options_.timeout.value_or(kDefaultTimeout),
        base::BindOnce(&GlicInvokeHandler::OnError,
                       weak_ptr_factory_.GetWeakPtr()),
        std::move(on_actuation_started)));
  }

  main_task_ = std::make_unique<SequentialTaskGroup>(std::move(tasks));

  main_task_->Start(base::BindOnce(&GlicInvokeHandler::OnSuccess,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void GlicInvokeHandler::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    OnError(GlicInvokeError::kTabClosed);
  }
}

void GlicInvokeHandler::OnInstanceWillBeDestroyed(GlicInstance* instance) {
  OnError(GlicInvokeError::kInstanceDestroyed);
}

void GlicInvokeHandler::OnConversationInfoChanged(
    const mojom::ConversationInfo& info) {
  if (auto_submit_options_.on_conversation_id_ready) {
    std::move(auto_submit_options_.on_conversation_id_ready)
        .Run(info.conversation_id);
  }
  conversation_subscription_ = {};
}

void GlicInvokeHandler::OnSuccess() {
  timeout_timer_.Stop();
  if (main_task_) {
    main_task_->NotifySequenceCompleted(/*success=*/true);
  }

  if (options_.on_success) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(options_.on_success));
  }
  if (completion_callback_) {
    std::move(completion_callback_).Run(&*instance_, this);
  }
}

void GlicInvokeHandler::OnError(GlicInvokeError error) {
  timeout_timer_.Stop();
  instance_->SuppressShowOnNextTabAddedToTask(false);
  if (main_task_) {
    main_task_->NotifySequenceCompleted(/*success=*/false);
  }

  if (options_.on_error) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(options_.on_error), error));
  }

  // The completion callback deletes `this`.
  std::move(completion_callback_).Run(&*instance_, this);
}

bool GlicInvokeHandler::IsActuatingFeatureMode() const {
  if (!options_.feature_mode.has_value()) {
    return false;
  }
  switch (*options_.feature_mode) {
    case mojom::FeatureMode::kActuation:
    case mojom::FeatureMode::kExperimentalTriggering:
    case mojom::FeatureMode::kUniversalCart:
      return true;
    default:
      return false;
  }
}

mojom::InvokeOptionsPtr GlicInvokeHandler::CreateMojoOptions() {
  auto mojo_options = mojom::InvokeOptions::New();
  mojo_options->invocation_source = options_.invocation_source;

  if (!options_.prompts.empty()) {
    mojo_options->prompts = options_.prompts;
  }

  if (options_.additional_context && options_.additional_context->context) {
    mojo_options->context = std::move(options_.additional_context->context);
  }

  mojo_options->auto_submit = auto_submit_passkey_.has_value();
  mojo_options->feature_mode =
      options_.feature_mode.value_or(mojom::FeatureMode::kUnspecified);
  mojo_options->actuation_target = options_.target.actuation_target;
  mojo_options->disable_zero_state_suggestions = options_.disable_zss;

  if (options_.skill_id) {
    mojo_options->skill_id = *options_.skill_id;
  }

  if (options_.zss_config) {
    auto mojo_zss_config = mojom::ZssConfig::New();
    mojo_zss_config->additional_content =
        options_.zss_config->additional_content;
    mojo_options->zss_config = std::move(mojo_zss_config);
  }

  return mojo_options;
}

}  // namespace glic
