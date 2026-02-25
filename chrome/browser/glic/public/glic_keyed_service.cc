// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_keyed_service.h"

#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/actor/glic_actor_task_manager.h"
#include "chrome/browser/glic/common/application_hotkey_delegate.h"
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/context/glic_active_instance_sharing_manager.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/context/glic_share_image_handler.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/context/glic_tab_data_observer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_occlusion_notifier.h"
#include "chrome/browser/glic/host/glic_region_capture_controller.h"
#include "chrome/browser/glic/media/glic_media_integration.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_controller_impl.h"

#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/android/glic_keyed_service_android.h"
#endif

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

#if !BUILDFLAG(IS_ANDROID)
bool UseDefaultWindowController() {
  return !GlicEnabling::IsMultiInstanceEnabled();
}
#endif

std::unique_ptr<GlicWindowController> CreateWindowController(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    GlicKeyedService* glic_service,
    GlicEnabling* glic_enabling,
    contextual_cueing::ContextualCueingService* contextual_cueing_service) {
  // Update the eligibility state for future runs of Chrome in case this
  // newly loaded profile was not captured in the initial eligibility check.
  // This will not affect the multi-instance eligibiltiy state of the current
  // run.
  GlicEnabling::GetAndUpdateEligibilityForGlicMultiInstanceTieredRollout(
      profile);

#if !BUILDFLAG(IS_ANDROID)
  if (UseDefaultWindowController()) {
    return std::make_unique<GlicWindowControllerImpl>(
        profile, identity_manager, glic_service, glic_enabling);
  }
#endif
  return std::make_unique<GlicInstanceCoordinatorImpl>(
      profile, identity_manager, glic_service, glic_enabling,
      contextual_cueing_service);
}

std::unique_ptr<GlicSharingManager> CreateSharingManager(
    Profile* profile,
    GlicWindowController* window_controller,
    GlicMetrics* metrics,
    GlicEnabling* glic_enabling) {
#if !BUILDFLAG(IS_ANDROID)
  if (UseDefaultWindowController()) {
    return std::make_unique<GlicSharingManagerImpl>(
        profile, static_cast<GlicWindowControllerImpl*>(window_controller),
        metrics);
  }
#endif

  return std::make_unique<GlicActiveInstanceSharingManager>(
      profile, glic_enabling,
      static_cast<GlicInstanceCoordinatorImpl*>(window_controller));
}

void WriteGuestUrlPresetToPrefs(const char* switch_name,
                                const char* pref_name) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_name)) {
    std::string preset_url =
        GURL(command_line->GetSwitchValueASCII(switch_name)).spec();
    g_browser_process->local_state()->SetString(pref_name, preset_url);
  }
}

