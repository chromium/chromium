// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_page_handler.h"

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_hotkey.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_settings_util.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_annotation_manager.h"
#include "chrome/browser/glic/host/glic_synthetic_trial_manager.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/media/audio_ducker.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/message.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

// Monitors the panel state and the browser widget state. Emits an event any
// time the active state changes.
// inactive = (panel hidden) || (panel attached) && (window not active)
class ActiveStateCalculator : public GlicWindowController::StateObserver {
 public:
  // Observes changes to active state.
  class Observer : public base::CheckedObserver {
   public:
    virtual void ActiveStateChanged(bool is_active) = 0;
  };

  explicit ActiveStateCalculator(GlicWindowController* window_controller)
      : window_controller_(window_controller) {
    window_controller_->AddStateObserver(this);
    PanelStateChanged(window_controller_->GetPanelState(),
                      window_controller_->attached_browser());
  }
  ~ActiveStateCalculator() override {
    window_controller_->RemoveStateObserver(this);
  }

  bool IsActive() const { return is_active_; }
  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // GlicWindowController::StateObserver implementation.
  void PanelStateChanged(const glic::mojom::PanelState& panel_state,
                         Browser* attached_browser) override {
    panel_state_kind_ = panel_state.kind;
    SetAttachedBrowser(attached_browser);
    PostRecalcAndNotify();
  }

 private:
  // Calls RecalculateAndNotify after a short delay. This is required to prevent
  // transient states from being emitted.
  void PostRecalcAndNotify() {
    calc_timer_.Start(
        FROM_HERE, base::Milliseconds(10),
        base::BindRepeating(&ActiveStateCalculator::RecalculateAndNotify,
                            base::Unretained(this)));
  }

  void RecalculateAndNotify() {
    if (Calculate() != is_active_) {
      is_active_ = !is_active_;
      observers_.Notify(&Observer::ActiveStateChanged, is_active_);
    }
  }

  void AttachedBrowserActiveChanged(BrowserWindowInterface* browser) {
    PostRecalcAndNotify();
  }

  void AttachedBrowserDidClose(BrowserWindowInterface* browser) {
    SetAttachedBrowser(nullptr);
    PostRecalcAndNotify();
  }

  bool SetAttachedBrowser(Browser* attached_browser) {
    if (attached_browser_ == attached_browser) {
      return false;
    }
    attached_browser_subscriptions_.clear();
    attached_browser_ = attached_browser;

    if (attached_browser_ && !attached_browser_->IsBrowserClosing()) {
      attached_browser_subscriptions_.push_back(
          attached_browser_->RegisterDidBecomeActive(base::BindRepeating(
              &ActiveStateCalculator::AttachedBrowserActiveChanged,
              base::Unretained(this))));
      attached_browser_subscriptions_.push_back(
          attached_browser_->RegisterDidBecomeInactive(base::BindRepeating(
              &ActiveStateCalculator::AttachedBrowserActiveChanged,
              base::Unretained(this))));
      attached_browser_subscriptions_.push_back(
          attached_browser_->RegisterBrowserDidClose(base::BindRepeating(
              &ActiveStateCalculator::AttachedBrowserDidClose,
              base::Unretained(this))));
    }
    return true;
  }

  bool Calculate() {
    if (panel_state_kind_ == glic::mojom::PanelState::Kind::kHidden) {
      return false;
    }
    if (!attached_browser_) {
      return true;
    }
    if (attached_browser_->IsBrowserClosing()) {
      return false;
    }

    return attached_browser_->IsActive();
  }

  base::OneShotTimer calc_timer_;
  std::vector<base::CallbackListSubscription> attached_browser_subscriptions_;

  raw_ptr<GlicWindowController> window_controller_;
  base::ObserverList<Observer> observers_;
  glic::mojom::PanelState::Kind panel_state_kind_;
  bool is_active_ = false;
  raw_ptr<Browser> attached_browser_ = nullptr;
};

