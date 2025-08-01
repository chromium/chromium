// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_keyed_service.h"

#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_occlusion_notifier.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_actor_controller.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_controller_impl.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace glic {

namespace {

base::TimeDelta GetWarmingDelay() {
  base::TimeDelta delay_start =
      base::Milliseconds(features::kGlicWarmingDelayMs.Get());
  base::TimeDelta delay_limit =
      delay_start + base::Milliseconds(features::kGlicWarmingJitterMs.Get());
  if (delay_limit > delay_start) {
    return RandTimeDelta(delay_start, delay_limit);
  }
  return delay_start;
}

}  // namespace

GlicKeyedService::GlicKeyedService(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    ProfileManager* profile_manager,
    GlicProfileManager* glic_profile_manager,
    contextual_cueing::ContextualCueingService* contextual_cueing_service)
    : profile_(profile),
      enabling_(std::make_unique<GlicEnabling>(
          profile,
          &profile_manager->GetProfileAttributesStorage())),
      metrics_(std::make_unique<GlicMetrics>(profile, enabling_.get())),
      host_(std::make_unique<Host>(profile)),
      window_controller_(
          std::make_unique<GlicWindowControllerImpl>(profile,
                                                     identity_manager,
                                                     this,
                                                     enabling_.get())),
      sharing_manager_(
          std::make_unique<GlicSharingManagerImpl>(profile,
                                                   window_controller_.get(),
                                                   host_.get(),
                                                   metrics_.get())),
      screenshot_capturer_(std::make_unique<GlicScreenshotCapturer>()),
      auth_controller_(std::make_unique<AuthController>(profile,
                                                        identity_manager,
                                                        /*use_for_fre=*/false)),
      occlusion_notifier_(
          std::make_unique<GlicOcclusionNotifier>(*window_controller_)),
      zero_state_suggestions_manager_(
          std::make_unique<GlicZeroStateSuggestionsManager>(
              sharing_manager_.get(),
              window_controller_.get(),
              contextual_cueing_service,
              host_.get())),
      contextual_cueing_service_(contextual_cueing_service) {
  CHECK(GlicEnabling::IsProfileEligible(Profile::FromBrowserContext(profile)));
  host_->Initialize(window_controller_.get());
  metrics_->SetControllers(window_controller_.get(), sharing_manager_.get());

  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&GlicKeyedService::OnMemoryPressure,
                                     weak_ptr_factory_.GetWeakPtr()));

  // If `--glic-always-open-fre` is present, unset this pref to ensure the FRE
  // is shown for testing convenience.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kGlicAlwaysOpenFre)) {
    profile_->GetPrefs()->SetInteger(
        prefs::kGlicCompletedFre,
        static_cast<int>(prefs::FreStatus::kNotStarted));
    // or if automation is enabled, skip FRE
  } else if (command_line->HasSwitch(::switches::kGlicAutomation)) {
    profile_->GetPrefs()->SetInteger(
        prefs::kGlicCompletedFre,
        static_cast<int>(prefs::FreStatus::kCompleted));
  }

  if (base::FeatureList::IsEnabled(features::kGlicActor)) {
    actor_controller_ = std::make_unique<GlicActorController>(profile_);
  }

  // This is only used by automation for tests.
  glic_profile_manager->MaybeAutoOpenGlicPanel();
}

GlicKeyedService::~GlicKeyedService() {
  host().Destroy();
  metrics_->SetControllers(nullptr, nullptr);
}

// static
GlicKeyedService* GlicKeyedService::Get(content::BrowserContext* context) {
  return GlicKeyedServiceFactory::GetGlicKeyedService(context);
}

void GlicKeyedService::Shutdown() {
  CloseUI();
  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  if (glic_profile_manager) {
    glic_profile_manager->OnServiceShutdown(this);
  }
}

