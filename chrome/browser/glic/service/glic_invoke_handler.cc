// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_handler.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_invoke_task.h"
#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/base_window.h"

namespace glic {

namespace {
constexpr base::TimeDelta kDefaultTimeout = base::Minutes(1);

ShowOptions CreateShowOptions(
    const GlicInvokeHandler::ResolvedTarget& resolved_target,
    const GlicInvokeOptions& options) {
  // A floating panel can be asked to come up in text mode, e.g. to take a
  // conversation out of live mode. The browser can only request it; leaving
  // live mode is up to the web client.
  const mojom::WebClientMode floating_initial_mode =
      options.target.live_mode_behavior ==
              LiveModeBehavior::kForceFloatingTextMode
          ? mojom::WebClientMode::kText
          : mojom::WebClientMode::kUnknown;
  ShowOptions show_options = std::visit(
      absl::Overload{
          [&](const GlicInvokeHandler::TabSurface& tab_surface) {
            SidePanelShowOptions side_panel_options{*tab_surface.tab};
            // TODO(b/562983414): Infer a more specific pin trigger
            // from the invocation source.
            side_panel_options.pin_trigger = GlicPinTrigger::kInstanceCreation;
            side_panel_options.pin_on_bind = options.pin_on_bind;
            return ShowOptions(side_panel_options);
          },
          [&](Floating) {
            return ShowOptions::ForFloating(
                /*source_tab=*/tabs::TabHandle::Null(), floating_initial_mode);
          }},
      resolved_target);
  show_options.invocation_source = options.GetInvocationSource();
  show_options.focus_on_show = options.focus_on_show;
  return show_options;
}

GlicInvokeError ToInvokeError(InstanceRemovalReason reason) {
  switch (reason) {
    case InstanceRemovalReason::kBlankInstanceClosed:
      return GlicInvokeError::kInstanceDestroyedBlankInstanceClosed;
    case InstanceRemovalReason::kUnbound:
      return GlicInvokeError::kInstanceDestroyedUnbound;
    case InstanceRemovalReason::kArchived:
      return GlicInvokeError::kInstanceDestroyedArchived;
    case InstanceRemovalReason::kSignedOut:
      return GlicInvokeError::kInstanceDestroyedSignedOut;
    case InstanceRemovalReason::kShutdown:
      return GlicInvokeError::kInstanceDestroyedShutdown;
  }
  NOTREACHED();
}

// Returns the mojom::ActuationTarget that `target` maps to. An actor surface
// maps to kTargetSurface.
mojom::ActuationTarget GetMojoActuationTarget(const Target& target) {
  return std::visit(
      absl::Overload{[](mojom::ActuationTarget actuation_target) {
                       return actuation_target;
                     },
                     [](const actor::ActorSurfaceHandle&) {
                       return mojom::ActuationTarget::kTargetSurface;
                     }},
      target.actuation_target);
}

// LINT.IfChange(ClientLoadStateToDiagnosticString)
std::string_view ClientLoadStateToDiagnosticString(ClientLoadState state) {
  switch (state) {
    case ClientLoadState::kLoading:
      return "LOADING";
    case ClientLoadState::kReady:
      return "READY";
    case ClientLoadState::kError:
      return "ERROR";
  }
  NOTREACHED();
}
// LINT.ThenChange(//chrome/browser/glic/glic_enums.h:ClientLoadState)

// LINT.IfChange(ClientLoadErrorReasonToDiagnosticString)
std::string_view ClientLoadErrorReasonToDiagnosticString(
    ClientLoadErrorReason reason) {
  switch (reason) {
    case ClientLoadErrorReason::kUnknown:
      return "UNKNOWN";
    case ClientLoadErrorReason::kUnavailable:
      return "UNAVAILABLE";
    case ClientLoadErrorReason::kIneligibleAccount:
      return "INELIGIBLE_ACCOUNT";
    case ClientLoadErrorReason::kLocationMismatch:
      return "LOCATION_MISMATCH";
    case ClientLoadErrorReason::kDisabledByAdmin:
      return "DISABLED_BY_ADMIN";
    case ClientLoadErrorReason::kOffline:
      return "OFFLINE";
    case ClientLoadErrorReason::kSignIn:
      return "SIGN_IN";
    case ClientLoadErrorReason::kCookieSyncFailed:
      return "COOKIE_SYNC_FAILED";
    case ClientLoadErrorReason::kGuestLoadFailed:
      return "GUEST_LOAD_FAILED";
    case ClientLoadErrorReason::kGuestProcessGone:
      return "GUEST_PROCESS_GONE";
    case ClientLoadErrorReason::kClientError:
      return "CLIENT_ERROR";
    case ClientLoadErrorReason::kClientLoadTimeout:
      return "CLIENT_LOAD_TIMEOUT";
    case ClientLoadErrorReason::kWarmedTimeout:
      return "WARMED_TIMEOUT";
    case ClientLoadErrorReason::kGuestOriginBlocked:
      return "GUEST_ORIGIN_BLOCKED";
    case ClientLoadErrorReason::kGuestApiNotAllowed:
      return "GUEST_API_NOT_ALLOWED";
    case ClientLoadErrorReason::kPendingProviderChange:
      return "PENDING_PROVIDER_CHANGE";
  }
  NOTREACHED();
}
// LINT.ThenChange(//chrome/browser/glic/glic_enums.h:ClientLoadErrorReason)

}  // namespace

std::string FormatInvokeDiagnosticInfo(
    std::optional<GlicTaskType> last_active_task,
    std::optional<ClientLoadState> client_load_state_at_start,
    std::optional<ClientLoadErrorReason> client_load_error_reason) {
  std::vector<std::string> parts;
  if (last_active_task.has_value()) {
    parts.push_back(base::StrCat(
        {"stage=", GlicTaskTypeToDiagnosticString(*last_active_task)}));
  }
  if (client_load_state_at_start.has_value()) {
    parts.push_back(base::StrCat(
        {"load_state_at_start=",
         ClientLoadStateToDiagnosticString(*client_load_state_at_start)}));
  }
  if (client_load_error_reason.has_value()) {
    parts.push_back(base::StrCat(
        {"load_error=",
         ClientLoadErrorReasonToDiagnosticString(*client_load_error_reason)}));
  }
  return base::JoinString(parts, ";");
}

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
    if (browser &&
        browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {
      tabs::TabInterface* tab = TabListInterface::From(browser)->GetActiveTab();
      if (tab) {
        return TabSurface{tab, /*is_new=*/false};
      }
    }

#if !BUILDFLAG(IS_ANDROID)
    tabs::TabInterface* tab = CreateBrowserAndGetActiveTab(profile);
    if (tab) {
      return TabSurface{tab, /*is_new=*/true};
    }
#endif

    return {TabSurface{nullptr, /*is_new=*/false}};
  } else if (const auto* new_tab_opt = std::get_if<NewTab>(&target.surface)) {
    BrowserWindowInterface* browser = new_tab_opt->window;
    if (!browser ||
        browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL) {
#if !BUILDFLAG(IS_ANDROID)
      tabs::TabInterface* tab = CreateBrowserAndGetActiveTab(profile);
      if (tab) {
        return {TabSurface{tab, /*is_new=*/true}};
      }
#endif
      return {TabSurface{/*tab=*/nullptr, /*is_new=*/false}};
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
      return {TabSurface{tab, /*is_new=*/true}};
    }
    return {TabSurface{/*tab=*/nullptr, /*is_new=*/false}};
  } else if (std::holds_alternative<Floating>(target.surface)) {
    return {Floating()};
  }