class BrowserIsOpenCalculator : public BrowserListObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void BrowserIsOpenChanged(bool browser_is_open) = 0;
  };

  explicit BrowserIsOpenCalculator(Profile* profile, Observer* observer)
      : profile_(profile) {
    BrowserList::AddObserver(this);
    BrowserList* list = BrowserList::GetInstance();
    for (Browser* browser : *list) {
      OnBrowserAdded(browser);
    }
    // Don't notify observer during construction.
    observer_ = observer;
  }
  ~BrowserIsOpenCalculator() override { BrowserList::RemoveObserver(this); }

  void OnBrowserAdded(Browser* browser) override {
    if (browser->profile() == profile_) {
      UpdateBrowserCount(1);
    }
  }
  void OnBrowserRemoved(Browser* browser) override {
    if (browser->profile() == profile_) {
      UpdateBrowserCount(-1);
    }
  }

  bool IsOpen() const { return open_browser_count_ > 0; }

 private:
  void UpdateBrowserCount(int delta) {
    bool was_open = IsOpen();
    open_browser_count_ += delta;
    bool is_open = IsOpen();
    if (was_open != is_open && observer_) {
      observer_->BrowserIsOpenChanged(is_open);
    }
  }
  // Profile outlives this class. The glic web contents is torn down along with
  // GlicKeyedService, which is tied to the profile.
  raw_ptr<Profile> profile_;
  raw_ptr<Observer> observer_ = nullptr;
  int open_browser_count_ = 0;
};

mojom::WebClientSizingMode GetWebClientSizingMode() {
  return base::FeatureList::IsEnabled(features::kGlicSizingFitWindow)
             ? glic::mojom::WebClientSizingMode::kFitWindow
             : glic::mojom::WebClientSizingMode::kNatural;
}

}  // namespace

