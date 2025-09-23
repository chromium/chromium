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
#include "base/notimplemented.h"
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
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_occlusion_notifier.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_controller_impl.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
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

bool UseDefaultWindowController() {
  return !base::FeatureList::IsEnabled(features::kGlicMultiInstance);
}

std::unique_ptr<GlicWindowController> CreateWindowController(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    GlicKeyedService* glic_service,
    GlicEnabling* glic_enabling) {
  if (UseDefaultWindowController()) {
    return std::make_unique<GlicWindowControllerImpl>(
        profile, identity_manager, glic_service, glic_enabling);
  }
  return std::make_unique<GlicInstanceCoordinatorImpl>(
      profile, identity_manager, glic_service, glic_enabling);
}

}  // namespace

GlicKeyedService::GlicKeyedService(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    ProfileManager* profile_manager,
    GlicProfileManager* glic_profile_manager,
    contextual_cueing::ContextualCueingService* contextual_cueing_service,
    actor::ActorKeyedService* actor_keyed_service)
    : profile_(profile),
      enabling_(std::make_unique<GlicEnabling>(
          profile,
          &profile_manager->GetProfileAttributesStorage())),
      metrics_(std::make_unique<GlicMetrics>(profile, enabling_.get())),
      fre_controller_(
          std::make_unique<GlicFreController>(profile, identity_manager)),
      window_controller_(CreateWindowController(profile,
                                                identity_manager,
                                                this,
                                                enabling_.get())),
      sharing_manager_(
          std::make_unique<GlicSharingManagerImpl>(profile,
                                                   &window_controller(),
                                                   metrics_.get())),
      screenshot_capturer_(std::make_unique<GlicScreenshotCapturer>()),
      auth_controller_(std::make_unique<AuthController>(profile,
                                                        identity_manager,
                                                        /*use_for_fre=*/false)),
      occlusion_notifier_(
          std::make_unique<GlicOcclusionNotifier>(window_controller())),
      zero_state_suggestions_manager_(
          std::make_unique<GlicZeroStateSuggestionsManager>(
              sharing_manager_.get(),
              &window_controller(),
              contextual_cueing_service)),
      contextual_cueing_service_(contextual_cueing_service),
      actor_keyed_service_(actor_keyed_service) {
  CHECK(GlicEnabling::IsProfileEligible(Profile::FromBrowserContext(profile)));
  CHECK(actor_keyed_service_);
  metrics_->SetControllers(&window_controller(), sharing_manager_.get());

  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::MemoryPressureListenerTag::kGlicKeyedService,
      base::BindRepeating(&GlicKeyedService::OnMemoryPressure,
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

  // This is only used by automation for tests.
  glic_profile_manager->MaybeAutoOpenGlicPanel();
}

GlicKeyedService::~GlicKeyedService() {
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

  // Show the FRE if not yet completed, and if we have a browser to use.
  if (fre_controller_->ShouldShowFreDialog()) {
    Browser* browser = bwi ? bwi->GetBrowserForMigrationOnly() : nullptr;
    if (!fre_controller_->CanShowFreDialog(browser)) {
      // If the FRE is blocked because it is already showing, we should instead
      // dismiss it. This allows the glic button to be used to toggle the
      // presence of the FRE.
      fre_controller_->DismissFreIfOpenOnActiveTab(browser);
      return;
    }
    fre_controller_->ShowFreDialog(browser, source);
    return;
  }

  window_controller().Toggle(bwi, prevent_close, source);
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
  fre_controller().OpenFreDialogInNewTab(bwi, source);
}

void GlicKeyedService::CloseUI() {
  window_controller().Shutdown();
  host_manager().Shutdown();
  fre_controller_->Shutdown();
}

void GlicKeyedService::PrepareForOpen() {
  fre_controller().MaybePreconnect();

  auto* active_web_contents =
      sharing_manager_->GetFocusedTabData().focus()
          ? sharing_manager_->GetFocusedTabData().focus()->GetContents()
          : nullptr;
  if (contextual_cueing_service_ && active_web_contents) {
    contextual_cueing_service_
        ->PrepareToFetchContextualGlicZeroStateSuggestions(active_web_contents);
  }
}

GlicWindowController& GlicKeyedService::window_controller() const {
  CHECK(window_controller_);
  return *window_controller_.get();
}

GlicFreController& GlicKeyedService::fre_controller() {
  CHECK(fre_controller_);
  return *fre_controller_.get();
}

GlicSharingManager& GlicKeyedService::sharing_manager() {
  return *sharing_manager_.get();
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

void GlicKeyedService::RegisterConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::RegisterConversationCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(mojom::RegisterConversationErrorReason::kUnknown);
}

void GlicKeyedService::GetZeroStateSuggestionsAndSubscribe(
    bool has_active_subscription,
    const mojom::ZeroStateSuggestionsOptions& options,
    mojom::WebClientHandler::GetZeroStateSuggestionsAndSubscribeCallback
        callback) {
  zero_state_suggestions_manager().ObserveZeroStateSuggestions(
      has_active_subscription, options.is_first_run, options.supported_tools,
      std::move(callback));
}

void GlicKeyedService::GuestAdded(content::WebContents* guest_contents) {
  host_manager().GuestAdded(guest_contents);
}

bool GlicKeyedService::IsWindowShowing() const {
  return window_controller().IsShowing();
}

bool GlicKeyedService::IsWindowDetached() const {
  return window_controller().IsDetached();
}

bool GlicKeyedService::IsWindowOrFreShowing() const {
  return window_controller().IsShowing() || fre_controller_->IsShowingDialog();
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
  window_controller().Close();
  screenshot_capturer_->CloseScreenPicker();
}

void GlicKeyedService::SetContextAccessIndicator(bool show) {
  if (is_context_access_indicator_enabled_ == show) {
    return;
  }
  is_context_access_indicator_enabled_ = show;
  context_access_indicator_callback_list_.Notify(show);
}

void GlicKeyedService::CreateTask(
    actor::webui::mojom::TaskOptionsPtr options,
    mojom::WebClientHandler::CreateTaskCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
    std::move(callback).Run(
        base::unexpected(mojom::CreateTaskErrorReason::kTaskSystemUnavailable));
    return;
  }
  actor::TaskId task_id = actor_keyed_service_->CreateTask(std::move(options));
  std::move(callback).Run(task_id.value());
}