  if (const auto* tab_handle = std::get_if<tabs::TabHandle>(&target.surface)) {
    tabs::TabInterface* tab = tab_handle->Get();
    if (tab) {
      BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
      // Allow detached / background tabs for actuation, reject non-normal
      // browser windows.
      bool is_background_actuation =
          !browser && GetMojoActuationTarget(target) ==
                          mojom::ActuationTarget::kTargetSurface;
      bool is_normal_browser =
          browser &&
          browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;

      if (is_background_actuation || is_normal_browser) {
        return {TabSurface{tab, /*is_new=*/false}};
      }
      return {TabSurface{/*tab=*/nullptr, /*is_new=*/false}};
    }
  }

  return {TabSurface{/*tab=*/nullptr, /*is_new=*/false}};
}

// static
EmbedderType GlicInvokeHandler::GetEmbedderType(
    const ResolvedTarget& resolved_target) {
  // A tab target is shown in that tab's side panel.
  return std::visit(
      absl::Overload{[](const TabSurface&) { return EmbedderType::kSidePanel; },
                     [](const Floating&) { return EmbedderType::kFloaty; }},
      resolved_target);
}

bool GlicInvokeHandler::IsFloatingTarget() const {
  return std::holds_alternative<Floating>(resolved_target_);
}