void GlicKeyedService::ToggleUI(BrowserWindowInterface* bwi,
                                bool prevent_close,
                                mojom::InvocationSource source) {
  // Glic may be disabled for certain user profiles (the user is browsing in
  // incognito or guest mode, policy, etc). In those cases, the entry points to
  // this method should already have been removed.
  CHECK(GlicEnabling::IsEnabledForProfile(profile_));

  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  if (glic_profile_manager) {
    glic_profile_manager->SetActiveGlic(this);
  }
  window_controller_->Toggle(bwi, prevent_close, source);
}

void GlicKeyedService::OpenFreDialogInNewTab(BrowserWindowInterface* bwi,
                                             mojom::InvocationSource source) {
  // Glic may be disabled for certain user profiles (the user is browsing in
  // incognito or guest mode, policy, etc). In those cases, the entry points to
  // this method should already have been removed.
  CHECK(GlicEnabling::IsEnabledForProfile(profile_));

  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  if (glic_profile_manager) {
    glic_profile_manager->SetActiveGlic(this);
  }
  window_controller_->fre_controller()->OpenFreDialogInNewTab(bwi, source);
}

void GlicKeyedService::CloseUI() {
  window_controller_->Shutdown();
  host().Shutdown();
  SetContextAccessIndicator(false);
}

void GlicKeyedService::PrepareForOpen() {
  window_controller_->fre_controller()->MaybePreconnect();

  auto* active_web_contents =
      sharing_manager_->GetFocusedTabData().focus()
          ? sharing_manager_->GetFocusedTabData().focus()->GetContents()
          : nullptr;
  if (contextual_cueing_service_ && active_web_contents) {
    contextual_cueing_service_
        ->PrepareToFetchContextualGlicZeroStateSuggestions(active_web_contents);
  }
}

void GlicKeyedService::OnZeroStateSuggestionsFetched(
    mojom::ZeroStateSuggestionsPtr suggestions,
    mojom::WebClientHandler::GetZeroStateSuggestionsForFocusedTabCallback
        callback,
    std::vector<std::string> returned_suggestions) {
  std::vector<mojom::SuggestionContentPtr> output_suggestions;
  for (const std::string& suggestion_string : returned_suggestions) {
    output_suggestions.push_back(
        mojom::SuggestionContent::New(suggestion_string));
  }
  suggestions->suggestions = std::move(output_suggestions);

  std::move(callback).Run(std::move(suggestions));
}

void GlicKeyedService::FetchZeroStateSuggestions(
    bool is_first_run,
    std::optional<std::vector<std::string>> supported_tools,
    mojom::WebClientHandler::GetZeroStateSuggestionsForFocusedTabCallback
        callback) {
  auto* active_web_contents =
      sharing_manager_->GetFocusedTabData().focus()
          ? sharing_manager_->GetFocusedTabData().focus()->GetContents()
          : nullptr;

  if (contextual_cueing_service_ && active_web_contents && IsWindowShowing()) {
    auto suggestions = mojom::ZeroStateSuggestions::New();
    suggestions->tab_id = GetTabId(active_web_contents);
    suggestions->tab_url = active_web_contents->GetLastCommittedURL();
    contextual_cueing_service_
        ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
            active_web_contents, is_first_run, supported_tools,
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                base::BindOnce(&GlicKeyedService::OnZeroStateSuggestionsFetched,
                               GetWeakPtr(), std::move(suggestions),
                               std::move(callback)),
                std::vector<std::string>({})));

  } else {
    std::move(callback).Run(nullptr);
  }
}

GlicWindowController& GlicKeyedService::window_controller() {
  CHECK(window_controller_);
  return *window_controller_.get();
}

GlicSharingManager& GlicKeyedService::sharing_manager() {
  return *sharing_manager_.get();
}

void GlicKeyedService::GuestAdded(content::WebContents* guest_contents) {
  host().GuestAdded(guest_contents);
}