void GlicKeyedService::PerformActionsFinished(
    mojom::WebClientHandler::PerformActionsCallback callback,
    actor::TaskId task_id,
    base::TimeTicks start_time,
    actor::mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action,
    std::vector<actor::ActionResultWithLatencyInfo> action_results) {
  actor::ActorTask* task = actor_keyed_service_->GetTask(task_id);

  // Task is checked when calling PerformActions and it doesn't go away.
  CHECK(task);

  // The callback doesn't need any weak semantics since all it does is wrap the
  // result and pass it to the mojo callback. If `this` is destroyed the mojo
  // connection is closed so this will be a no-op but the callback doesn't touch
  // any freed memory.
  auto result_callback = base::BindOnce(
      [](mojom::WebClientHandler::PerformActionsCallback callback,
         std::unique_ptr<optimization_guide::proto::ActionsResult> result,
         std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>
             journal_entry) {
        CHECK(result);
        std::move(callback).Run(mojo_base::ProtoWrapper(*result));
      },
      std::move(callback));

  actor::BuildActionsResultWithObservations(
      *profile_, start_time, result_code, index_of_failed_action,
      std::move(action_results), *task, std::move(result_callback));
}

void GlicKeyedService::PerformActions(
    const std::vector<uint8_t>& actions_proto,
    mojom::WebClientHandler::PerformActionsCallback callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  // TODO(bokan): Refactor the actor code in this class into an actor-specific
  // wrapper for proto-to-actor conversion.
  optimization_guide::proto::Actions actions;
  if (!actions.ParseFromArray(actions_proto.data(), actions_proto.size())) {
    std::move(callback).Run(
        base::unexpected(mojom::PerformActionsErrorReason::kInvalidProto));
    return;
  }

  actor_keyed_service_->GetJournal().Log(
      GURL(), actor::TaskId(actions.task_id()),
      actor::mojom::JournalTrack::kActor, "GlicPerformActions",
      actor::JournalDetailsBuilder()
          .Add("proto", actor::ToBase64(actions))
          .Build());

  if (!actions.has_task_id()) {
    std::move(callback).Run(
        base::unexpected(mojom::PerformActionsErrorReason::kMissingTaskId));
    return;
  }

  actor::TaskId task_id(actions.task_id());
  if (!actor_keyed_service_->GetTask(task_id)) {
    actor_keyed_service_->GetJournal().Log(GURL::EmptyGURL(), task_id,
                                           actor::mojom::JournalTrack::kActor,
                                           "Act Failed",
                                           actor::JournalDetailsBuilder()
                                               .AddError("No such task")
                                               .Add("id", task_id.value())
                                               .Build());

    optimization_guide::proto::ActionsResult response =
        actor::BuildErrorActionsResult(
            actor::mojom::ActionResultCode::kTaskWentAway, std::nullopt);
    std::move(callback).Run(mojo_base::ProtoWrapper(response));
    return;
  }

  actor::BuildToolRequestResult requests = actor::BuildToolRequest(actions);
  if (!requests.has_value()) {
    actor_keyed_service_->GetJournal().Log(
        GURL::EmptyGURL(), task_id, actor::mojom::JournalTrack::kActor,
        "Act Failed",
        actor::JournalDetailsBuilder()
            .AddError("Failed to convert proto::Actions to ToolRequest")
            .Add("failed_action_index", requests.error())
            .Build());
    optimization_guide::proto::ActionsResult response =
        actor::BuildErrorActionsResult(
            actor::mojom::ActionResultCode::kArgumentsInvalid,
            requests.error());
    std::move(callback).Run(mojo_base::ProtoWrapper(response));
    return;
  }

  actor_keyed_service_->PerformActions(
      task_id, std::move(requests.value()),
      base::BindOnce(&GlicKeyedService::PerformActionsFinished, GetWeakPtr(),
                     std::move(callback), task_id, start_time));
}