bool GlicInvokeHandler::IsTabTarget() const {
  return std::holds_alternative<TabSurface>(resolved_target_);
}

tabs::TabInterface& GlicInvokeHandler::GetTab() const {
  CHECK(IsTabTarget());
  tabs::TabInterface* tab = std::get<TabSurface>(resolved_target_).tab;
  CHECK(tab);
  return *tab;
}

GlicInvokeHandler::GlicInvokeHandler(
    GlicInstanceImpl& instance,
    ResolvedTarget resolved_target,
    GlicInvokeOptions options,
    GlicInvokeWithAutoSubmitOptions auto_submit_options,
    std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey,
    std::unique_ptr<GlicInvokeMetrics> invoke_metrics,
    CompletionCallback completion_callback)
    : instance_(instance),
      resolved_target_(std::move(resolved_target)),
      options_(std::move(options)),
      auto_submit_passkey_(auto_submit_passkey),
      auto_submit_options_(std::move(auto_submit_options)),
      completion_callback_(std::move(completion_callback)),
      requires_client_invoke_(
          RequiresClientInvoke(options_, auto_submit_passkey.has_value())),
      metrics_(std::move(invoke_metrics)) {
  if (const auto* tab_surface = std::get_if<TabSurface>(&resolved_target_)) {
    CHECK(tab_surface->tab);

    if (tab_surface->is_new && tab_surface->tab->GetContents() &&
        tab_surface->tab->GetContents()
            ->HasUncommittedNavigationInPrimaryMainFrame()) {
      // NOTE: This simple check won't do the right thing for chained
      // navigations or potentially redirects, as the first navigation will
      // finish and we will proceed, but then another navigation will start.
      should_wait_for_load_ = true;
    }

    // As the handler holds a raw pointer to TabInterface (via
    // resolved_target_), it must listen to its destruction.
    tab_destruction_subscription_ = tab_surface->tab->RegisterWillDetach(
        base::BindRepeating(&GlicInvokeHandler::OnTabWillDetach,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

GlicInvokeHandler::~GlicInvokeHandler() = default;

// static
bool GlicInvokeHandler::RequiresClientInvoke(const GlicInvokeOptions& options,
                                             bool has_auto_submit_passkey) {
  // Note: these conditions mirror the options populated by
  // CreateMojoOptions(). `actuation_tab_id` is intentionally not checked, as
  // it is only populated when `actuation_target` is `kTargetSurface`, which is
  // already covered below.
  const auto* payload =
      std::get_if<mojom::InvocationPayloadPtr>(&options.source_or_payload);
  const mojom::ActuationTarget actuation_target =
      GetMojoActuationTarget(options.target);
  return options.GetInvocationSource() ==
             mojom::InvocationSource::kCaptureRegionHotkey ||
         has_auto_submit_passkey || (payload && !payload->is_null()) ||
         !options.prompts.empty() ||
         (options.additional_context.has_value() &&
          !options.additional_context->context.is_null()) ||
         options.feature_mode.value_or(mojom::FeatureMode::kUnspecified) !=
             mojom::FeatureMode::kUnspecified ||
         (actuation_target != mojom::ActuationTarget::kUnknown &&
          actuation_target != mojom::ActuationTarget::kAgentDecides) ||
         options.disable_zss || options.skill_id.has_value() ||
         options.zss_config.has_value();
}

void GlicInvokeHandler::Invoke() {
  // Ensure the invocation source is set so that any following events are
  // properly logged to the corresponding source.
  instance_->instance_metrics().MaybeSetInitialInvocationSource(
      options_.GetInvocationSource());

  timeout_timer_.Start(FROM_HERE, options_.timeout.value_or(kDefaultTimeout),
                       base::BindOnce(&GlicInvokeHandler::OnTimeout,
                                      weak_ptr_factory_.GetWeakPtr()));

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

  std::vector<std::unique_ptr<GlicInvokeTask>> tasks;

  // A clipboard policy task checks a single payload, so only one of prompts and
  // additional context can request a check.
  const bool context_policy_check =
      options_.RequiresContextClipboardPolicyCheck();
  const bool prompt_policy_check =
      options_.RequiresPromptClipboardPolicyCheck();
  CHECK(!(context_policy_check && prompt_policy_check),
        base::NotFatalUntil::M163);
  const bool needs_clipboard_policy_check =
      context_policy_check || prompt_policy_check;

  // The copy check task must come first so that it is started immediately. This
  // ensures that data needed for the check is cached immediately, removing the
  // need to observe navigations that may disrupt the check.
  if (needs_clipboard_policy_check) {
    tasks.push_back(std::make_unique<CopyPolicyTask>(
        &*instance_, options_,
        base::BindOnce(&GlicInvokeHandler::OnError,
                       weak_ptr_factory_.GetWeakPtr())));
  }

  // Mark the invoked tab as pending only when actuation is not explicitly
  // directed to a different surface.
  if (IsActuatingFeatureMode() && IsTabTarget() &&
      !GetActuationSurface().differs_from_invoked_tab) {
    tasks.push_back(std::make_unique<SetTabPendingActuationTask>(
        instance_->profile(), GetTab().GetHandle()));
  }

  if (should_wait_for_load_ && IsTabTarget()) {
    tasks.push_back(
        std::make_unique<WaitForNavigationTask>(GetTab().GetContents()));
  }

  ShowOptions show_options = CreateShowOptions(resolved_target_, options_);

  // If the side panel is already active, suppress the opening animation. This
  // avoids a visual flicker when swapping from one conversation instance to
  // another (e.g. starting a New Conversation when the panel is already open).
  if (IsTabTarget()) {
    if (GlicSidePanelCoordinator::IsGlicSidePanelActive(&GetTab())) {
      if (auto* panel_options = std::get_if<SidePanelShowOptions>(
              &show_options.embedder_options)) {
        panel_options->suppress_opening_animation = true;
      }
    }
  }

  if (options_.fre_override != mojom::FreOverride::kUnspecified) {
    show_options.fre_override = options_.fre_override;
  }

  if (!options_.tab_sharing.tabs_to_pin.empty()) {
    CHECK(options_.tab_sharing.pin_trigger != GlicPinTrigger::kUnknown);
    // When `tabs_to_pin` includes the target tab, bind it before `PinTabs`
    // (after capturing `show_options` above) so `OnTabPinningStatusEvent` sees
    // it bound to `instance_` and skips creating an inactive side-panel
    // embedder for the foreground target tab.
    if (IsTabTarget() && std::ranges::contains(options_.tab_sharing.tabs_to_pin,
                                               GetTab().GetHandle())) {
      instance_->BindTabWithoutShowing(&GetTab(),
                                       options_.tab_sharing.pin_trigger,
                                       /*pin_on_bind=*/false);
    }
    instance_->GetSharingManagerInternal().PinTabs(
        options_.tab_sharing.tabs_to_pin, options_.tab_sharing.pin_trigger);
  }

  const bool should_show_panel =
      !auto_submit_passkey_.has_value() || auto_submit_options_.show_panel;
  const bool wait_for_panel_stabilization =
      should_show_panel && options_.wait_for_panel_open && IsTabTarget();
  auto maybe_enqueue_on_panel_opened = [&]() {
    if (!options_.on_panel_opened) {
      return;
    }
    tasks.push_back(std::make_unique<PostCallbackTask>(
        base::BindOnce(
            [](base::WeakPtr<GlicInstanceImpl> instance, base::OnceClosure cb) {
              if (instance) {
                std::move(cb).Run();
              }
            },
            instance_->GetWeakPtr(), std::move(options_.on_panel_opened)),
        base::BindOnce(
            [](base::WeakPtr<GlicInvokeHandler> self) {
              return self && self->WasPanelOpened();
            },
            weak_ptr_factory_.GetWeakPtr())));
  };

  if (should_show_panel) {
    tasks.push_back(std::make_unique<ShowInstanceTask>(
        *instance_, show_options,
        base::BindOnce(&GlicInvokeHandler::OnShowInstanceCompleted,
                       weak_ptr_factory_.GetWeakPtr())));
    if (!wait_for_panel_stabilization) {
      maybe_enqueue_on_panel_opened();
    }
  } else if (IsTabTarget()) {
    tasks.push_back(std::make_unique<SetupHiddenPanelTask>(
        *instance_, GetTab().GetHandle()));
  }
  tasks.push_back(std::make_unique<MaybeInitializeHiddenClientTask>(
      &*instance_, options_.GetInvocationSource(), options_.fre_override));
  // Waiting for the client to be ready subsumes waiting for it to connect:
  // `client_load_state()` is `kReady` exactly while the client is connected and
  // responsive. Unlike a bare connection wait, this fails the invocation if the
  // client never loads instead of hanging until the invocation times out.
  //
  // A client stopped on the sign-in panel is the exception: an invocation
  // that only shows the panel is done at that point, while one that needs the
  // client waits for the user to sign in while the panel is visible (see
  // SignInBehavior and OnTimeout()).
  tasks.push_back(std::make_unique<WaitForClientReadyTask>(
      instance_->host(),
      requires_client_invoke_ ? SignInBehavior::kWaits
                              : SignInBehavior::kCompletes,
      base::BindOnce(&GlicInvokeHandler::OnError,
                     weak_ptr_factory_.GetWeakPtr())));
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

  if (wait_for_panel_stabilization) {
    tasks.push_back(
        std::make_unique<StabilizationTask>(GetTab().GetContents()));
    maybe_enqueue_on_panel_opened();
  }

  if (needs_clipboard_policy_check) {
    tasks.push_back(std::make_unique<PastePolicyTask>(
        &*instance_, options_,
        base::BindOnce(&GlicInvokeHandler::OnError,
                       weak_ptr_factory_.GetWeakPtr())));
  }

  mojom::InvokeOptionsPtr mojo_options = CreateMojoOptions();

  if (NeedsFreCompletion()) {
    tasks.push_back(
        std::make_unique<WaitForFreCompletionTask>(instance_->profile()));
  }

  if (requires_client_invoke_) {
    tasks.push_back(std::make_unique<SendToClientTask>(
        &*instance_, std::move(mojo_options), auto_submit_passkey_));
  }

  if (IsActuatingFeatureMode()) {
    if (auto* task_manager = instance_->GetActorTaskManager()) {
      if (task_manager->IsActuating()) {
        OnActuatingChanged(true);
      } else {
        actuating_subscription_ = task_manager->AddActuatingChangedCallback(
            base::BindRepeating(&GlicInvokeHandler::OnActuatingChanged,
                                weak_ptr_factory_.GetWeakPtr()));
      }
    }

    tasks.push_back(std::make_unique<WaitForActuationTask>(
        &*instance_, options_.timeout.value_or(kDefaultTimeout),
        base::BindOnce(&GlicInvokeHandler::OnError,
                       weak_ptr_factory_.GetWeakPtr())));
  }

  main_task_ = std::make_unique<SequentialTaskGroup>(
      std::move(tasks),
      base::BindRepeating(
          [](base::WeakPtr<GlicInvokeHandler> handler,
             std::optional<GlicTaskType> task_type, base::TimeDelta duration) {
            if (handler && handler->metrics_) {
              handler->metrics_->RecordTaskPhaseCompleted(task_type, duration);
            }
          },
          weak_ptr_factory_.GetWeakPtr()));

  // The feature mode and embedder type were set on `metrics_` by the
  // coordinator while resolving this invocation. The client load state is
  // captured here, before any task has run, so it reflects what the
  // invocation inherited rather than what it caused.
  client_load_state_at_start_ = instance_->host().client_load_state();
  metrics_->SetClientLoadStateAtStart(*client_load_state_at_start_);
  metrics_->RecordStarted();
  instance_->instance_metrics().SetActiveInvocationId(
      metrics_->GetInvocationId());
  main_task_->Start(base::BindOnce(&GlicInvokeHandler::OnSuccess,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void GlicInvokeHandler::OnActuatingChanged(bool actuating) {
  if (actuating) {
    actuating_subscription_ = {};
    tab_destruction_subscription_ = {};
    if (auto* tab_surface = std::get_if<TabSurface>(&resolved_target_)) {
      tab_surface->tab = nullptr;
    }
    timeout_timer_.Stop();
  }
}

void GlicInvokeHandler::Cancel(GlicInvokeError error) {
  OnError(error);
}

void GlicInvokeHandler::CancelForInstanceRemoval(InstanceRemovalReason reason) {
  OnError(ToInvokeError(reason));
}

void GlicInvokeHandler::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    OnError(GlicInvokeError::kTabClosed);
  }
}

void GlicInvokeHandler::OnConversationInfoChanged(
    const mojom::ConversationInfo& info) {
  if (auto_submit_options_.on_conversation_id_ready) {
    std::move(auto_submit_options_.on_conversation_id_ready)
        .Run(info.conversation_id);
  }
  conversation_subscription_ = {};
}

void GlicInvokeHandler::OnShowInstanceCompleted(bool panel_opened) {
  panel_opened_by_show_ = panel_opened;
}

bool GlicInvokeHandler::WasPanelOpened() const {
  return panel_opened_by_show_ || instance_->IsShowing();
}

void GlicInvokeHandler::OnSuccess() {
  timeout_timer_.Stop();
  if (main_task_) {
    main_task_->NotifySequenceCompleted(/*success=*/true);
  }

  metrics_->RecordSuccess(GetLastActiveTaskType());
  instance_->instance_metrics().SetActiveInvocationId(std::nullopt);

  if (options_.on_success) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(options_.on_success));
  }
  if (completion_callback_) {
    std::move(completion_callback_).Run(&*instance_, this);
  }
}

void GlicInvokeHandler::OnTimeout() {
  // An invocation that needed the client and spent its time on a visible
  // sign-in panel (SignInBehavior::kWaits) did not run out of time for anything
  // the product did; the user did not sign in. Report that rather than a
  // timeout.
  const Host& host = instance_->host();
  OnError(instance_->IsShowing() &&
                  host.client_load_state() == ClientLoadState::kError &&
                  host.client_load_error_reason() ==
                      ClientLoadErrorReason::kSignIn
              ? GlicInvokeError::kSignInRequired
              : GlicInvokeError::kTimeout);
}

void GlicInvokeHandler::OnError(GlicInvokeError error) {
  timeout_timer_.Stop();
  instance_->SuppressShowOnNextTabAddedToTask(false);
  if (main_task_) {
    main_task_->NotifySequenceCompleted(/*success=*/false);
  }

  const std::optional<GlicTaskType> last_active_task = GetLastActiveTaskType();
  metrics_->RecordError(error, last_active_task);
  instance_->instance_metrics().SetActiveInvocationId(std::nullopt);

  if (options_.on_error) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(options_.on_error), error));
  }
  if (options_.on_error_with_diagnostics_) {
    const std::optional<ClientLoadErrorReason> client_load_error_reason =
        instance_->host().client_load_state() == ClientLoadState::kError
            ? instance_->host().client_load_error_reason()
            : std::nullopt;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(options_.on_error_with_diagnostics_), error,
                       FormatInvokeDiagnosticInfo(last_active_task,
                                                  client_load_state_at_start_,
                                                  client_load_error_reason)));
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
    case mojom::FeatureMode::kPasswordChange:
      return true;
    default:
      return false;
  }
}