bool GlicKeyedService::IsWindowShowing() const {
  return window_controller_->IsShowing();
}

bool GlicKeyedService::IsWindowDetached() const {
  return window_controller_->IsDetached();
}

bool GlicKeyedService::IsWindowOrFreShowing() const {
  return window_controller_->IsShowing() ||
         window_controller_->fre_controller()->IsShowingDialog();
}

base::CallbackListSubscription
GlicKeyedService::AddContextAccessIndicatorStatusChangedCallback(
    ContextAccessIndicatorChangedCallback callback) {
  return context_access_indicator_callback_list_.Add(std::move(callback));
}

void GlicKeyedService::CreateTab(
    const ::GURL& url,
    bool open_in_background,
    const std::optional<int32_t>& window_id,
    glic::mojom::WebClientHandler::CreateTabCallback callback) {
  // If we need to open other URL types, it should be done in a more specific
  // function.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(nullptr);
    return;
  }
  NavigateParams params(profile_, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = open_in_background
                           ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                           : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      Navigate(&params);
  if (!navigation_handle.get()) {
    std::move(callback).Run(nullptr);
    return;
  }
  // Right after requesting the navigation, the WebContents will have almost no
  // information to populate TabData, hence the overriding of the URL. Should we
  // ever want to send more data back to the web client, we should wait until
  // the navigation commits.
  mojom::TabDataPtr tab_data =
      CreateTabData(navigation_handle.get()->GetWebContents());
  if (tab_data) {
    tab_data->url = url;
  }
  std::move(callback).Run(std::move(tab_data));
}

void GlicKeyedService::ClosePanel() {
  window_controller_->Close();
  SetContextAccessIndicator(false);
  screenshot_capturer_->CloseScreenPicker();
}

void GlicKeyedService::AttachPanel() {
  window_controller_->Attach();
}

void GlicKeyedService::DetachPanel() {
  window_controller_->Detach();
}

void GlicKeyedService::ResizePanel(const gfx::Size& size,
                                   base::TimeDelta duration,
                                   base::OnceClosure callback) {
  window_controller_->Resize(size, duration, std::move(callback));
}

void GlicKeyedService::SetPanelDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  window_controller_->SetDraggableAreas(draggable_areas);
}

void GlicKeyedService::SetContextAccessIndicator(bool show) {
  if (is_context_access_indicator_enabled_ == show) {
    return;
  }
  is_context_access_indicator_enabled_ = show;
  context_access_indicator_callback_list_.Notify(show);
}

void GlicKeyedService::CreateTask(
    mojom::WebClientHandler::CreateTaskCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
    std::move(callback).Run(
        base::unexpected(mojom::CreateTaskErrorReason::kTaskSystemUnavailable));
    return;
  }
  actor::TaskId task_id = actor::ActorKeyedService::Get(profile_)->CreateTask();
  std::move(callback).Run(task_id.value());
}

void GlicKeyedService::PerformActionsFinished(
    mojom::WebClientHandler::PerformActionsCallback callback,
    actor::TaskId task_id,
    actor::mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action) {
  actor::ActorTask* task =
      actor::ActorKeyedService::Get(profile_)->GetTask(task_id);

  // Task is checked when calling PerformActions and it doesn't go away.
  CHECK(task);

  // The callback doesn't need any weak semantics since all it does is wrap the
  // result and pass it to the mojo callback. If `this` is destroyed the mojo
  // connection is closed so this will be a no-op but the callback doesn't touch
  // any freed memory.
  auto result_callback = base::BindOnce(
      [](mojom::WebClientHandler::PerformActionsCallback callback,
         std::unique_ptr<optimization_guide::proto::ActionsResult> result) {
        CHECK(result);
        std::move(callback).Run(mojo_base::ProtoWrapper(*result));
      },
      std::move(callback));

  actor::BuildActionsResultWithObservations(*profile_, result_code,
                                            index_of_failed_action, *task,
                                            std::move(result_callback));
}