void GlicKeyedService::StopActorTask(actor::TaskId task_id,
                                     mojom::ActorTaskStopReason stop_reason) {
  actor::ActorTask* task = actor_keyed_service_->GetTask(task_id);
  if (!task || task->IsStopped()) {
    actor_keyed_service_->GetJournal().Log(
        GURL::EmptyGURL(), task_id, actor::mojom::JournalTrack::kActor,
        "Failed to stop task",
        actor::JournalDetailsBuilder()
            .AddError(task ? "Task already stopped" : "No such task")
            .Add("id", task_id.value())
            .Build());
    return;
  }

  bool success = false;
  switch (stop_reason) {
    case mojom::ActorTaskStopReason::kTaskComplete:
      success = true;
      break;
    case mojom::ActorTaskStopReason::kStoppedByUser:
      success = false;
      break;
  }

  actor_keyed_service_->StopTask(task->id(), success);
}

void GlicKeyedService::PauseActorTask(
    actor::TaskId task_id,
    mojom::ActorTaskPauseReason pause_reason) {
  actor::ActorTask* task = actor_keyed_service_->GetTask(task_id);
  if (!task || task->IsStopped() || task->IsPaused()) {
    actor_keyed_service_->GetJournal().Log(
        GURL::EmptyGURL(), task_id, actor::mojom::JournalTrack::kActor,
        "Failed to pause task",
        actor::JournalDetailsBuilder()
            .AddError(task ? "Task is not running" : "No such task")
            .Add("id", task_id.value())
            .Build());
    return;
  }

  bool from_actor = false;
  switch (pause_reason) {
    case mojom::ActorTaskPauseReason::kPausedByModel:
      from_actor = true;
      break;
    case mojom::ActorTaskPauseReason::kPausedByUser:
      from_actor = false;
      break;
  }

  task->Pause(from_actor);
}

void GlicKeyedService::ResumeActorTask(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& context_options,
    glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) {
  actor::ActorTask* task = actor_keyed_service_->GetTask(task_id);
  if (!task || !task->IsPaused()) {
    std::string error_message = task ? "Task is not paused" : "No such task";
    actor_keyed_service_->GetJournal().Log(GURL::EmptyGURL(), task_id,
                                           actor::mojom::JournalTrack::kActor,
                                           "Failed to resume task",
                                           actor::JournalDetailsBuilder()
                                               .AddError(error_message)
                                               .Add("id", task_id.value())
                                               .Build());
    std::move(callback).Run(
        mojom::GetContextResult::NewErrorReason(error_message));
    return;
  }

  task->Resume();

  // TODO(crbug.com/420669167): GetLastActedTabs should only ever have 1 tab in
  // it for now but once we support multi-tab we'll need to grab observations
  // for all relevant tabs.
  DCHECK_GT(task->GetLastActedTabs().size(), 0ul);
  DCHECK_LT(task->GetLastActedTabs().size(), 2ul);
  tabs::TabInterface* tab_of_resumed_task = nullptr;
  for (tabs::TabHandle tab_handle : task->GetLastActedTabs()) {
    if (tabs::TabInterface* tab = tab_handle.Get()) {
      tab_of_resumed_task = tab;
      break;
    }
  }
  if (!tab_of_resumed_task) {
    std::string error_message = "No tab for observation";
    actor_keyed_service_->GetJournal().Log(GURL::EmptyGURL(), task_id,
                                           actor::mojom::JournalTrack::kActor,
                                           "Failed to resume task",
                                           actor::JournalDetailsBuilder()
                                               .AddError(error_message)
                                               .Add("id", task_id.value())
                                               .Build());
    std::move(callback).Run(
        glic::mojom::GetContextResult::NewErrorReason(error_message));
    return;
  }

  auto observation_callback = base::BindOnce(
      [](glic::mojom::WebClientHandler::ResumeActorTaskCallback reply_callback,
         glic::mojom::TabDataPtr tab_data,
         actor::ActorKeyedService::TabObservationResult result) {
        if (!result.has_value()) {
          std::move(reply_callback)
              .Run(glic::mojom::GetContextResult::NewErrorReason(
                  result.error()));
          return;
        }

        page_content_annotations::FetchPageContextResult& page_context =
            *result.value();

        // RequestTabObservation guarantees a successful request has both
        // screenshot and APC.
        CHECK(page_context.screenshot_result.has_value());
        CHECK(page_context.annotated_page_content_result.has_value());

        auto glic_tab_context = mojom::TabContext::New();

        glic_tab_context->tab_data = std::move(tab_data);

        glic_tab_context->viewport_screenshot = glic::mojom::Screenshot::New(
            page_context.screenshot_result->dimensions.width(),
            page_context.screenshot_result->dimensions.height(),
            std::move(page_context.screenshot_result->jpeg_data), "image/jpeg",
            // TODO(b/380495633): Finalize and implement image annotations.
            glic::mojom::ImageOriginAnnotations::New());

        glic_tab_context->annotated_page_data = mojom::AnnotatedPageData::New();
        glic_tab_context->annotated_page_data->annotated_page_content =
            mojo_base::ProtoWrapper(
                page_context.annotated_page_content_result->proto);
        glic_tab_context->annotated_page_data->metadata =
            std::move(page_context.annotated_page_content_result->metadata);

        glic::mojom::GetContextResultPtr glic_result =
            glic::mojom::GetContextResult::NewTabContext(
                std::move(glic_tab_context));
        std::move(reply_callback).Run(std::move(glic_result));
      },
      std::move(callback), CreateTabData(tab_of_resumed_task->GetContents()));

  actor_keyed_service_->RequestTabObservation(*tab_of_resumed_task, task_id,
                                              std::move(observation_callback));
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
      window_controller().GetHostNativeWindow(), std::move(callback));
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