GlicInvokeHandler::ActuationSurface GlicInvokeHandler::GetActuationSurface()
    const {
  const auto* handle =
      std::get_if<actor::ActorSurfaceHandle>(&options_.target.actuation_target);
  if (!handle || handle->is_null()) {
    return {};
  }
  // A headless surface has no tab, so GetTabHandle() returns
  // TabHandle::Null(), which never equals GetTab().GetHandle().
  const bool is_invoked_tab_surface =
      IsTabTarget() && handle->GetTabHandle() == GetTab().GetHandle();
  return {.handle = *handle,
          .differs_from_invoked_tab = !is_invoked_tab_surface};
}

mojom::InvokeOptionsPtr GlicInvokeHandler::CreateMojoOptions() {
  auto mojo_options = mojom::InvokeOptions::New();
  mojo_options->invocation_source = options_.GetInvocationSource();

  if (auto* payload = std::get_if<glic::mojom::InvocationPayloadPtr>(
          &options_.source_or_payload)) {
    mojo_options->payload = (*payload).Clone();
  }

  if (!options_.prompts.empty()) {
    mojo_options->prompts = options_.prompts;
  }

  if (options_.additional_context && options_.additional_context->context) {
    mojo_options->context = std::move(options_.additional_context->context);
  }

  mojo_options->auto_submit = auto_submit_passkey_.has_value();
  mojo_options->feature_mode =
      options_.feature_mode.value_or(mojom::FeatureMode::kUnspecified);
  mojo_options->disable_zero_state_suggestions = options_.disable_zss;

  std::visit(absl::Overload{
                 [&](mojom::ActuationTarget target) {
                   mojo_options->actuation_target = target;
                   if (target == mojom::ActuationTarget::kTargetSurface &&
                       IsTabTarget()) {
                     const actor::ActorSurfaceHandle surface =
                         actor::ActorSurfaceHandle::From(GetTab().GetHandle());
                     if (!surface.is_null()) {
                       mojo_options->actuation_tab_id = surface.raw_value();
                     }
                   }
                 },
                 [&](const actor::ActorSurfaceHandle& surface) {
                   CHECK(!surface.is_null());
                   mojo_options->actuation_target =
                       mojom::ActuationTarget::kTargetSurface;
                   mojo_options->actuation_tab_id = surface.raw_value();
                 }},
             options_.target.actuation_target);

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

std::optional<GlicTaskType> GlicInvokeHandler::GetLastActiveTaskType() const {
  if (!main_task_) {
    return std::nullopt;
  }
  return main_task_->GetLastActiveTaskType();
}

bool GlicInvokeHandler::NeedsFreCompletion() const {
  if (GlicEnabling::HasConsentedForProfile(instance_->profile())) {
    return false;
  }
  // kNativeFre waits like kUnspecified: the native FRE card blocks the panel
  // until the user accepts, so invocations that send a client invoke must not
  // reach the web client before consent.
  if (options_.fre_override != mojom::FreOverride::kTrustFirstClick &&
      options_.fre_override != mojom::FreOverride::kUnspecified &&
      options_.fre_override != mojom::FreOverride::kNativeFre) {
    return false;
  }
  return options_.fre_completion_wait_mode == FreCompletionWaitMode::kAlways ||
         (options_.fre_completion_wait_mode ==
              FreCompletionWaitMode::kDefault &&
          requires_client_invoke_);
}

}  // namespace glic
