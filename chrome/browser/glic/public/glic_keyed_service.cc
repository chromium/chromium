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
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/browser_actuator/browser_actuator_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/common/application_hotkey_delegate.h"
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/common/glic_navigation.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_transport_handler.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_warming_checks.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/context/glic_share_image_handler.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/context/glic_tab_data_observer.h"
#include "chrome/browser/glic/host/context/glic_tab_favicon_observer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_local_storage_migration.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/features.h"
#include "components/browser_actuator/public/transport_channel.h"
#include "components/browser_actuator/public/transport_handler_factory_registry.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
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
#include "ui/base/window_open_disposition.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/android/glic_keyed_service_android.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#else
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/media/glic_media_integration.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#endif

namespace glic {

namespace {

base::TimeDelta GetWarmingDelay(GlicWarmingTrigger trigger) {
  switch (trigger) {
    case GlicWarmingTrigger::kStartup:
      return base::Milliseconds(features::kGlicWarmingDelayMs.Get());
    case GlicWarmingTrigger::kNudge:
    case GlicWarmingTrigger::kIph:
      return base::TimeDelta();
  }
}

std::string_view GlicWarmingTriggerToString(GlicWarmingTrigger trigger) {
  switch (trigger) {
    case GlicWarmingTrigger::kStartup:
      return "Startup";
    case GlicWarmingTrigger::kNudge:
      return "Nudge";
    case GlicWarmingTrigger::kIph:
      return "Iph";
  }
}

void RecordPrewarmingChecksResult(GlicWarmingTrigger trigger,
                                  GlicPrewarmingChecksResult result) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Prewarming.ChecksResult.",
                    GlicWarmingTriggerToString(trigger)}),
      result);
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
    ContextualCueingService* contextual_cueing_service,
    actor::ActorKeyedService* actor_keyed_service)
    : profile_(profile),
      actor_policy_checker_(
          actor_keyed_service
              ? std::make_unique<GlicActorPolicyChecker>(*profile_)
              : nullptr),
      enabling_(std::make_unique<GlicEnabling>(
          base::PassKey<GlicKeyedService>(),
          profile,
          &profile_manager->GetProfileAttributesStorage())),
      metrics_(std::make_unique<GlicMetrics>(profile, enabling_.get())),
      opt_in_controller_(
          std::make_unique<GlicExperimentalOptInController>(profile)),
      instance_coordinator_(std::make_unique<GlicInstanceCoordinatorImpl>(
          profile,
          identity_manager,
          this,
          enabling_.get(),
          contextual_cueing_service)),
      auth_controller_(
          base::FeatureList::IsEnabled(features::kGlicNoWebview)
              ? nullptr
              : std::make_unique<AuthController>(profile, identity_manager)),

      tab_data_observer_(std::make_unique<GlicTabDataObserver>(profile)),
      tab_favicon_observer_(std::make_unique<GlicTabFaviconObserver>(profile)) {
  CHECK(GlicEnabling::IsProfileEligible(Profile::FromBrowserContext(profile)));

  // TODO(crbug.com/450026474): Consider not constructing this metrics
  // instance for multi-instance
  metrics_->ClearControllers();
  metrics_->RecordGlicProfilePreferences();

  if (base::FeatureList::IsEnabled(features::kGlicShareImage)) {
    share_image_handler_ = std::make_unique<GlicShareImageHandler>(*this);
  }

  // If `--glic-always-open-fre` is present, unset this pref to ensure the FRE
  // is shown for testing convenience.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kGlicAlwaysOpenFre)) {
    enabling_->SetCompletedFre(prefs::FreStatus::kNotStarted);
    // or if automation is enabled, skip FRE
  } else if (command_line->HasSwitch(::switches::kGlicAutomation) ||
             command_line->HasSwitch(::switches::kGlicAlwaysSkipFre)) {
    enabling_->SetCompletedFre(prefs::FreStatus::kCompleted);
  }

  // Sets up prefs storing manually configured glic guest URLs. Intended for
  // manual testing only.
  SetupGuestUrlPresetPrefs();

  // This is only used by automation for tests.
  glic_profile_manager->MaybeAutoOpenGlicPanel();

  if (base::FeatureList::IsEnabled(features::kGlicWarming)) {
    TryPreload(GlicWarmingTrigger::kStartup);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&GlicKeyedService::InitializeAfterConstruction,
                                GetWeakPtr()));

  experimental_triggering_state_subscription_ =
      enabling_->RegisterOnExperimentalTriggeringStateChanged(
          base::BindRepeating(
              &GlicKeyedService::OnExperimentalTriggeringStateChanged,
              base::Unretained(this)));

  if (base::FeatureList::IsEnabled(browser_actuator::kBrowserActuator) &&
      base::FeatureList::IsEnabled(
          browser_actuator::
              kEnableBrowserActuatorForGlicExperimentalTriggering)) {
    browser_actuator::BrowserActuatorService* actuator_service =
        browser_actuator::BrowserActuatorServiceFactory::GetForProfile(
            profile_);
    if (actuator_service && actuator_service->GetChannel()) {
      experimental_triggering_transport_handler_factory_ =
          std::make_unique<GlicExperimentalTriggeringTransportHandlerFactory>(
              profile_);
      actuator_service->GetChannel()
          ->GetHandlerFactoryRegistry()
          ->RegisterFactory(
              experimental_triggering_transport_handler_factory_.get());
    }
  }
}