void SetupGuestUrlPresetPrefs() {
  WriteGuestUrlPresetToPrefs(::switches::kGlicGuestUrlPresetAutopush,
                             prefs::kGlicGuestUrlPresetAutopush);
  WriteGuestUrlPresetToPrefs(::switches::kGlicGuestUrlPresetStaging,
                             prefs::kGlicGuestUrlPresetStaging);
  WriteGuestUrlPresetToPrefs(::switches::kGlicGuestUrlPresetPreprod,
                             prefs::kGlicGuestUrlPresetPreprod);
  WriteGuestUrlPresetToPrefs(::switches::kGlicGuestUrlPresetProd,
                             prefs::kGlicGuestUrlPresetProd);
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
      actor_policy_checker_(
          actor_keyed_service
              ? std::make_unique<GlicActorPolicyChecker>(*profile_)
              : nullptr),
      enabling_(std::make_unique<GlicEnabling>(
          profile,
          &profile_manager->GetProfileAttributesStorage())),
      metrics_(std::make_unique<GlicMetrics>(profile, enabling_.get())),
      fre_controller_(
          std::make_unique<GlicFreController>(profile, identity_manager)),
      window_controller_(CreateWindowController(profile,
                                                identity_manager,
                                                this,
                                                enabling_.get(),
                                                contextual_cueing_service)),
      sharing_manager_(CreateSharingManager(profile,
                                            &window_controller(),
                                            metrics_.get(),
                                            enabling_.get())),
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL: CaptureRegion
      region_capture_controller_(
          std::make_unique<GlicRegionCaptureController>()),
#endif
      auth_controller_(std::make_unique<AuthController>(profile,
                                                        identity_manager,
                                                        /*use_for_fre=*/false)),

#if !BUILDFLAG(IS_ANDROID)  // Single instance only
      occlusion_notifier_(UseDefaultWindowController()
                              ? std::make_unique<GlicOcclusionNotifier>(
                                    GetSingleInstanceWindowController())
                              : nullptr),
      actor_task_manager_(
          actor_keyed_service
              ? std::make_unique<GlicActorTaskManager>(profile,
                                                       actor_keyed_service,
                                                       *actor_policy_checker_)
              : nullptr),
#endif
      tab_data_observer_(std::make_unique<GlicTabDataObserver>()),
      web_contents_warming_pool_(
          std::make_unique<GlicWebContentsWarmingPool>(profile)),
      contextual_cueing_service_(contextual_cueing_service) {
  CHECK(GlicEnabling::IsProfileEligible(Profile::FromBrowserContext(profile)));

  // TODO(crbug.com/450026474): Consider not constructing this metrics
  // instance for multi-instance
  metrics_->ClearControllers();

#if !BUILDFLAG(IS_ANDROID)  // Single instance only
  if (UseDefaultWindowController()) {
    zero_state_suggestions_manager_ =
        std::make_unique<GlicZeroStateSuggestionsManager>(
            sharing_manager_.get(), &GetSingleInstanceWindowController(),
            contextual_cueing_service);
    metrics_->SetControllers(&GetSingleInstanceWindowController(),
                             sharing_manager_.get());
  }
#endif

  memory_pressure_listener_registration_ =
      std::make_unique<base::MemoryPressureListenerRegistration>(
          FROM_HERE, base::MemoryPressureListenerTag::kGlicKeyedService, this);
  if (base::FeatureList::IsEnabled(features::kGlicShareImage)) {
    share_image_handler_ = std::make_unique<GlicShareImageHandler>(*this);
  }

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

  // Sets up prefs storing manually configured glic guest URLs. Intended for
  // manual testing only.
  SetupGuestUrlPresetPrefs();

  // This is only used by automation for tests.
  glic_profile_manager->MaybeAutoOpenGlicPanel();

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(media::kHeadlessCaptionEarlyStart)) {
    GlicMediaIntegration::GetFor(profile_);
  }
#endif
}

GlicKeyedService::~GlicKeyedService() {
  metrics_->SetControllers(nullptr, nullptr);
}

#if !BUILDFLAG(IS_ANDROID)
GlicRegionCaptureController& GlicKeyedService::region_capture_controller() {
  return *region_capture_controller_;
}
#endif

// static
GlicKeyedService* GlicKeyedService::Get(content::BrowserContext* context) {
  return GlicKeyedServiceFactory::GetGlicKeyedService(context);
}

void GlicKeyedService::Shutdown() {
  if (GlicEnabling::IsMultiInstanceEnabled()) {
    window_controller().Shutdown();
    fre_controller_->Shutdown();
  } else {
    CloseAndShutdown();
  }
  web_contents_warming_pool_->Shutdown();

  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  if (glic_profile_manager) {
    glic_profile_manager->OnServiceShutdown(this);
  }
}

void GlicKeyedService::ToggleUI(BrowserWindowInterface* bwi,
                                bool prevent_close,
                                mojom::InvocationSource source,
                                std::optional<std::string> prompt_suggestion,
                                bool auto_send) {
  ToggleUIInternal(bwi, prevent_close, source, std::move(prompt_suggestion),
                   auto_send, std::nullopt);
}