// WARNING: One instance of this class is created per WebUI navigated to
// chrome://glic. The design and implementation of this class, which plumbs
// events through GlicKeyedService to other components, relies on the assumption
// that there is exactly 1 WebUI instance. If this assumption is ever violated
// then many classes will break.
class GlicWebClientHandler : public glic::mojom::WebClientHandler,
                             public GlicWindowController::StateObserver,
                             public GlicWebClientAccess,
                             public BrowserAttachObserver,
                             public ActiveStateCalculator::Observer,
                             public BrowserIsOpenCalculator::Observer {
 public:
  explicit GlicWebClientHandler(
      GlicPageHandler* page_handler,
      content::BrowserContext* browser_context,
      mojo::PendingReceiver<glic::mojom::WebClientHandler> receiver)
      : profile_(Profile::FromBrowserContext(browser_context)),
        page_handler_(page_handler),
        glic_service_(
            GlicKeyedServiceFactory::GetGlicKeyedService(browser_context)),
        pref_service_(profile_->GetPrefs()),
        active_state_calculator_(&glic_service_->window_controller()),
        browser_is_open_calculator_(profile_, this),
        receiver_(this, std::move(receiver)),
        annotation_manager_(
            std::make_unique<GlicAnnotationManager>(glic_service_)) {
    active_state_calculator_.AddObserver(this);
  }

  ~GlicWebClientHandler() override {
    active_state_calculator_.RemoveObserver(this);
    if (web_client_) {
      Uninstall();
    }
  }

  // glic::mojom::WebClientHandler implementation.
  void WebClientCreated(
      ::mojo::PendingRemote<glic::mojom::WebClient> web_client,
      WebClientCreatedCallback callback) override {
    web_client_.Bind(std::move(web_client));
    web_client_.set_disconnect_handler(base::BindOnce(
        &GlicWebClientHandler::WebClientDisconnected, base::Unretained(this)));

    // Listen for changes to prefs.
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        prefs::kGlicMicrophoneEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kGlicGeolocationEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kGlicTabContextEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kGlicClosedCaptioningEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    glic_service_->window_controller().AddStateObserver(this);

    focus_changed_subscription_ = glic_service_->AddFocusedTabChangedCallback(
        base::BindRepeating(&GlicWebClientHandler::OnFocusedTabChanged,
                            base::Unretained(this)));

    focus_data_changed_subscription_ =
        glic_service_->AddFocusedTabDataChangedCallback(
            base::BindRepeating(&GlicWebClientHandler::OnFocusedTabDataChanged,
                                base::Unretained(this)));

    browser_attach_observation_ = ObserveBrowserForAttachment(profile_, this);

    system_permission_settings_observation_ =
        system_permission_settings::Observe(base::BindRepeating(
            &GlicWebClientHandler::OnOsPermissionSettingChanged,
            base::Unretained(this)));

    auto state = glic::mojom::WebClientInitialState::New();
    state->chrome_version = version_info::GetVersion();
    state->microphone_permission_enabled =
        pref_service_->GetBoolean(prefs::kGlicMicrophoneEnabled);
    state->location_permission_enabled =
        pref_service_->GetBoolean(prefs::kGlicGeolocationEnabled);
    state->tab_context_permission_enabled =
        pref_service_->GetBoolean(prefs::kGlicTabContextEnabled);
    state->os_location_permission_enabled =
        system_permission_settings::IsAllowed(ContentSettingsType::GEOLOCATION);

    state->panel_state =
        glic_service_->window_controller().GetPanelState().Clone();

    state->focused_tab_data =
        CreateFocusedTabData(glic_service_->GetFocusedTabData());
    state->can_attach = browser_attach_observation_->CanAttachToBrowser();
    state->panel_is_active = active_state_calculator_.IsActive();

    if (ShouldDoApiActivationGating()) {
      // We will force a notification to be sent later when the panel
      // is activated, so skip here.
      cached_focused_tab_data_ =
          CreateFocusedTabData(glic_service_->GetFocusedTabData());
      state->focused_tab_data = CreateFocusedTabData(NoFocusedTabData());
    } else {
      state->focused_tab_data =
          CreateFocusedTabData(glic_service_->GetFocusedTabData());
    }

    state->sizing_mode = GetWebClientSizingMode();

    state->browser_is_open = browser_is_open_calculator_.IsOpen();

    state->always_detached_mode = GlicWindowController::AlwaysDetached();

    state->enable_act_in_focused_tab =
        base::FeatureList::IsEnabled(features::kGlicActor);
    state->enable_scroll_to =
        base::FeatureList::IsEnabled(features::kGlicScrollTo);
    state->enable_zero_state_suggestions = base::FeatureList::IsEnabled(
        contextual_cueing::kGlicZeroStateSuggestions);

    local_state_pref_change_registrar_.Init(g_browser_process->local_state());
    local_state_pref_change_registrar_.Add(
        prefs::kGlicLauncherHotkey,
        base::BindRepeating(&GlicWebClientHandler::OnLocalStatePrefChanged,
                            base::Unretained(this)));
    state->hotkey = GetHotkeyString();
    state->enable_closed_captioning_feature =
        base::FeatureList::IsEnabled(features::kGlicClosedCaptioning);
    state->closed_captioning_setting_enabled =
        pref_service_->GetBoolean(prefs::kGlicClosedCaptioningEnabled);

    std::move(callback).Run(std::move(state));
  }

  void WebClientInitializeFailed() override {
    glic_service_->host().WebClientInitializeFailed(this);
  }

  void WebClientInitialized() override {
    glic_service_->host().SetWebClient(page_handler_, this);
    // If chrome://glic is opened in a tab for testing, send a synthetic open
    // signal.
    if (page_handler_->webui_contents() !=
        glic_service_->host().webui_contents()) {
      mojom::PanelOpeningDataPtr panel_opening_data =
          mojom::PanelOpeningData::New();
      panel_opening_data->panel_state =
          glic_service_->window_controller().GetPanelState().Clone();
      panel_opening_data->invocation_source =
          mojom::InvocationSource::kUnsupported;
      web_client_->NotifyPanelWillOpen(std::move(panel_opening_data),
                                       base::DoNothing());
    }
  }

  void CreateTab(const ::GURL& url,
                 bool open_in_background,
                 const std::optional<int32_t> window_id,
                 CreateTabCallback callback) override {
    if (ShouldDoApiActivationGating()) {
      std::move(callback).Run(nullptr);
      return;
    }
    glic_service_->CreateTab(url, open_in_background, window_id,
                             std::move(callback));
  }

  void OpenGlicSettingsPage(mojom::OpenSettingsOptionsPtr options) override {
    switch (options->highlightField) {
      case mojom::SettingsPageField::kOsHotkey:
        ::glic::OpenGlicKeyboardShortcutSetting(profile_);
        base::RecordAction(
            base::UserMetricsAction("GlicSessionSettingsOpened.OsHotkey"));
        break;
      case mojom::SettingsPageField::kOsEntrypointToggle:
        ::glic::OpenGlicOsToggleSetting(profile_);
        base::RecordAction(base::UserMetricsAction(
            "GlicSessionSettingsOpened.OsEntrypointToggle"));
        break;
      case mojom::SettingsPageField::kNone:  // Default value.
        ::glic::OpenGlicSettingsPage(profile_);
        base::RecordAction(
            base::UserMetricsAction("GlicSessionSettingsOpened.Default"));
        break;
    }
  }

  void ClosePanel() override { glic_service_->ClosePanel(); }

  void ClosePanelAndShutdown() override {
    // Despite the name, CloseUI here tears down the web client in addition to
    // closing the window.
    glic_service_->CloseUI();
  }

  void AttachPanel() override {
    if (GlicWindowController::AlwaysDetached()) {
      receiver_.ReportBadMessage(
          "AttachPanel cannot be called when always detached mode is enabled.");
      return;
    }
    glic_service_->AttachPanel();
  }

  void DetachPanel() override {
    if (GlicWindowController::AlwaysDetached()) {
      receiver_.ReportBadMessage(
          "DetachPanel cannot be called when always detached mode is enabled.");
      return;
    }
    glic_service_->DetachPanel();
  }

  void ShowProfilePicker() override {
    glic::GlicProfileManager::GetInstance()->ShowProfilePicker();
  }

  void ResizeWidget(const gfx::Size& size,
                    base::TimeDelta duration,
                    ResizeWidgetCallback callback) override {
    glic_service_->ResizePanel(size, duration, std::move(callback));
  }

  void GetContextFromFocusedTab(
      glic::mojom::GetTabContextOptionsPtr options,
      GetContextFromFocusedTabCallback callback) override {
    glic_service_->GetContextFromFocusedTab(*options, std::move(callback));
  }

  void ActInFocusedTab(const std::vector<uint8_t>& action_proto,
                       glic::mojom::GetTabContextOptionsPtr options,
                       ActInFocusedTabCallback callback) override {
    if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
      receiver_.ReportBadMessage(
          "ActInFocusedTab cannot be called without GlicActor enabled.");
      return;
    }
    glic_service_->ActInFocusedTab(action_proto, *options, std::move(callback));
  }

  void StopActorTask(int32_t task_id) override {
    if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
      receiver_.ReportBadMessage(
          "StopActorTask cannot be called without GlicActor enabled.");
      return;
    }
    glic_service_->StopActorTask(actor::TaskId(task_id));
  }

  void PauseActorTask(int32_t task_id) override {
    if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
      receiver_.ReportBadMessage(
          "PauseActorTask cannot be called without GlicActor enabled.");
      return;
    }
    glic_service_->PauseActorTask(actor::TaskId(task_id));
  }

  void ResumeActorTask(int32_t task_id,
                       glic::mojom::GetTabContextOptionsPtr context_options,
                       ResumeActorTaskCallback callback) override {
    if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
      receiver_.ReportBadMessage(
          "ResumeActorTask cannot be called without GlicActor enabled.");
      return;
    }
    glic_service_->ResumeActorTask(actor::TaskId(task_id), *context_options,
                                   std::move(callback));
  }

  void CaptureScreenshot(CaptureScreenshotCallback callback) override {
    if (ShouldDoApiActivationGating()) {
      std::move(callback).Run(mojom::CaptureScreenshotResult::NewErrorReason(
          glic::mojom::CaptureScreenshotErrorReason::kUnknown));
      return;
    }
    glic_service_->CaptureScreenshot(std::move(callback));
  }

  void SetAudioDucking(bool enabled,
                       SetAudioDuckingCallback callback) override {
    content::RenderFrameHost* guest_frame = page_handler_->GetGuestMainFrame();
    if (!guest_frame) {
      std::move(callback).Run(false);
      return;
    }
    AudioDucker* audio_ducker =
        AudioDucker::GetOrCreateForPage(guest_frame->GetPage());
    std::move(callback).Run(enabled ? audio_ducker->StartDuckingOtherAudio()
                                    : audio_ducker->StopDuckingOtherAudio());
  }

  void SetPanelDraggableAreas(
      const std::vector<gfx::Rect>& draggable_areas,
      SetPanelDraggableAreasCallback callback) override {
    if (!draggable_areas.empty()) {
      glic_service_->SetPanelDraggableAreas(draggable_areas);
    } else {
      // Default to the top bar area of the panel.
      // TODO(cuianthony): Define panel dimensions constants in shared location.
      glic_service_->SetPanelDraggableAreas({{0, 0, 400, 80}});
    }
    std::move(callback).Run();
  }

  void SetMinimumPanelSize(const gfx::Size& size) override {
    glic_service_->window_controller().SetMinimumWidgetSize(size);
  }

  void SetMicrophonePermissionState(
      bool enabled,
      SetMicrophonePermissionStateCallback callback) override {
    pref_service_->SetBoolean(prefs::kGlicMicrophoneEnabled, enabled);
    if (enabled) {
      base::RecordAction(
          base::UserMetricsAction("GlicMicrophonePermissionEnabled"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("GlicMicrophonePermissionDisabled"));
    }
    std::move(callback).Run();
  }

  void SetLocationPermissionState(
      bool enabled,
      SetLocationPermissionStateCallback callback) override {
    pref_service_->SetBoolean(prefs::kGlicGeolocationEnabled, enabled);
    if (enabled) {
      base::RecordAction(
          base::UserMetricsAction("GlicLocationPermissionEnabled"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("GlicLocationPermissionDisabled"));
    }
    std::move(callback).Run();
  }

  void SetTabContextPermissionState(
      bool enabled,
      SetTabContextPermissionStateCallback callback) override {
    pref_service_->SetBoolean(prefs::kGlicTabContextEnabled, enabled);
    if (enabled) {
      base::RecordAction(
          base::UserMetricsAction("GlicTabContextPermissionEnabled"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("GlicTabContextPermissionDisabled"));
    }
    std::move(callback).Run();
  }

  void SetClosedCaptioningSetting(
      bool enabled,
      SetClosedCaptioningSettingCallback callback) override {
    if (!base::FeatureList::IsEnabled(features::kGlicClosedCaptioning)) {
      receiver_.ReportBadMessage(
          "Client should not be able to call SetClosedCaptioningSetting "
          "without the GlicClosedCaptioning feature enabled.");
      return;
    }
    pref_service_->SetBoolean(prefs::kGlicClosedCaptioningEnabled, enabled);
    if (enabled) {
      base::RecordAction(
          base::UserMetricsAction("GlicClosedCaptioningEnabled"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("GlicClosedCaptioningDisabled"));
    }
    std::move(callback).Run();
  }

  void ShouldAllowMediaPermissionRequest(
      ShouldAllowMediaPermissionRequestCallback callback) override {
    std::move(callback).Run(
        pref_service_->GetBoolean(prefs::kGlicMicrophoneEnabled) &&
        glic_service_->window_controller().IsShowing());
  }

  void ShouldAllowGeolocationPermissionRequest(
      ShouldAllowGeolocationPermissionRequestCallback callback) override {
    std::move(callback).Run(
        pref_service_->GetBoolean(prefs::kGlicGeolocationEnabled) &&
        glic_service_->window_controller().IsShowing());
  }

  void SetContextAccessIndicator(bool enabled) override {
    glic_service_->SetContextAccessIndicator(enabled);
  }

  void GetUserProfileInfo(GetUserProfileInfoCallback callback) override {
    if (ShouldDoGetUserProfileInfoApiActivationGating()) {
      on_get_user_profile_info_activation_callbacks_.push_back(
          base::BindOnce(&GlicWebClientHandler::GetUserProfileInfo,
                         base::Unretained(this), std::move(callback)));
      return;
    }

    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile_->GetPath());
    if (!entry) {
      std::move(callback).Run(nullptr);
      return;
    }

    auto result = glic::mojom::UserProfileInfo::New();
    // TODO(crbug.com/382794680): Determine the correct size.
    gfx::Image icon = entry->GetAvatarIcon(512);
    if (!icon.IsEmpty()) {
      result->avatar_icon = icon.AsBitmap();
    }
    result->display_name = base::UTF16ToUTF8(entry->GetGAIAName());
    result->email = base::UTF16ToUTF8(entry->GetUserName());
    result->given_name = base::UTF16ToUTF8(entry->GetGAIAGivenName());
    result->local_profile_name =
        base::UTF16ToUTF8(entry->GetLocalProfileName());
    policy::ManagementService* management_service =
        policy::ManagementServiceFactory::GetForProfile(profile_);
    result->is_managed =
        management_service && management_service->IsAccountManaged();
    std::move(callback).Run(std::move(result));
  }

  void SyncCookies(SyncCookiesCallback callback) override {
    glic_service_->GetAuthController().ForceSyncCookies(std::move(callback));
  }

  void OnUserInputSubmitted(glic::mojom::WebClientMode mode) override {
    glic_service_->metrics()->OnUserInputSubmitted(mode);
  }

  void OnResponseStarted() override {
    glic_service_->metrics()->OnResponseStarted();
  }

  void OnResponseStopped() override {
    glic_service_->metrics()->OnResponseStopped();
  }

  void OnSessionTerminated() override {
    glic_service_->metrics()->OnSessionTerminated();
  }

  void OnResponseRated(bool positive) override {
    glic_service_->metrics()->OnResponseRated(positive);
  }

  void ScrollTo(mojom::ScrollToParamsPtr params,
                ScrollToCallback callback) override {
    if (!base::FeatureList::IsEnabled(features::kGlicScrollTo)) {
      receiver_.ReportBadMessage(
          "Client should not be able to call ScrollTo without the GlicScrollTo "
          "feature enabled.");
      return;
    }
    if (ShouldDoApiActivationGating()) {
      std::move(callback).Run(glic::mojom::ScrollToErrorReason::kNotSupported);
      return;
    }
    annotation_manager_->ScrollTo(std::move(params), std::move(callback));
  }

  void DropScrollToHighlight() override {
    if (!base::FeatureList::IsEnabled(features::kGlicScrollTo)) {
      receiver_.ReportBadMessage(
          "Client should not be able to call DropScrollToHighlight without the "
          "GlicScrollTo feature enabled.");
      return;
    }
    annotation_manager_->RemoveAnnotation(
        mojom::ScrollToErrorReason::kDroppedByWebClient);
  }

  void SetSyntheticExperimentState(const std::string& trial_name,
                                   const std::string& group_name) override {
    g_browser_process->GetFeatures()
        ->glic_synthetic_trial_manager()
        ->SetSyntheticExperimentState(trial_name, group_name);
  }

  void OpenOsPermissionSettingsMenu(ContentSettingsType type) override {
    if (type != ContentSettingsType::MEDIASTREAM_MIC &&
        type != ContentSettingsType::GEOLOCATION) {
      // This will terminate the render process.
      receiver_.ReportBadMessage(
          "OpenOsPermissionSettingsMenu received for unsupported "
          "OS permission.");
      return;
    }
    system_permission_settings::OpenSystemSettings(
        page_handler_->webui_contents(), type);
  }

  void GetOsMicrophonePermissionStatus(
      GetOsMicrophonePermissionStatusCallback callback) override {
    std::move(callback).Run(system_permission_settings::IsAllowed(
        ContentSettingsType::MEDIASTREAM_MIC));
  }

  // GlicWindowController::StateObserver implementation.
  void PanelStateChanged(const glic::mojom::PanelState& panel_state,
                         Browser* attached_browser) override {
    web_client_->NotifyPanelStateChange(panel_state.Clone());
  }

  // GlicWebClientAccess implementation.

  void PanelWillOpen(glic::mojom::PanelOpeningDataPtr panel_opening_data,
                     PanelWillOpenCallback done) override {
    web_client_->NotifyPanelWillOpen(
        std::move(panel_opening_data),
        base::BindOnce(
            [](PanelWillOpenCallback done, glic::mojom::OpenPanelInfoPtr info) {
              base::UmaHistogramEnumeration("Glic.Api.NotifyPanelWillOpen",
                                            info->web_client_mode);
              std::move(done).Run(std::move(info));
            },
            std::move(done)));
  }

  void PanelWasClosed(base::OnceClosure done) override {
    web_client_->NotifyPanelWasClosed(
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(done)));
  }

  void ManualResizeChanged(bool resizing) override {
    web_client_->NotifyManualResizeChanged(resizing);
  }

  // BrowserAttachmentObserver implementation.
  void CanAttachToBrowserChanged(bool can_attach) override {
    web_client_->NotifyPanelCanAttachChange(can_attach);
  }
  // ActiveStateCalculator implementation.
  void ActiveStateChanged(bool is_active) override {
    if (web_client_) {
      web_client_->NotifyPanelActiveChange(is_active);
    }

    if (!is_active) {
      return;
    }

    if (base::FeatureList::IsEnabled(
            features::kGlicGetUserProfileInfoApiActivationGating)) {
      auto to_remove =
          std::move(on_get_user_profile_info_activation_callbacks_);
      on_get_user_profile_info_activation_callbacks_.clear();
      for (auto& cb : to_remove) {
        std::move(cb).Run();
      }
    }

    CHECK(on_get_user_profile_info_activation_callbacks_.empty());

    if (base::FeatureList::IsEnabled(features::kGlicApiActivationGating)) {
      if (cached_focused_tab_data_) {
        web_client_->NotifyFocusedTabChanged(
            std::move(cached_focused_tab_data_));
      }
      cached_focused_tab_data_ = nullptr;
    }
  }

  // BrowserIsOpenCalculator implementation.
  void BrowserIsOpenChanged(bool is_open) override {
    if (web_client_) {
      web_client_->NotifyBrowserIsOpenChanged(is_open);
    }
  }

  void GetZeroStateSuggestionsForFocusedTab(
      std::optional<bool> is_fre,
      GetZeroStateSuggestionsForFocusedTabCallback callback) override {
    if (!base::FeatureList::IsEnabled(
            contextual_cueing::kGlicZeroStateSuggestions)) {
      receiver_.ReportBadMessage(
          "Client should not call "
          "GetZeroStateSuggestionsForFocusedTab "
          "without the GlicZeroStateSuggestions feature enabled.");
      return;
    }

    if (ShouldDoApiActivationGating()) {
      std::move(callback).Run(nullptr);
      return;
    }

    glic_service_->FetchZeroStateSuggestions(
        is_fre.value_or(false),
        base::BindOnce(
            [](GetZeroStateSuggestionsForFocusedTabCallback callback,
               base::TimeTicks start,
               glic::mojom::ZeroStateSuggestionsPtr suggestions) {
              base::UmaHistogramTimes(
                  "Glic.Api.FetchZeroStateSuggestionsLatency",
                  base::TimeTicks::Now() - start);
              std::move(callback).Run(std::move(suggestions));
            },
            std::move(callback), base::TimeTicks::Now()));
  }

  void OnOsPermissionSettingChanged(ContentSettingsType content_type,
                                    bool is_blocked) {
    // Ignore other content types.
    if (content_type == ContentSettingsType::GEOLOCATION) {
      web_client_->NotifyOsLocationPermissionStateChanged(!is_blocked);
    }
  }

 private:
  void Uninstall() {
    SetAudioDucking(false, base::DoNothing());
    // TODO(b/409332639): centralize access indicator resetting in a single
    // class.
    glic_service_->SetContextAccessIndicator(false);
    glic_service_->host().SetWebClient(page_handler_, nullptr);
    pref_change_registrar_.Reset();
    local_state_pref_change_registrar_.Reset();
    glic_service_->window_controller().RemoveStateObserver(this);
    focus_changed_subscription_ = {};
    browser_attach_observation_.reset();
  }

  void WebClientDisconnected() { Uninstall(); }

  void OnPrefChanged(const std::string& pref_name) {
    bool is_enabled = pref_service_->GetBoolean(pref_name);
    if (pref_name == prefs::kGlicMicrophoneEnabled) {
      web_client_->NotifyMicrophonePermissionStateChanged(is_enabled);
    } else if (pref_name == prefs::kGlicGeolocationEnabled) {
      web_client_->NotifyLocationPermissionStateChanged(is_enabled);
    } else if (pref_name == prefs::kGlicTabContextEnabled) {
      web_client_->NotifyTabContextPermissionStateChanged(is_enabled);
    } else if (pref_name == prefs::kGlicClosedCaptioningEnabled) {
      web_client_->NotifyClosedCaptioningSettingChanged(is_enabled);
    } else {
      DCHECK(false) << "Unknown Glic permission pref changed: " << pref_name;
    }
  }

  void OnLocalStatePrefChanged(const std::string& pref_name) {
    if (pref_name == prefs::kGlicLauncherHotkey) {
      web_client_->NotifyOsHotkeyStateChanged(GetHotkeyString());
    } else {
      CHECK(false) << "Unknown local state pref changed: " << pref_name;
    }
  }

  void OnFocusedTabChanged(FocusedTabData focused_tab_data) {
    if (ShouldDoApiActivationGating()) {
      cached_focused_tab_data_ = CreateFocusedTabData(focused_tab_data);
      return;
    }
    web_client_->NotifyFocusedTabChanged(
        CreateFocusedTabData(focused_tab_data));
  }

  void OnFocusedTabDataChanged(const glic::mojom::TabData* tab_data) {
    if (!tab_data) {
      return;
    }
    if (ShouldDoApiActivationGating()) {
      cached_focused_tab_data_ =
          glic::mojom::FocusedTabData::NewFocusedTab(tab_data->Clone());
      return;
    }
    web_client_->NotifyFocusedTabChanged(
        glic::mojom::FocusedTabData::NewFocusedTab(tab_data->Clone()));
  }

  bool ShouldDoApiActivationGating() const {
    return base::FeatureList::IsEnabled(features::kGlicApiActivationGating) &&
           !active_state_calculator_.IsActive();
  }

  bool ShouldDoGetUserProfileInfoApiActivationGating() const {
    return base::FeatureList::IsEnabled(
               features::kGlicGetUserProfileInfoApiActivationGating) &&
           !active_state_calculator_.IsActive();
  }

  glic::mojom::FocusedTabDataPtr cached_focused_tab_data_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;
  raw_ptr<Profile> profile_;
  raw_ptr<GlicPageHandler> page_handler_;
  raw_ptr<GlicKeyedService> glic_service_;
  raw_ptr<PrefService> pref_service_;
  ActiveStateCalculator active_state_calculator_;
  BrowserIsOpenCalculator browser_is_open_calculator_;
  base::CallbackListSubscription focus_changed_subscription_;
  base::CallbackListSubscription focus_data_changed_subscription_;
  std::unique_ptr<TabDataObserver> focused_tab_data_observer_;
  mojo::Receiver<glic::mojom::WebClientHandler> receiver_;
  mojo::Remote<glic::mojom::WebClient> web_client_;
  std::unique_ptr<BrowserAttachObservation> browser_attach_observation_;
  const std::unique_ptr<GlicAnnotationManager> annotation_manager_;
  std::unique_ptr<system_permission_settings::ScopedObservation>
      system_permission_settings_observation_;
  std::vector<base::OnceClosure> on_get_user_profile_info_activation_callbacks_;
};

GlicPageHandler::GlicPageHandler(
    content::WebContents* webui_contents,
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
    mojo::PendingRemote<mojom::Page> page)
    : webui_contents_(webui_contents),
      browser_context_(webui_contents->GetBrowserContext()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  GetGlicService()->host().WebUIPageHandlerAdded(this);
  subscriptions_.push_back(
      GetGlicService()->enabling().RegisterAllowedChanged(base::BindRepeating(
          &GlicPageHandler::AllowedChanged, base::Unretained(this))));
  AllowedChanged();
}

GlicPageHandler::~GlicPageHandler() {
  WebUiStateChanged(glic::mojom::WebUiState::kUninitialized);
  // `GlicWebClientHandler` holds a pointer back to us, so delete it first.
  web_client_handler_.reset();
  GetGlicService()->host().WebUIPageHandlerRemoved(this);
}

GlicKeyedService* GlicPageHandler::GetGlicService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(browser_context_);
}