void GlicKeyedService::InitializeAfterConstruction() {
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(media::kHeadlessCaptionEarlyStart)) {
    GlicMediaIntegration::GetFor(profile_);
  }
#endif
  MaybeMigrateGlicLocalStorage(profile_);
}

GlicKeyedService::~GlicKeyedService() {
  metrics_->ClearControllers();
}

// static
GlicKeyedService* GlicKeyedService::Get(content::BrowserContext* context) {
  return GlicKeyedServiceFactory::GetGlicKeyedService(context);
}

void GlicKeyedService::Shutdown() {
  if (experimental_triggering_transport_handler_factory_) {
    browser_actuator::BrowserActuatorService* actuator_service =
        browser_actuator::BrowserActuatorServiceFactory::GetForProfile(
            profile_);
    if (actuator_service && actuator_service->GetChannel()) {
      actuator_service->GetChannel()
          ->GetHandlerFactoryRegistry()
          ->UnregisterFactory(
              experimental_triggering_transport_handler_factory_.get());
    }
    experimental_triggering_transport_handler_factory_.reset();
  }
  experimental_triggering_state_subscription_ = {};
  instance_coordinator().Shutdown();
}

void GlicKeyedService::ShowUI(BrowserWindowInterface* bwi,
                              mojom::InvocationSource source) {
  instance_coordinator().Show(
      bwi ? bwi : GetActiveGlicEligibleBrowser(profile_), source);
}

void GlicKeyedService::ToggleUI(BrowserWindowInterface* bwi,
                                bool prevent_close,
                                mojom::InvocationSource source) {
  instance_coordinator().Toggle(
      bwi ? bwi : GetActiveGlicEligibleBrowser(profile_), prevent_close,
      source);
}

base::WeakPtr<GlicInstance> GlicKeyedService::InvokeWithAutoSubmit(
    InvokeWithAutoSubmitPasskey auto_submit_passkey,
    GlicInvokeOptions options) {
  return InvokeWithAutoSubmit(auto_submit_passkey, std::move(options),
                              GlicInvokeWithAutoSubmitOptions());
}

base::WeakPtr<GlicInstance> GlicKeyedService::InvokeWithAutoSubmit(
    InvokeWithAutoSubmitPasskey auto_submit_passkey,
    GlicInvokeOptions options,
    GlicInvokeWithAutoSubmitOptions auto_submit_options) {
  enabling().MaybeRecordRecoveryOnInteraction();
  return static_cast<GlicInstanceCoordinatorImpl&>(instance_coordinator())
      .InvokeWithAutoSubmit(auto_submit_passkey, std::move(options),
                            std::move(auto_submit_options));
}