void GlicKeyedService::ToggleUI(BrowserWindowInterface* bwi,
                                bool prevent_close,
                                mojom::InvocationSource source) {
  ToggleUI(bwi, prevent_close, source, std::nullopt, false);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
void GlicKeyedService::ShowUiWithConversationID(BrowserWindowInterface* bwi,
                                                mojom::InvocationSource source,
                                                std::string conversation_id) {
  CHECK(source == mojom::InvocationSource::kNavigationCapture);

  ToggleUIInternal(
      bwi, /*prevent_close=*/true, source, /*prompt_suggestion=*/std::nullopt,
      /*auto_send=*/false, std::make_optional(std::move(conversation_id)));
}
#pragma clang diagnostic pop

void GlicKeyedService::ToggleUIInternal(
    BrowserWindowInterface* bwi,
    bool prevent_close,
    mojom::InvocationSource source,
    std::optional<std::string> prompt_suggestion,
    bool auto_send,
    std::optional<std::string> conversation_id) {
  // Glic may be disabled for certain user profiles (the user is browsing in
  // incognito or guest mode, policy, etc). In those cases, the entry points to
  // this method should already have been removed.
  CHECK(GlicEnabling::IsEnabledForProfile(profile_));

  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  if (glic_profile_manager) {
    glic_profile_manager->SetActiveGlic(this);
  }

  // Show the FRE if not yet completed, and if we have a browser to use.
  // Ignore ShouldBypassFreUi if auto_send is true.
  if ((!GlicEnabling::ShouldBypassFreUi(profile_, source) || auto_send) &&
      fre_controller_->ShouldShowFreDialog()) {
    fre_controller_->MarkFreStartAttempt();
#if !BUILDFLAG(IS_ANDROID)  // Single instance only
    if (!GlicEnabling::IsUnifiedFreEnabled(profile_)) {
      if (!fre_controller_->CanShowFreDialog(bwi)) {
        // If the FRE is blocked because it is already showing, we should
        // instead dismiss it. This allows the glic button to be used to toggle
        // the presence of the FRE.
        fre_controller_->DismissFreIfOpenOnActiveTab(bwi);
        return;
      }
      fre_controller_->ShowFreDialog(bwi, source);
      return;
    }
#endif
    fre_controller_->MarkSidepanelFreShown();
  }

  window_controller().Toggle(bwi ? bwi : GetActiveGlicEligibleBrowser(profile_),
                             prevent_close, source, prompt_suggestion,
                             auto_send, conversation_id);
}

void GlicKeyedService::OpenFreDialogInNewTab(BrowserWindowInterface* bwi,
                                             mojom::InvocationSource source) {
#if !BUILDFLAG(IS_ANDROID)
  // Glic may be disabled for certain user profiles (the user is browsing in
  // incognito or guest mode, policy, etc). In those cases, the entry points to
  // this method should already have been removed.
  CHECK(GlicEnabling::IsEnabledForProfile(profile_));

  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  if (glic_profile_manager) {
    glic_profile_manager->SetActiveGlic(this);
  }
  fre_controller().OpenFreDialogInNewTab(bwi, source);
#else
  NOTIMPLEMENTED() << "OpenFreDialogInNewTab";
#endif
}

void GlicKeyedService::CloseAndShutdown() {
  CHECK(!GlicEnabling::IsMultiInstanceEnabled());
  window_controller().Shutdown();
  host_manager().Shutdown();
  fre_controller_->Shutdown();
}

void GlicKeyedService::CloseAndShutdown(
    content::RenderFrameHost* render_frame_host) {
  window_controller().CloseAndShutdownInstanceWithFrame(render_frame_host);
}

void GlicKeyedService::CloseFloatingPanel() {
  window_controller().Close({});
}

#if !BUILDFLAG(IS_ANDROID)  // Single instance only
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
#endif

#if !BUILDFLAG(IS_ANDROID)  // Single instance only
glic::GlicInstanceMetrics* GlicKeyedService::instance_metrics() {
  return nullptr;
}

glic::GlicInstanceMetricsBackwardsCompatibility&
GlicKeyedService::instance_metrics_backwards_compatibility() {
  return *metrics_;
}
#endif

GlicWindowController& GlicKeyedService::window_controller() const {
  CHECK(window_controller_);
  return *window_controller_.get();
}

#if !BUILDFLAG(IS_ANDROID)  // Single instance only
GlicWindowControllerInterface&
GlicKeyedService::GetSingleInstanceWindowController() const {
  CHECK(UseDefaultWindowController());
  return static_cast<GlicWindowControllerInterface&>(window_controller());
}
#endif

GlicFreController& GlicKeyedService::fre_controller() {
  CHECK(fre_controller_);
  return *fre_controller_.get();
}

GlicSharingManager& GlicKeyedService::sharing_manager() {
  return *sharing_manager_.get();
}

bool GlicKeyedService::IsTabPinnedToAnyInstance(
    const tabs::TabHandle& tab_handle) const {
  auto instances = window_controller().GetInstances();
  return std::ranges::any_of(instances, [&](GlicInstance* instance) {
    return instance->host().sharing_manager().IsTabPinned(tab_handle);
  });
}

void GlicKeyedService::UnpinTabsFromAllInstances(
    base::span<const tabs::TabHandle> tab_handles,
    GlicUnpinTrigger trigger) {
  for (GlicInstance* instance : window_controller().GetInstances()) {
    instance->host().sharing_manager().UnpinTabs(tab_handles, trigger);
  }
}

#if !BUILDFLAG(IS_ANDROID)
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
  if (!zero_state_suggestions_manager()) {
    NOTIMPLEMENTED()
        << "Zero state suggestions not implemented for multi-instance.";
    std::move(callback).Run(nullptr);
    return;
  }
  zero_state_suggestions_manager()->ObserveZeroStateSuggestions(
      has_active_subscription, options.is_first_run, options.supported_tools,
      std::move(callback));
}
#endif

