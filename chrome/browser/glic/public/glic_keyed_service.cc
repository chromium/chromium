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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/common/application_hotkey_delegate.h"
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/common/glic_navigation.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#endif
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/context/glic_share_image_handler.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/context/glic_tab_data_observer.h"
#include "chrome/browser/glic/host/context/glic_tab_favicon_observer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
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
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/media/glic_media_integration.h"
#include "chrome/browser/glic/widget/glic_widget.h"
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
      fre_controller_(
          std::make_unique<GlicFreController>(profile, identity_manager)),
#if !BUILDFLAG(IS_ANDROID)
      opt_in_controller_(
          std::make_unique<GlicExperimentalOptInController>(profile)),
#endif
      instance_coordinator_(std::make_unique<GlicInstanceCoordinatorImpl>(
          profile,
          identity_manager,
          this,
          enabling_.get(),
          contextual_cueing_service)),
      auth_controller_(
          std::make_unique<AuthController>(profile, identity_manager)),

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
    TryPreload();
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&GlicKeyedService::InitializeAfterConstruction,
                                GetWeakPtr()));

  experimental_triggering_state_subscription_ =
      enabling_->RegisterOnExperimentalTriggeringStateChanged(
          base::BindRepeating(
              &GlicKeyedService::OnExperimentalTriggeringStateChanged,
              base::Unretained(this)));
}

void GlicKeyedService::InitializeAfterConstruction() {
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(media::kHeadlessCaptionEarlyStart)) {
    GlicMediaIntegration::GetFor(profile_);
  }
#endif
}

GlicKeyedService::~GlicKeyedService() {
  metrics_->ClearControllers();
}

// static
GlicKeyedService* GlicKeyedService::Get(content::BrowserContext* context) {
  return GlicKeyedServiceFactory::GetGlicKeyedService(context);
}

void GlicKeyedService::Shutdown() {
  instance_coordinator().Shutdown();
}

void GlicKeyedService::ToggleUI(BrowserWindowInterface* bwi,
                                bool prevent_close,
                                mojom::InvocationSource source) {
  // Glic may be disabled for certain user profiles (the user is browsing in
  // incognito or guest mode, policy, etc). In those cases, the entry points to
  // this method should already have been removed.
  CHECK(GlicEnabling::ShouldShowGlicButton(profile_));

  if (MaybeInvoke(bwi, source)) {
    return;
  }

  instance_coordinator().Toggle(
      bwi ? bwi : GetActiveGlicEligibleBrowser(profile_), prevent_close,
      source);
}

bool GlicKeyedService::MaybeInvoke(BrowserWindowInterface* bwi,
                                   mojom::InvocationSource source) {
  BrowserWindowInterface* target_bwi =
      bwi ? bwi : GetActiveGlicEligibleBrowser(profile_);
  if (!target_bwi) {
    return false;
  }

  bool panel_closed = !IsPanelShowingForBrowser(*target_bwi);
  bool fre_override_compatible =
      !GlicEnabling::HasConsentedForProfile(profile_);

  if (fre_override_compatible && panel_closed &&
      base::FeatureList::IsEnabled(features::kGlicMessageFirstFre)) {
    GlicInvokeOptions options(source);
    if (auto* active_tab = TabListInterface::From(target_bwi)->GetActiveTab()) {
      options.target = Target(*active_tab);
    }
    options.fre_override = mojom::FreOverride::kTrustFirstInline;
    Invoke(std::move(options));
    return true;
  }

  return false;
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
  CHECK(GlicEnabling::IsEnabledForProfile(profile_));

  return static_cast<GlicInstanceCoordinatorImpl&>(instance_coordinator())
      .InvokeWithAutoSubmit(auto_submit_passkey, std::move(options),
                            std::move(auto_submit_options));
}

base::WeakPtr<GlicInstance> GlicKeyedService::Invoke(
    GlicInvokeOptions options) {
  CHECK(GlicEnabling::IsEnabledForProfile(profile_));

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

GlicFreController& GlicKeyedService::fre_controller() {
  CHECK(fre_controller_);
  return *fre_controller_.get();
}

#if !BUILDFLAG(IS_ANDROID)
GlicExperimentalOptInController& GlicKeyedService::opt_in_controller() {
  CHECK(opt_in_controller_);
  return *opt_in_controller_.get();
}
#endif

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
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&GlicKeyedService::TryPreloadAfterDelay,
                           GetWeakPtr()),
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

void GlicKeyedService::Reload(content::RenderFrameHost* render_frame_host) {
  instance_coordinator().Reload(render_frame_host);
}

base::WeakPtr<GlicKeyedService> GlicKeyedService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
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

  instance_coordinator().EnsurePreload();
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

}  // namespace glic