base::WeakPtr<GlicInstance> GlicKeyedService::Invoke(
    GlicInvokeOptions options) {
  enabling().MaybeRecordRecoveryOnInteraction();
  return static_cast<GlicInstanceCoordinatorImpl&>(instance_coordinator())
      .Invoke(std::move(options));
}

void GlicKeyedService::CloseAndShutdown(
    content::RenderFrameHost* render_frame_host) {
  instance_coordinator().CloseAndShutdownInstanceWithFrame(render_frame_host);
}

void GlicKeyedService::CloseFloatingPanel() {
  instance_coordinator().Close({});
}

GlicInstanceCoordinator& GlicKeyedService::instance_coordinator() const {
  CHECK(instance_coordinator_);
  return *instance_coordinator_.get();
}

GlicExperimentalOptInController& GlicKeyedService::opt_in_controller() {
  CHECK(opt_in_controller_);
  return *opt_in_controller_.get();
}

GlicSharingManagerInternal&
GlicKeyedService::active_instance_sharing_manager() {
  return instance_coordinator().active_instance_sharing_manager();
}

bool GlicKeyedService::IsPanelShowingForBrowser(
    const BrowserWindowInterface& bwi) const {
  return instance_coordinator().IsPanelShowingForBrowser(bwi);
}

bool GlicKeyedService::IsWindowDetached() const {
  return instance_coordinator().IsDetached();
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
  std::unique_ptr<NavigateParams> params;
  BrowserWindowInterface* last_active_bwi = nullptr;

  if (base::FeatureList::IsEnabled(features::kGlicCreateTabAdjacent)) {
    // Find the most recently active browser window for this profile.
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&](BrowserWindowInterface* browser) {
          if (browser->GetProfile() == profile_) {
            last_active_bwi = browser;
            return false;
          }
          return true;
        });

    if (last_active_bwi) {
      // By setting the `source_contents` and using `PAGE_TRANSITION_LINK`, the
      // new tab will be opened adjacent to the currently active tab and inherit
      // its tab group.
      params = std::make_unique<NavigateParams>(last_active_bwi, url,
                                                ui::PAGE_TRANSITION_LINK);
      params->source_contents = TabListInterface::From(last_active_bwi)
                                    ->GetActiveTab()
                                    ->GetContents();
    } else {
      params = std::make_unique<NavigateParams>(profile_, url,
                                                ui::PAGE_TRANSITION_LINK);
    }
  } else {
    params = std::make_unique<NavigateParams>(
        profile_, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  }

  params->disposition = open_in_background
                            ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                            : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  // Set navigation as renderer initiated to open links in their app/PWA (if
  // installed).
  params->is_renderer_initiated = true;
  params->initiator_origin = url::Origin();
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      glic::Navigate(std::move(params));
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

void GlicKeyedService::OnUserInputSubmitted(glic::mojom::WebClientMode mode) {
  user_input_submitted_callback_list_.Notify();
}

base::CallbackListSubscription GlicKeyedService::AddUserInputSubmittedCallback(
    base::RepeatingClosure callback) {
  return user_input_submitted_callback_list_.Add(std::move(callback));
}

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
         active_instance_sharing_manager().GetFocusedTabData().focus() &&
         active_instance_sharing_manager()
                 .GetFocusedTabData()
                 .focus()
                 ->GetContents() == contents;
}

void GlicKeyedService::AddPreloadCallback(base::OnceCallback<void()> callback) {
  preload_callback_ = std::move(callback);
}

void GlicKeyedService::TryPreload(GlicWarmingTrigger trigger) {
  base::TimeDelta delay = GetWarmingDelay(trigger);

  // TODO(b/411100559): Ideally we'd use post delayed task in all cases,
  // but this requires a refactor of tests that are currently brittle. For now,
  // just synchronously call ShouldPreloadForProfile if there is no delay.
  if (delay.is_zero()) {
    ShouldPreloadForProfile(profile_, trigger,
                            base::BindOnce(&GlicKeyedService::FinishPreload,
                                           GetWeakPtr(), trigger));
  } else {
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&GlicKeyedService::TryPreloadAfterDelay,
                           GetWeakPtr(), trigger),
            delay);
  }
}