void GlicKeyedService::GuestAdded(content::WebContents* guest_contents) {
  host_manager().GuestAdded(guest_contents);
}

bool GlicKeyedService::IsWindowShowing() const {
#if !BUILDFLAG(IS_ANDROID)  // Single instance only
  if (UseDefaultWindowController()) {
    return GetSingleInstanceWindowController().IsShowing();
  }
#endif

  for (const auto* instance : window_controller().GetInstances()) {
    if (instance && instance->IsShowing()) {
      return true;
    }
  }
  return false;
}

bool GlicKeyedService::IsPanelShowingForBrowser(
    const BrowserWindowInterface& bwi) const {
  return window_controller().IsPanelShowingForBrowser(bwi);
}

bool GlicKeyedService::IsWindowDetached() const {
  return window_controller().IsDetached();
}

bool GlicKeyedService::IsWindowOrFreShowing() const {
  return IsWindowShowing() || IsFreShowing();
}

bool GlicKeyedService::IsFreShowing() const {
  return fre_controller_->IsShowingDialog();
}

base::CallbackListSubscription
GlicKeyedService::AddContextAccessIndicatorStatusChangedCallback(
    ContextAccessIndicatorChangedCallback callback) {
  return context_access_indicator_callback_list_.Add(std::move(callback));
}

tabs::TabInterface* GlicKeyedService::CreateTab(
    const ::GURL& url,
    bool open_in_background,
    const std::optional<int32_t>& window_id,
    glic::mojom::WebClientHandler::CreateTabCallback callback) {
  // If we need to open other URL types, it should be done in a more specific
  // function.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(nullptr);
    return nullptr;
  }
  NavigateParams params(profile_, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = open_in_background
                           ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                           : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      DoNavigate(&params);
  if (!navigation_handle.get()) {
    std::move(callback).Run(nullptr);
    return nullptr;
  }
  // Right after requesting the navigation, the WebContents will have almost no
  // information to populate TabData, hence the overriding of the URL. Should we
  // ever want to send more data back to the web client, we should wait until
  // the navigation commits.
  content::WebContents* new_web_contents =
      navigation_handle.get()->GetWebContents();
  mojom::TabDataPtr tab_data =
      CreateTabData(tabs::TabInterface::GetFromContents(new_web_contents));
  if (tab_data) {
    tab_data->url = url;
  }
  std::move(callback).Run(std::move(tab_data));
  return new_web_contents
             ? tabs::TabInterface::MaybeGetFromContents(new_web_contents)
             : nullptr;
}