void GlicKeyedService::PerformActions(
    const std::vector<uint8_t>& actions_proto,
    mojom::WebClientHandler::PerformActionsCallback callback) {
  // TODO(bokan): Refactor the actor code in this class into an actor-specific
  // wrapper for proto-to-actor conversion.
  optimization_guide::proto::Actions actions;
  if (!actions.ParseFromArray(actions_proto.data(), actions_proto.size())) {
    std::move(callback).Run(
        base::unexpected(mojom::PerformActionsErrorReason::kInvalidProto));
    return;
  }

  auto* actor_service = actor::ActorKeyedService::Get(profile_);
  actor_service->GetJournal().Log(
      GURL(), actor::TaskId(actions.task_id()),
      actor::mojom::JournalTrack::kActor, "GlicPerformActions",
      absl::StrFormat("Proto: %s", actor::ToBase64(actions)));

  if (!actions.has_task_id()) {
    std::move(callback).Run(
        base::unexpected(mojom::PerformActionsErrorReason::kMissingTaskId));
    return;
  }

  actor::TaskId task_id(actions.task_id());
  if (!actor_service->GetTask(task_id)) {
    actor_service->GetJournal().Log(
        GURL::EmptyGURL(), task_id, actor::mojom::JournalTrack::kActor,
        "Act Failed", absl::StrFormat("No task with id[%d]", task_id.value()));
    optimization_guide::proto::ActionsResult response =
        actor::BuildErrorActionsResult(
            actor::mojom::ActionResultCode::kTaskWentAway, std::nullopt);
    std::move(callback).Run(mojo_base::ProtoWrapper(response));
    return;
  }

  actor::BuildToolRequestResult requests = actor::BuildToolRequest(actions);
  if (!requests.has_value()) {
    actor_service->GetJournal().Log(
        GURL::EmptyGURL(), task_id, actor::mojom::JournalTrack::kActor,
        "Act Failed",
        absl::StrFormat("Failed to convert proto::Actions[%d] to ToolRequest",
                        requests.error()));
    optimization_guide::proto::ActionsResult response =
        actor::BuildErrorActionsResult(
            actor::mojom::ActionResultCode::kArgumentsInvalid,
            requests.error());
    std::move(callback).Run(mojo_base::ProtoWrapper(response));
    return;
  }

  actor_service->PerformActions(
      task_id, std::move(requests.value()),
      base::BindOnce(&GlicKeyedService::PerformActionsFinished, GetWeakPtr(),
                     std::move(callback), task_id));
}

// TODO(crbug.com/411462297): Stop/Pause/Resume task need to be routed to go
// through the ActorKeyedService, rather than the deprecated ActorController
// which ignores the task_id.
void GlicKeyedService::StopActorTask(actor::TaskId task_id) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  CHECK(actor_controller_);
  actor_controller_->StopTask(task_id);
}

void GlicKeyedService::PauseActorTask(actor::TaskId task_id) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  CHECK(actor_controller_);
  actor_controller_->PauseTask(task_id);
}

void GlicKeyedService::ResumeActorTask(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& context_options,
    glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  CHECK(actor_controller_);
  actor_controller_->ResumeTask(task_id, context_options, std::move(callback));
}

void GlicKeyedService::OnUserInputSubmitted(glic::mojom::WebClientMode mode) {
  user_input_submitted_callback_list_.Notify();
}

base::CallbackListSubscription GlicKeyedService::AddUserInputSubmittedCallback(
    base::RepeatingClosure callback) {
  return user_input_submitted_callback_list_.Add(std::move(callback));
}
void GlicKeyedService::CaptureScreenshot(
    mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  screenshot_capturer_->CaptureScreenshot(
      window_controller_->GetGlicWidget()->GetNativeWindow(),
      std::move(callback));
}