void GlicKeyedService::TryPreloadAfterDelay(GlicWarmingTrigger trigger) {
  ShouldPreloadForProfile(
      profile_, trigger,
      base::BindOnce(&GlicKeyedService::FinishPreload, GetWeakPtr(), trigger));
}

void GlicKeyedService::Reload(content::RenderFrameHost* render_frame_host) {
  instance_coordinator().Reload(render_frame_host);
}

base::WeakPtr<GlicKeyedService> GlicKeyedService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GlicKeyedService::FinishPreload(GlicWarmingTrigger trigger,
                                     GlicPrewarmingChecksResult result) {
  if (result == GlicPrewarmingChecksResult::kSuccess) {
    if (!instance_coordinator().MaybeStartWarming(trigger)) {
      result = GlicPrewarmingChecksResult::kUnderMemoryPressure;
    }
  }

  RecordPrewarmingChecksResult(trigger, result);
  if (preload_callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(preload_callback_)));
  }
}

GlicInstance* GlicKeyedService::GetInstanceForTab(tabs::TabInterface* tab) {
  return instance_coordinator().GetInstanceForTab(tab);
}

GlicInstance* GlicKeyedService::GetInstanceForActiveTab(
    BrowserWindowInterface* bwi) {
  if (!bwi) {
    return instance_coordinator().GetInstanceForTab(nullptr);
  }
  auto* tab_list = TabListInterface::From(bwi);
  if (!tab_list) {
    return nullptr;
  }
  return instance_coordinator().GetInstanceForTab(tab_list->GetActiveTab());
}

void GlicKeyedService::Close(
    content::RenderFrameHost* outermost_render_frame_host) {
  instance_coordinator().CloseInstanceWithFrame(outermost_render_frame_host);
}

void GlicKeyedService::Archive(
    content::RenderFrameHost* outermost_render_frame_host) {
  instance_coordinator().ArchiveInstanceWithFrame(outermost_render_frame_host);
}

base::CallbackListSubscription
GlicKeyedService::AddActOnWebCapabilityChangedCallback(
    ActOnWebCapabilityChangedCallback callback) {
  return actor_policy_checker_->AddActOnWebCapabilityChangedCallback(callback);
}

GlicActorPolicyChecker& GlicKeyedService::actor_policy_checker() {
  return *actor_policy_checker_;
}

void GlicKeyedService::OnExperimentalTriggeringStateChanged() {
  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile_);
  if (device_info_sync_service) {
    device_info_sync_service->RefreshLocalDeviceInfo();
  }
}

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/484037810): Once a window features object (similar to tab
// features) is supported on Android, move ownership of the nudge controller to
// it (accessed via unowned user data and ::From methods), matching Desktop,
// rather than storing it in GlicKeyedService.
GlicNudgeController* GlicKeyedService::GetOrCreateNudgeController(
    BrowserWindowInterface* browser) {
  if (!browser) {
    return nullptr;
  }
  auto it = button_controllers_.find(browser);
  if (it != button_controllers_.end()) {
    return it->second->nudge_controller();
  }

  auto controller = std::make_unique<GlicSplitButtonController>(browser, this);
  GlicNudgeController* nudge_controller = controller->nudge_controller();
  button_controllers_[browser] = std::move(controller);

  window_close_subscriptions_[browser] =
      browser->RegisterBrowserDidClose(base::BindRepeating(
          &GlicKeyedService::OnBrowserWindowClosed, base::Unretained(this)));

  return nudge_controller;
}

void GlicKeyedService::OnBrowserWindowClosed(BrowserWindowInterface* browser) {
  button_controllers_.erase(browser);
  window_close_subscriptions_.erase(browser);
}
#endif

}  // namespace glic