void GlicKeyedService::SetContextAccessIndicator(bool show) {
  if (is_context_access_indicator_enabled_ == show) {
    return;
  }
  is_context_access_indicator_enabled_ = show;
  context_access_indicator_callback_list_.Notify(show);
}

#if !BUILDFLAG(IS_ANDROID)  // Single instance only
void GlicKeyedService::CreateTask(
    base::WeakPtr<actor::ActorTaskDelegate> delegate,
    actor::webui::mojom::TaskOptionsPtr options,
    mojom::WebClientHandler::CreateTaskCallback callback) {
  if (actor_task_manager_) {
    actor_task_manager_->CreateTask(weak_ptr_factory_.GetWeakPtr(),
                                    std::move(options), std::move(callback));
  } else {
    std::move(callback).Run(
        base::unexpected(mojom::CreateTaskErrorReason::kTaskSystemUnavailable));
  }
}

void GlicKeyedService::PerformActions(
    const std::vector<uint8_t>& actions_proto,
    mojom::WebClientHandler::PerformActionsCallback callback) {
  if (actor_task_manager_) {
    actor_task_manager_->PerformActions(actions_proto, std::move(callback));
  } else {
    std::move(callback).Run(
        base::unexpected(mojom::PerformActionsErrorReason::kUnknown));
  }
}

void GlicKeyedService::CancelActions(
    actor::TaskId task_id,
    mojom::WebClientHandler::CancelActionsCallback callback) {
  if (actor_task_manager_) {
    actor_task_manager_->CancelActions(task_id, std::move(callback));
  } else {
    std::move(callback).Run(mojom::CancelActionsResult::kUnknown);
  }
}

void GlicKeyedService::StopActorTask(actor::TaskId task_id,
                                     mojom::ActorTaskStopReason stop_reason) {
  if (actor_task_manager_) {
    actor_task_manager_->StopActorTask(task_id, stop_reason);
  }
}

void GlicKeyedService::PauseActorTask(actor::TaskId task_id,
                                      mojom::ActorTaskPauseReason pause_reason,
                                      tabs::TabInterface::Handle tab_handle) {
  if (actor_task_manager_) {
    actor_task_manager_->PauseActorTask(task_id, pause_reason, tab_handle);
  }
}

void GlicKeyedService::ResumeActorTask(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& context_options,
    glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) {
  if (actor_task_manager_) {
    actor_task_manager_->ResumeActorTask(task_id, context_options,
                                         std::move(callback));
  } else {
    std::move(callback).Run(mojom::GetContextResultWithActionResultCode::New(
        mojom::GetContextResult::NewErrorReason("Actor not enabled"),
        std::nullopt));
  }
}

void GlicKeyedService::InterruptActorTask(actor::TaskId task_id) {
  if (actor_task_manager_) {
    actor_task_manager_->InterruptActorTask(task_id);
  }
}

void GlicKeyedService::UninterruptActorTask(actor::TaskId task_id) {
  if (actor_task_manager_) {
    actor_task_manager_->UninterruptActorTask(task_id);
  }
}

void GlicKeyedService::CreateActorTab(
    actor::TaskId task_id,
    bool open_in_background,
    const std::optional<int32_t>& initiator_tab_id,
    const std::optional<int32_t>& initiator_window_id,
    glic::mojom::WebClientHandler::CreateActorTabCallback callback) {
  if (actor_task_manager_) {
    actor_task_manager_->CreateActorTab(task_id, open_in_background,
                                        initiator_tab_id, initiator_window_id,
                                        std::move(callback));
  } else {
    std::move(callback).Run(nullptr);
  }
}