void GlicPageHandler::CreateWebClient(
    ::mojo::PendingReceiver<glic::mojom::WebClientHandler>
        web_client_receiver) {
  web_client_handler_ = std::make_unique<GlicWebClientHandler>(
      this, browser_context_, std::move(web_client_receiver));
}

void GlicPageHandler::PrepareForClient(
    base::OnceCallback<void(mojom::PrepareForClientResult)> callback) {
  GetGlicService()->GetAuthController().CheckAuthBeforeLoad(
      std::move(callback));
}

void GlicPageHandler::WebviewCommitted(const GURL& url) {
  // TODO(crbug.com/388328847): Remove this code once launch issues are ironed
  // out.
  if (url.DomainIs("login.corp.google.com") ||
      url.DomainIs("accounts.google.com")) {
    GetGlicService()->host().LoginPageCommitted(this);
  }
}

void GlicPageHandler::NotifyWindowIntentToShow() {
  page_->IntentToShow();
}

content::RenderFrameHost* GlicPageHandler::GetGuestMainFrame() {
  extensions::WebViewGuest* web_view_guest = nullptr;
  content::RenderFrameHost* webui_frame =
      webui_contents_->GetPrimaryMainFrame();
  if (!webui_frame) {
    return nullptr;
  }
  webui_frame->ForEachRenderFrameHostWithAction(
      [&web_view_guest](content::RenderFrameHost* rfh) {
        auto* web_view = extensions::WebViewGuest::FromRenderFrameHost(rfh);
        if (web_view && web_view->attached()) {
          web_view_guest = web_view;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return web_view_guest ? web_view_guest->GetGuestMainFrame() : nullptr;
}

void GlicPageHandler::ClosePanel() {
  GetGlicService()->ClosePanel();
}

void GlicPageHandler::SignInAndClosePanel() {
  GetGlicService()->GetAuthController().ShowReauthForAccount(base::BindOnce(
      &GlicWindowController::ShowAfterSignIn,
      // Unretained is safe because the keyed service owns the
      // auth controller and the window controller.
      base::Unretained(&GetGlicService()->window_controller()), nullptr));
  GetGlicService()->window_controller().Close();
}

void GlicPageHandler::ResizeWidget(const gfx::Size& size,
                                   base::TimeDelta duration,
                                   ResizeWidgetCallback callback) {
  GetGlicService()->ResizePanel(size, duration, std::move(callback));
}

void GlicPageHandler::EnableDragResize(bool enabled) {
  // features::kGlicUserResize is not checked here because the WebUI page
  // invokes this method when it is disabled, too (when its state changes).
  GetGlicService()->window_controller().EnableDragResize(enabled);
}

void GlicPageHandler::WebUiStateChanged(glic::mojom::WebUiState new_state) {
  GetGlicService()->host().WebUiStateChanged(this, new_state);
}

void GlicPageHandler::AllowedChanged() {
  page_->SetProfileReadyState(GlicEnabling::GetProfileReadyState(
      Profile::FromBrowserContext(browser_context_)));
}

}  // namespace glic