void GlicKeyedService::TryPreloadFre(GlicPrewarmingFreSource source) {
  if (base::FeatureList::IsEnabled(features::kGlicDisableWarming) &&
      !base::FeatureList::IsEnabled(features::kGlicFreWarming)) {
    base::UmaHistogramEnumeration(
        "Glic.PrewarmingFre.DisabledShouldNotPreloadFreForSource", source);
    return;
  }
  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  CHECK(glic_profile_manager);

  glic_profile_manager->ShouldPreloadFreForProfile(
      profile_, base::BindOnce(&GlicKeyedService::FinishPreloadFre,
                               GetWeakPtr(), source));
}

void GlicKeyedService::Reload() {
  if (fre_controller_->IsShowingDialog()) {
    if (auto* fre_contents = fre_controller_->GetWebContents()) {
      fre_contents->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                           /*check_for_repost=*/false);
    }
  } else {
    window_controller().Reload();
  }
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
  return host_manager().IsGlicWebUi(contents) ||
         contents == fre_controller().GetWebContents();
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

  window_controller().Preload();
}

void GlicKeyedService::FinishPreloadFre(GlicPrewarmingFreSource source,
                                        bool should_preload) {
  if (!should_preload) {
    base::UmaHistogramEnumeration(
        "Glic.PrewarmingFre.ShouldNotPreloadFreForSource", source);
    return;
  }

  base::UmaHistogramEnumeration("Glic.PrewarmingFre.ShouldPreloadFreForSource",
                                source);

  fre_controller().TryPreload();
}

bool GlicKeyedService::IsProcessHostForGlic(
    content::RenderProcessHost* process_host) {
  auto* fre_contents = fre_controller().GetWebContents();
  if (fre_contents) {
    if (fre_contents->GetPrimaryMainFrame()->GetProcess() == process_host) {
      return true;
    }
  }
  return host_manager().IsGlicWebUiHost(process_host);
}

bool GlicKeyedService::IsGlicWebUi(content::WebContents* web_contents) {
  return host_manager().IsGlicWebUi(web_contents);
}

HostManager& GlicKeyedService::host_manager() {
  if (!UseDefaultWindowController()) {
    // Must be accessed through an instance.
    NOTIMPLEMENTED();
  }
  return window_controller().host_manager();
}

GlicInstance* GlicKeyedService::GetInstanceForActiveTab(
    BrowserWindowInterface* bwi) {
  return window_controller().GetInstanceForTab(
      bwi ? bwi->GetActiveTabInterface() : nullptr);
}

void GlicKeyedService::SendAdditionalContext(
    tabs::TabHandle tab_handle,
    mojom::AdditionalContextPtr context) {
  auto* tab = tab_handle.Get();
  auto* host = &window_controller().GetInstanceForTab(tab)->host();
  host->NotifyAdditionalContext(std::move(context));
}

}  // namespace glic