void GlicKeyedService::OnTabAddedToTask(
    actor::TaskId task_id,
    const tabs::TabInterface::Handle& tab_handle) {}
#endif

void GlicKeyedService::OnUserInputSubmitted(glic::mojom::WebClientMode mode) {
  user_input_submitted_callback_list_.Notify();
}

base::CallbackListSubscription GlicKeyedService::AddUserInputSubmittedCallback(
    base::RepeatingClosure callback) {
  return user_input_submitted_callback_list_.Add(std::move(callback));
}

#if !BUILDFLAG(IS_ANDROID)  // Single instance only
void GlicKeyedService::CaptureRegion(
    tabs::TabInterface* tab,
    mojo::PendingRemote<mojom::CaptureRegionObserver> observer) {
  region_capture_controller_->CaptureRegion(tab, std::move(observer));
}
#endif

void GlicKeyedService::ShareContextImage(tabs::TabInterface* tab,
                                         content::RenderFrameHost* frame,
                                         const ::GURL& src_url) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicShareImage));
  CHECK(share_image_handler_);
  share_image_handler_->ShareContextImage(tab, frame, src_url);
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
  GlicProfileManager* glic_profile_manager = GlicProfileManager::GetInstance();
  CHECK(glic_profile_manager);

  glic_profile_manager->ShouldPreloadFreForProfile(
      profile_, base::BindOnce(&GlicKeyedService::FinishPreloadFre,
                               GetWeakPtr(), source));
}

void GlicKeyedService::Reload(content::RenderFrameHost* render_frame_host) {
  if (fre_controller_->IsShowingDialog()) {
    if (auto* fre_contents = fre_controller_->GetWebContents()) {
      if (fre_contents ==
          content::WebContents::FromRenderFrameHost(render_frame_host)) {
        fre_contents->GetController().Reload(
            content::ReloadType::BYPASSING_CACHE,
            /*check_for_repost=*/false);
      }
    }
  }
  window_controller().Reload(render_frame_host);
}

void GlicKeyedService::OnMemoryPressure(base::MemoryPressureLevel level) {
  if (level == base::MEMORY_PRESSURE_LEVEL_NONE ||
      (this == GlicProfileManager::GetInstance()->GetLastActiveGlic())) {
    return;
  }
  if (!GlicEnabling::IsMultiInstanceEnabled()) {
    CloseAndShutdown();
  }
  // TODO(crbug.com/453747043): Handle Multi Instance.
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

  if (base::FeatureList::IsEnabled(features::kGlicWebContentsWarming)) {
    web_contents_warming_pool_->EnsurePreload();
  } else {
    window_controller().Preload();
  }
}