bool GlicKeyedService::IsContextAccessIndicatorShown(
    const content::WebContents* contents) {
  return is_context_access_indicator_enabled_ &&
         sharing_manager_->GetFocusedTabData().focus() &&
         sharing_manager_->GetFocusedTabData().focus()->GetContents() ==
             contents;
}

void GlicKeyedService::AddPreloadCallback(base::OnceCallback<void()> callback) {
  preload_callback_ = std::move(callback);
}

void GlicKeyedService::TryPreload() {
  if (base::FeatureList::IsEnabled(features::kGlicDisableWarming) &&
      !base::FeatureList::IsEnabled(features::kGlicWarming)) {
    // This is to ensure the preload process completes and preload_callback_ is
    // called.
    FinishPreload(GlicPrewarmingChecksResult::kWarmingDisabled);
    return;
  }
  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  CHECK(glic_profile_manager);
  base::TimeDelta delay = GetWarmingDelay();

  // TODO(b/411100559): Ideally we'd use post delayed task in all cases,
  // but this requires a refactor of tests that are currently brittle. For now,
  // just synchronously call ShouldPreloadForProfile if there is no delay.
  if (delay.is_zero()) {
    glic_profile_manager->ShouldPreloadForProfile(
        profile_,
        base::BindOnce(&GlicKeyedService::FinishPreload, GetWeakPtr()));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GlicKeyedService::TryPreloadAfterDelay, GetWeakPtr()),
        delay);
  }
}

void GlicKeyedService::TryPreloadAfterDelay() {
  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  if (glic_profile_manager) {
    glic_profile_manager->ShouldPreloadForProfile(
        profile_,
        base::BindOnce(&GlicKeyedService::FinishPreload, GetWeakPtr()));
  }
}

void GlicKeyedService::TryPreloadFre() {
  if (base::FeatureList::IsEnabled(features::kGlicDisableWarming) &&
      !base::FeatureList::IsEnabled(features::kGlicFreWarming)) {
    return;
  }
  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  CHECK(glic_profile_manager);

  glic_profile_manager->ShouldPreloadFreForProfile(
      profile_,
      base::BindOnce(&GlicKeyedService::FinishPreloadFre, GetWeakPtr()));
}

void GlicKeyedService::Reload() {
  window_controller().Reload();
}

void GlicKeyedService::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level == base::MemoryPressureListener::MemoryPressureLevel::
                   MEMORY_PRESSURE_LEVEL_NONE ||
      (this == GlicProfileManager::GetInstance()->GetLastActiveGlic())) {
    return;
  }

  CloseUI();
}

base::WeakPtr<GlicKeyedService> GlicKeyedService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool GlicKeyedService::IsActiveWebContents(content::WebContents* contents) {
  if (!contents) {
    return false;
  }
  return contents == host().webui_contents() ||
         contents == window_controller().GetFreWebContents();
}

void GlicKeyedService::FinishPreload(GlicPrewarmingChecksResult result) {
  base::UmaHistogramEnumeration("Glic.Prewarming.ChecksResult", result);
  if (preload_callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(preload_callback_)));
  }

  if (result != GlicPrewarmingChecksResult::kSuccess) {
    return;
  }

  window_controller_->Preload();
}

void GlicKeyedService::FinishPreloadFre(bool should_preload) {
  if (!should_preload) {
    return;
  }

  window_controller_->PreloadFre();
}

bool GlicKeyedService::IsProcessHostForGlic(
    content::RenderProcessHost* process_host) {
  auto* fre_contents = window_controller_->GetFreWebContents();
  if (fre_contents) {
    if (fre_contents->GetPrimaryMainFrame()->GetProcess() == process_host) {
      return true;
    }
  }
  return host().IsGlicWebUiHost(process_host);
}

bool GlicKeyedService::IsGlicWebUi(content::WebContents* web_contents) {
  return host().IsGlicWebUi(web_contents);
}

}  // namespace glic