void GlicKeyedService::FinishPreloadFre(GlicPrewarmingFreSource source,
                                        GlicPrewarmingChecksResult result) {
  if (result != GlicPrewarmingChecksResult::kSuccess) {
    // If FRE preloading was rejected, log error metrics and return.
    base::UmaHistogramEnumeration(
        "Glic.PrewarmingFre.ShouldNotPreloadFreForSource", source);
    if (result == GlicPrewarmingChecksResult::kWarmingDisabled) {
      base::UmaHistogramEnumeration(
          "Glic.PrewarmingFre.DisabledShouldNotPreloadFreForSource", source);
    }
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
  return window_controller().host_manager();
}

GlicInstance* GlicKeyedService::GetInstanceForTab(tabs::TabInterface* tab) {
  return window_controller().GetInstanceForTab(tab);
}

GlicInstance* GlicKeyedService::GetInstanceForActiveTab(
    BrowserWindowInterface* bwi) {
  return window_controller().GetInstanceForTab(
      bwi ? TabListInterface::From(bwi)->GetActiveTab() : nullptr);
}

void GlicKeyedService::SendAdditionalContext(
    tabs::TabHandle tab_handle,
    mojom::AdditionalContextPtr context) {
  auto* tab = tab_handle.Get();
  auto* host = &window_controller().GetInstanceForTab(tab)->host();
  host->NotifyAdditionalContext(std::move(context));
}

void GlicKeyedService::Close(
    content::RenderFrameHost* outermost_render_frame_host) {
  window_controller().CloseInstanceWithFrame(outermost_render_frame_host);
}

void GlicKeyedService::Archive(
    content::RenderFrameHost* outermost_render_frame_host) {
  window_controller().ArchiveInstanceWithFrame(outermost_render_frame_host);
}

#if !BUILDFLAG(IS_ANDROID)  // Single instance only
void GlicKeyedService::OnWebClientCleared() {
  if (actor_task_manager_) {
    actor_task_manager_->CancelTask();
  }
}

void GlicKeyedService::OnInteractionModeChange(mojom::WebClientMode new_mode) {
  // Unused in single instance mode.
}

bool GlicKeyedService::IsActive() {
  // The `browser_is_active` signal was changed to `instance_is_active`. This
  // the logic that originally backed `browser_is_active` for single-instance.
  // This function will only be called from `GlicPageHandler` when in
  // single-instance, and should be deleted when single-instance is deleted and
  // GKS no longer implements `Host::InstanceDelegate`.
  return sharing_manager().GetFocusedBrowser();
}
#endif

#if !BUILDFLAG(IS_ANDROID)  // Single instance only
void GlicKeyedService::RequestToShowCredentialSelectionDialog(
    actor::TaskId task_id,
    const base::flat_map<std::string, gfx::Image>& icons,
    const std::vector<actor_login::Credential>& credentials,
    actor::ActorTaskDelegate::CredentialSelectedCallback callback) {
  CHECK(UseDefaultWindowController());
  auto* window_controller_impl =
      static_cast<GlicWindowControllerImpl*>(window_controller_.get());
  window_controller_impl->host().RequestToShowCredentialSelectionDialog(
      task_id, icons, credentials, std::move(callback));
}

void GlicKeyedService::RequestToShowUserConfirmationDialog(
    actor::TaskId task_id,
    const url::Origin& navigation_origin,
    bool for_blocklisted_origin,
    actor::ActorTaskDelegate::UserConfirmationDialogCallback callback) {
  CHECK(UseDefaultWindowController());
  auto* window_controller_impl =
      static_cast<GlicWindowControllerImpl*>(window_controller_.get());
  window_controller_impl->host().RequestToShowUserConfirmationDialog(
      task_id, navigation_origin, for_blocklisted_origin, std::move(callback));
}

void GlicKeyedService::RequestToConfirmNavigation(
    actor::TaskId task_id,
    const url::Origin& navigation_origin,
    actor::ActorTaskDelegate::NavigationConfirmationCallback callback) {
  CHECK(UseDefaultWindowController());
  auto* window_controller_impl =
      static_cast<GlicWindowControllerImpl*>(window_controller_.get());
  window_controller_impl->host().RequestToConfirmNavigation(
      task_id, navigation_origin, std::move(callback));
}

void GlicKeyedService::RequestToShowAutofillSuggestionsDialog(
    actor::TaskId task_id,
    std::vector<autofill::ActorFormFillingRequest> requests,
    base::WeakPtr<actor::AutofillSelectionDialogEventHandler> event_handler,
    AutofillSuggestionSelectedCallback callback) {
  CHECK(UseDefaultWindowController());
  auto* window_controller_impl =
      static_cast<GlicWindowControllerImpl*>(window_controller_.get());
  window_controller_impl->host().RequestToShowAutofillSuggestionsDialog(
      task_id, std::move(requests), std::move(event_handler),
      std::move(callback));
}
#endif

base::CallbackListSubscription
GlicKeyedService::AddActOnWebCapabilityChangedCallback(
    ActOnWebCapabilityChangedCallback callback) {
  return actor_policy_checker_->AddActOnWebCapabilityChangedCallback(callback);
}

}  // namespace glic
