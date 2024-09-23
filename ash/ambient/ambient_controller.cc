// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_animation_ui_launcher.h"
#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_managed_slideshow_ui_launcher.h"
#include "ash/ambient/ambient_photo_cache.h"
#include "ash/ambient/ambient_photo_cache_settings.h"
#include "ash/ambient/ambient_slideshow_ui_launcher.h"
#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/ambient_video_ui_launcher.h"
#include "ash/ambient/managed/screensaver_images_policy_handler.h"
#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/ambient/metrics/ambient_session_metrics_recorder.h"
#include "ash/ambient/metrics/managed_screensaver_metrics.h"
#include "ash/ambient/model/ambient_animation_photo_config.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ambient/model/ambient_slideshow_photo_config.h"
#include "ash/ambient/model/ambient_topic_queue_animation_delegate.h"
#include "ash/ambient/model/ambient_topic_queue_slideshow_delegate.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/ui/ambient_animation_frame_rate_controller.h"
#include "ash/ambient/ui/ambient_animation_progress_tracker.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/constants/ash_features.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_mode_photo_source.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/power_status.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "cc/paint/skottie_wrapper.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"

#if BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
#include "ash/ambient/backdrop/ambient_backend_controller_impl.h"
#endif  // BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)

namespace ash {

namespace {

// Used by wake lock APIs.
constexpr char kWakeLockReason[] = "AmbientMode";

// kAmbientModeRunningDurationMinutes with value 0 means "forever".
constexpr int kDurationForever = 0;

std::unique_ptr<AmbientBackendController> CreateAmbientBackendController() {
#if BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
  return std::make_unique<AmbientBackendControllerImpl>();
#else
  return std::make_unique<FakeAmbientBackendControllerImpl>();
#endif  // BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
}

// Returns the name of the ambient widget.
std::string GetWidgetName() {
  if (ambient::util::IsShowing(LockScreen::ScreenType::kLock))
    return "LockScreenAmbientModeContainer";
  return "InSessionAmbientModeContainer";
}

// Returns true if the device is currently connected to a charger.
bool IsChargerConnected() {
  DCHECK(PowerStatus::IsInitialized());
  auto* power_status = PowerStatus::Get();
  if (power_status->IsBatteryPresent()) {
    // If battery is charging, that implies sufficient power is connected. If
    // battery is not charging, return true only if an official, non-USB charger
    // is connected. This will happen if the battery is fully charged or
    // charging is delayed by Adaptive Charging.
    return power_status->IsBatteryCharging() ||
           power_status->IsMainsChargerConnected();
  } else {
    // Chromeboxes have no battery.
    return power_status->IsLinePowerConnected();
  }
}

bool IsUiHidden(AmbientUiVisibility visibility) {
  return visibility == AmbientUiVisibility::kHidden;
}

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

PrefService* GetSigninPrefService() {
  return Shell::Get()->session_controller()->GetSigninScreenPrefService();
}

PrefService* GetActivePrefService() {
  if (GetPrimaryUserPrefService()) {
    return GetPrimaryUserPrefService();
  }
  if (ash::features::IsAmbientModeManagedScreensaverEnabled()) {
    return GetSigninPrefService();
  }
  return nullptr;
}

bool IsUserAmbientModeEnabled() {
  if (!AmbientClient::Get()->IsAmbientModeAllowed()) {
    return false;
  }

  auto* pref_service = GetPrimaryUserPrefService();
  return pref_service &&
         pref_service->GetBoolean(ambient::prefs::kAmbientModeEnabled);
}

bool IsAmbientModeManagedScreensaverEnabled() {
  PrefService* pref_service = GetActivePrefService();

  return ash::features::IsAmbientModeManagedScreensaverEnabled() &&
         !chromeos::IsKioskSession() && pref_service &&
         pref_service->GetBoolean(
             ambient::prefs::kAmbientModeManagedScreensaverEnabled);
}

bool IsAmbientModeEnabled() {
  return IsUserAmbientModeEnabled() || IsAmbientModeManagedScreensaverEnabled();
}

class AmbientWidgetDelegate : public views::WidgetDelegate {
 public:
  AmbientWidgetDelegate() {
    SetCanFullscreen(true);
    SetCanMaximize(true);
    SetOwnedByWidget(true);
  }
};

void RecordManagedScreensaverEnabledPref() {
  if (!ash::features::IsAmbientModeManagedScreensaverEnabled()) {
    return;
  }

  if (PrefService* pref_service = GetActivePrefService();
      pref_service &&
      pref_service->IsManagedPreference(
          ambient::prefs::kAmbientModeManagedScreensaverEnabled)) {
    RecordManagedScreensaverEnabled(pref_service->GetBoolean(
        ambient::prefs::kAmbientModeManagedScreensaverEnabled));
  }
}

}  // namespace

// static
void AmbientController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(ash::ambient::prefs::kAmbientBackdropClientId,
                               std::string());

  // Do not sync across devices to allow different usages for different
  // devices.
  registry->RegisterBooleanPref(ash::ambient::prefs::kAmbientModeEnabled,
                                false);

  // Used to upload usage metrics. Derived from |AmbientSettings| when
  // settings are successfully saved by the user. This pref is not displayed
  // to the user.
  registry->RegisterIntegerPref(
      ash::ambient::prefs::kAmbientModePhotoSourcePref,
      static_cast<int>(ash::ambient::AmbientModePhotoSource::kUnset));

  // Used to control the number of seconds of inactivity on lock screen before
  // showing Ambient mode. This pref is not displayed to the user. Registered
  // as integer rather than TimeDelta to work with prefs_util.
  registry->RegisterIntegerPref(
      ambient::prefs::kAmbientModeLockScreenInactivityTimeoutSeconds,
      kLockScreenInactivityTimeout.InSeconds());

  // Used to control the number of seconds to lock the session after starting
  // Ambient mode. This pref is not displayed to the user. Registered as
  // integer rather than TimeDelta to work with prefs_util.
  registry->RegisterIntegerPref(
      ambient::prefs::kAmbientModeLockScreenBackgroundTimeoutSeconds,
      kLockScreenBackgroundTimeout.InSeconds());

  // Used to control the photo refresh interval in Ambient mode. This pref is
  // not displayed to the user. Registered as integer rather than TimeDelta to
  // work with prefs_util.
  registry->RegisterIntegerPref(
      ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds,
      kPhotoRefreshInterval.InSeconds());

  // |ambient::prefs::kAmbientTheme| is for legacy purposes only. It is being
  // migrated to |ambient::prefs::kAmbientUiSettings|, which is the newer
  // version of these settings.
  registry->RegisterIntegerPref(ambient::prefs::kAmbientTheme,
                                static_cast<int>(kDefaultAmbientTheme));
  registry->RegisterDictionaryPref(ambient::prefs::kAmbientUiSettings);

  registry->RegisterDoublePref(
      ambient::prefs::kAmbientModeAnimationPlaybackSpeed,
      kAnimationPlaybackSpeed);

  registry->RegisterBooleanPref(
      ash::ambient::prefs::kAmbientModeManagedScreensaverEnabled, false);

  registry->RegisterIntegerPref(
      ambient::prefs::kAmbientModeManagedScreensaverIdleTimeoutSeconds,
      kManagedScreensaverInactivityTimeout.InSeconds());

  registry->RegisterIntegerPref(
      ambient::prefs::kAmbientModeManagedScreensaverImageDisplayIntervalSeconds,
      kManagedScreensaverImageRefreshInterval.InSeconds());

  registry->RegisterIntegerPref(
      ambient::prefs::kAmbientModeRunningDurationMinutes, kDurationForever);
}

AmbientController::AmbientController(
    mojo::PendingRemote<device::mojom::Fingerprint> fingerprint)
    : ambient_weather_controller_(std::make_unique<AmbientWeatherController>(
          SimpleGeolocationProvider::GetInstance())),
      fingerprint_(std::move(fingerprint)) {
  ambient_photo_cache::SetFileTaskRunner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
  ambient_backend_controller_ = CreateAmbientBackendController();

  // |SessionController| is initialized before |this| in Shell. Necessary to
  // bind observer here to monitor |OnActiveUserPrefServiceChanged|.
  session_observer_.Observe(Shell::Get()->session_controller());
  backlights_forced_off_observation_.Observe(
      Shell::Get()->backlights_forced_off_setter());
}

AmbientController::~AmbientController() {
  SetUiVisibilityClosed(/*immediately=*/true);
}

void AmbientController::OnAmbientUiVisibilityChanged(
    AmbientUiVisibility visibility) {
  switch (visibility) {
    case AmbientUiVisibility::kShouldShow:
      // Cancels the timer upon shown.
      inactivity_timer_.Stop();

      if (IsChargerConnected()) {
        // Requires wake lock to prevent display from sleeping.
        AcquireWakeLock();
        StartTimerToReleaseWakeLock();
      }
      // Observes the |PowerStatus| on the battery charging status change for
      // the current ambient session.
      if (!power_status_observer_.IsObserving())
        power_status_observer_.Observe(PowerStatus::Get());

      MaybeStartScreenSaver();
      break;
    case AmbientUiVisibility::kPreview: {
      MaybeStartScreenSaver();
      break;
    }
    case AmbientUiVisibility::kHidden:
    case AmbientUiVisibility::kClosed: {
      // TODO(wutao): This will clear the image cache currently. It will not
      // work with `kHidden` if the token has expired and ambient mode is shown
      // again.
      StopScreensaver();

      // Should do nothing if the wake lock has already been released.
      ReleaseWakeLock();

      ClearPreTargetHandler();

      // Should stop observing AssistantInteractionModel when ambient screen is
      // not shown.
      AssistantInteractionController::Get()->GetModel()->RemoveObserver(this);

      if (visibility == AmbientUiVisibility::kHidden) {
        if (LockScreen::HasInstance()) {
          // Add observer for user activity.
          if (!user_activity_observer_.IsObserving())
            user_activity_observer_.Observe(ui::UserActivityDetector::Get());

          // Start timer to show ambient mode.
          inactivity_timer_.Start(
              FROM_HERE, ambient_ui_model_.lock_screen_inactivity_timeout(),
              base::BindOnce(&AmbientController::OnAutoShowTimeOut,
                             weak_ptr_factory_.GetWeakPtr()));
        }
      } else {
        DCHECK(visibility == AmbientUiVisibility::kClosed);
        inactivity_timer_.Stop();
        user_activity_observer_.Reset();
        power_status_observer_.Reset();
      }

      break;
    }
  }
}

void AmbientController::OnAutoShowTimeOut() {
  DCHECK(IsUiHidden(ambient_ui_model_.ui_visibility()));

  // Show ambient screen after time out.
  SetUiVisibilityShouldShow();
}

void AmbientController::OnLoginOrLockScreenCreated() {
  if (!ambient::util::IsShowing(LockScreen::ScreenType::kLogin)) {
    return;
  }
  OnLoginLockStateChanged(LockScreenState::kLogin);
}

void AmbientController::OnLockStateChanged(bool locked) {
  OnLoginLockStateChanged(locked ? LockScreenState::kLocked
                                 : LockScreenState::kUnlocked);
}

void AmbientController::OnLoginLockStateChanged(LockScreenState state) {
  if (state == LockScreenState::kUnlocked) {
    // Ambient screen will be destroyed along with the lock screen when user
    // logs in.
    SetUiVisibilityClosed();
    return;
  }

  if (!IsAmbientModeEnabled()) {
    VLOG(1) << "Ambient mode is not allowed.";
    return;
  }

  // Reset image failures to allow retrying ambient mode after lock state
  // changes.
  if (GetAmbientBackendModel()) {
    GetAmbientBackendModel()->ResetImageFailures();
  }

  // We have 3 options to manage the token for lock screen. Here use option 1.
  // 1. Request only one time after entering lock screen. We will use it once
  //    to request all the image links and no more requests.
  // 2. Request one time before entering lock screen. This will introduce
  //    extra latency.
  // 3. Request and refresh the token in the background (even the ambient mode
  //    is not started) with extra buffer time to use. When entering
  //    lock screen, it will be most likely to have the token already and
  //    enough time to use. More specifically,
  //    3a. We will leave enough buffer time (e.g. 10 mins before expire) to
  //        start to refresh the token.
  //    3b. When lock screen is triggered, most likely we will have >10 mins
  //        of token which can be used on lock screen.
  //    3c. There is a corner case that we may not have the token fetched when
  //        locking screen, we probably can use PrepareForLock(callback) when
  //        locking screen. We can add the refresh token into it. If the token
  //        has already been fetched, then there is not additional time to
  //        wait.
  RequestAccessToken(base::DoNothing(), /*may_refresh_token_on_lock=*/true);

  if (!IsShowing()) {
    // When lock screen starts, we don't immediately show the UI. The Ui is
    // hidden and will show after a delay.
    SetUiVisibilityHidden();
  }
}

AmbientController::LockScreenState AmbientController::GetLockScreenState() {
  if (!LockScreen::HasInstance()) {
    return LockScreenState::kUnlocked;
  }
  if (ambient::util::IsShowing(LockScreen::ScreenType::kLogin)) {
    return LockScreenState::kLogin;
  }
  return LockScreenState::kLocked;
}

void AmbientController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (GetPrimaryUserPrefService() != pref_service) {
    return;
  }

  // Do not continue if pref_change_registrar has already been set up. This
  // prevents re-binding observers when secondary profiles are activated.
  if (pref_change_registrar_)
    return;

  // Once logged in just remove the sign in pref registrations. So that we
  // don't react to device policy changes. Note: we do not need to re-add it
  // on logout because the chrome process is destroyed on logout.
  if (sign_in_pref_change_registrar_) {
    sign_in_pref_change_registrar_.reset();
  }

  bool ambient_mode_allowed = AmbientClient::Get()->IsAmbientModeAllowed();
  bool managed_screensaver_flag_enabled =
      ash::features::IsAmbientModeManagedScreensaverEnabled();

  if (ambient_mode_allowed || managed_screensaver_flag_enabled) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(pref_service);
  }

  if (ambient_mode_allowed) {
    pref_change_registrar_->Add(
        ambient::prefs::kAmbientModeEnabled,
        base::BindRepeating(&AmbientController::OnEnabledPrefChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  if (managed_screensaver_flag_enabled) {
    screensaver_images_policy_handler_ =
        ScreensaverImagesPolicyHandler::Create(pref_service);

    pref_change_registrar_->Add(
        ambient::prefs::kAmbientModeManagedScreensaverEnabled,
        base::BindRepeating(&AmbientController::OnEnabledPrefChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  if (ambient_mode_allowed || managed_screensaver_flag_enabled) {
    OnEnabledPrefChanged();
  }
}

void AmbientController::OnSigninScreenPrefServiceInitialized(
    PrefService* pref_service) {
  if (!ash::features::IsAmbientModeManagedScreensaverEnabled()) {
    return;
  }

  screensaver_images_policy_handler_ =
      ScreensaverImagesPolicyHandler::Create(pref_service);

  CHECK(!sign_in_pref_change_registrar_);
  CHECK(!pref_change_registrar_);
  sign_in_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  sign_in_pref_change_registrar_->Init(pref_service);

  sign_in_pref_change_registrar_->Add(
      ambient::prefs::kAmbientModeManagedScreensaverEnabled,
      base::BindRepeating(&AmbientController::OnEnabledPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  OnEnabledPrefChanged();
}

void AmbientController::OnPowerStatusChanged() {
  if (ambient_ui_model_.ui_visibility() != AmbientUiVisibility::kShouldShow) {
    // No action needed if ambient screen is not shown.
    return;
  }

  // TODO(b/300158227): There is a pending decision of whether we should
  // reacquire wake lock when the power is reconnected before screen saver
  // goes off. We make this change only to make sure that wake lock should
  // never be acquired while on battery.
  if (!IsChargerConnected()) {
    ReleaseWakeLock();
  }
}

void AmbientController::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& idle_state) {
  DVLOG(1) << "ScreenIdleStateChanged: dimmed(" << idle_state.dimmed()
           << ") off(" << idle_state.off() << ")";

  if (!IsAmbientModeEnabled())
    return;

  is_screen_off_ = idle_state.off();

  if (idle_state.off()) {
    DVLOG(1) << "Screen is off, close ambient mode.";

    SetUiVisibilityClosed(/*immediately=*/true);
    return;
  }

  if (idle_state.dimmed()) {
    // Do not show the UI if lockscreen is active. The inactivity monitor should
    // have activated ambient mode.
    if (LockScreen::HasInstance())
      return;

    // Do not show UI if loading images was unsuccessful.
    if (GetAmbientBackendModel() &&
        GetAmbientBackendModel()->ImageLoadingFailed()) {
      VLOG(1) << "Skipping ambient mode activation due to prior failure";
      GetAmbientBackendModel()->ResetImageFailures();
      return;
    }

    SetUiVisibilityShouldShow();
    return;
  }

  if (LockScreen::HasInstance() &&
      ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kClosed) {
    // Restart hidden ui if the screen is back on and lockscreen is shown.
    SetUiVisibilityHidden();
  }
}

void AmbientController::OnBacklightsForcedOffChanged(bool forced_off) {
  if (forced_off) {
    SetUiVisibilityClosed(/*immediately=*/true);
  }
  if (!forced_off && LockScreen::HasInstance() &&
      ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kClosed) {
    // Restart hidden ui if the screen is back on and lockscreen is shown.
    SetUiVisibilityHidden();
  }
}

void AmbientController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  // If about to suspend, turn everything off. This covers:
  //   1. Clicking power button.
  //   2. Close lid.
  // Need to specially close the widget immediately here to be able to close
  // the UI before device goes to suspend. Otherwise when opening lid after
  // lid closed, there may be a flash of the old window before previous
  // closing finished.
  SetUiVisibilityClosed(/*immediately=*/true);
  is_suspend_imminent_ = true;
}

void AmbientController::SuspendDone(base::TimeDelta sleep_duration) {
  is_suspend_imminent_ = false;
  // |DismissUI| will restart the lock screen timer if lock screen is active and
  // if Ambient mode is enabled, so call it when resuming from suspend to
  // restart Ambient mode if applicable.
  DismissUI();
}

void AmbientController::OnAuthScanDone(
    const device::mojom::FingerprintMessagePtr msg,
    const base::flat_map<std::string, std::vector<std::string>>& matches) {
  DismissUI();
}

void AmbientController::OnUserActivity(const ui::Event* event) {
  // The following events are handled separately so that we can consume them.
  // In case events come from external sources (i.e. Chrome extensions), the
  // event will be nullptr.
  if (is_receiving_pretarget_events_ && event &&
      (event->IsMouseEvent() || event->IsTouchEvent() || event->IsKeyEvent() ||
       event->IsFlingScrollEvent())) {
    return;
  }
  // While |kPreview| is loading, don't |DismissUI| on user activity.
  // Users can still |DismissUI| with mouse, touch, key or assistant events.
  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kPreview &&
      !IsShowing()) {
    return;
  }
  DismissUI();
}

void AmbientController::OnKeyEvent(ui::KeyEvent* event) {
  // Prevent dispatching key press event to the login UI.
  MaybeStopUiEventPropagation(event);
  // |DismissUI| only on |EventType::kKeyPressed|. Otherwise it won't be
  // possible to start the preview by pressing "enter" key. It'll be cancelled
  // immediately on |EventType::kKeyReleased|.
  if (event->type() == ui::EventType::kKeyPressed) {
    DismissUI();
  }
}

void AmbientController::OnMouseEvent(ui::MouseEvent* event) {
  // |DismissUI| on actual mouse move only if the screen saver widget is shown
  // (images are downloaded).
  if (event->type() == ui::EventType::kMouseMoved) {
    MaybeDismissUIOnMouseMove();
    last_mouse_event_was_move_ = true;
    return;
  }

  // Prevent dispatching mouse event to the windows behind screen saver.
  // Let move event pass through, so that it clears hover states.
  MaybeStopUiEventPropagation(event);
  if (event->IsAnyButton()) {
    DismissUI();
  }
  last_mouse_event_was_move_ = false;
}

void AmbientController::OnTouchEvent(ui::TouchEvent* event) {
  // Prevent dispatching touch event to the windows behind screen saver.
  MaybeStopUiEventPropagation(event);
  DismissUI();
}

void AmbientController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state == InteractionState::kActive) {
    // Assistant is active.
    DismissUI();
  }
}

void AmbientController::SetUiVisibilityShouldShow() {
  DVLOG(1) << __func__;

  // TODO(meilinw): move the eligibility check to the idle entry point once
  // implemented: b/149246117.
  if (!IsAmbientModeEnabled()) {
    LOG(WARNING) << "Ambient mode is not allowed.";
    return;
  }

  if (is_suspend_imminent_) {
    VLOG(1) << "Do not show UI when suspend imminent";
    return;
  }

  // If the ambient ui launcher is not ready to be started then do not change
  // the visibility. This will disabled the ui launcher until the next AmbientUi
  // starting event occurs. Right now the only ambient ui starting events are
  // screen lock/unlock, screen dim, preview and screen backlight off.
  if (ambient_ui_launcher_ && !ambient_ui_launcher_->IsReady()) {
    return;
  }

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kShouldShow);
}

void AmbientController::SetUiVisibilityPreview() {
  if (!IsAmbientModeEnabled()) {
    LOG(WARNING) << "Ambient mode is not allowed.";
    return;
  }

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kPreview);
  base::RecordAction(base::UserMetricsAction(kScreenSaverPreviewUserAction));
}

void AmbientController::SetUiVisibilityHidden() {
  DVLOG(1) << __func__;

  if (!IsAmbientModeEnabled()) {
    LOG(WARNING) << "Ambient mode is not allowed.";
    return;
  }

  if (is_suspend_imminent_) {
    VLOG(1) << "Do not start hidden UI when suspend imminent";
    return;
  }

  if (is_screen_off_) {
    VLOG(1) << "Do not start hidden UI when screen is off";
    return;
  }

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kHidden);
}

void AmbientController::SetUiVisibilityClosed(bool immediately) {
  DVLOG(1) << __func__;
  // Early return if the UI is already closed to make sure we do not change the
  // cursor visibility when it is not required.
  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kClosed) {
    return;
  }

  close_widgets_immediately_ = immediately;
  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kClosed);
  if (!Shell::Get()->IsInTabletMode()) {
    Shell::Get()->cursor_manager()->ShowCursor();
  }
}

void AmbientController::SetScreenSaverDuration(int minutes) {
  auto* pref_service = GetPrimaryUserPrefService();
  if (!pref_service) {
    return;
  }
  pref_service->Set(ambient::prefs::kAmbientModeRunningDurationMinutes,
                    base::Value(minutes));
}

void AmbientController::StartTimerToReleaseWakeLock() {
  CHECK(!screensaver_running_timer_.IsRunning());

  auto* pref_service = GetPrimaryUserPrefService();
  if (!pref_service) {
    return;
  }

  const int session_duration_in_minutes = pref_service->GetInteger(
      ambient::prefs::kAmbientModeRunningDurationMinutes);
  CHECK(session_duration_in_minutes >= 0);

  if (session_duration_in_minutes != kDurationForever) {
    const base::TimeDelta delay = base::Minutes(session_duration_in_minutes);
    screensaver_running_timer_.Start(FROM_HERE, delay, this,
                                     &AmbientController::ReleaseWakeLock);
  }
}

bool AmbientController::ShouldShowAmbientUi() const {
  return ambient_ui_model_.ui_visibility() ==
             AmbientUiVisibility::kShouldShow ||
         ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kPreview;
}

bool AmbientController::IsShowing() const {
  const std::vector<RootWindowController*> root_window_controllers =
      RootWindowController::root_window_controllers();

  const bool has_at_least_one_widget = std::any_of(
      root_window_controllers.cbegin(), root_window_controllers.cend(),
      [](const RootWindowController* const controller) {
        return controller->HasAmbientWidget();
      });

#if DCHECK_IS_ON()
  if (!ShouldShowAmbientUi()) {
    DCHECK(!has_at_least_one_widget);
  }
#endif  // DCHECK_IS_ON()

  return has_at_least_one_widget;
}

void AmbientController::AcquireWakeLock() {
  if (!wake_lock_) {
    mojo::Remote<device::mojom::WakeLockProvider> provider;
    AmbientClient::Get()->RequestWakeLockProvider(
        provider.BindNewPipeAndPassReceiver());
    provider->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventDisplaySleep,
        device::mojom::WakeLockReason::kOther, kWakeLockReason,
        wake_lock_.BindNewPipeAndPassReceiver());
  }

  DCHECK(wake_lock_);
  wake_lock_->RequestWakeLock();
  VLOG(1) << "Acquired wake lock";

  auto* session_controller = Shell::Get()->session_controller();
  if (session_controller->CanLockScreen() &&
      session_controller->ShouldLockScreenAutomatically()) {
    if (!session_controller->IsScreenLocked() &&
        !delayed_lock_timer_.IsRunning()) {
      delayed_lock_timer_.Start(
          FROM_HERE, ambient_ui_model_.background_lock_screen_timeout(),
          base::BindOnce(
              []() { Shell::Get()->session_controller()->LockScreen(); }));
    }
  }
}

void AmbientController::ReleaseWakeLock() {
  if (!wake_lock_)
    return;

  wake_lock_->CancelWakeLock();
  VLOG(1) << "Released wake lock";

  delayed_lock_timer_.Stop();
  screensaver_running_timer_.Stop();
}

void AmbientController::CloseAllWidgets(bool immediately) {
  for (auto* root_window_controller :
       RootWindowController::root_window_controllers()) {
    root_window_controller->CloseAmbientWidget(immediately);
  }
}

void AmbientController::SetUpPreTargetHandler() {
  if (!is_receiving_pretarget_events_) {
    Shell::Get()->AddPreTargetHandler(this);
    is_receiving_pretarget_events_ = true;
  }
}

void AmbientController::ClearPreTargetHandler() {
  if (is_receiving_pretarget_events_) {
    Shell::Get()->RemovePreTargetHandler(this);
    is_receiving_pretarget_events_ = false;
  }
}

PrefChangeRegistrar* AmbientController::GetActivePrefChangeRegistrar() {
  if (pref_change_registrar_) {
    return pref_change_registrar_.get();
  }

  if (ash::features::IsAmbientModeManagedScreensaverEnabled()) {
    return sign_in_pref_change_registrar_.get();
  }
  return nullptr;
}

void AmbientController::AddManagedScreensaverPolicyPrefObservers() {
  PrefChangeRegistrar* registrar = GetActivePrefChangeRegistrar();
  CHECK(registrar);
  registrar->Add(
      ambient::prefs::kAmbientModeManagedScreensaverIdleTimeoutSeconds,
      base::BindRepeating(
          &AmbientController::
              OnManagedScreensaverLockScreenIdleTimeoutPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  registrar->Add(
      ambient::prefs::kAmbientModeManagedScreensaverImageDisplayIntervalSeconds,
      base::BindRepeating(
          &AmbientController::
              OnManagedScreensaverPhotoRefreshIntervalPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  OnManagedScreensaverLockScreenIdleTimeoutPrefChanged();
  OnManagedScreensaverPhotoRefreshIntervalPrefChanged();
}

void AmbientController::RemoveAmbientModeSettingsPrefObservers() {
  for (const auto* pref_name :
       {ambient::prefs::kAmbientModeLockScreenBackgroundTimeoutSeconds,
        ambient::prefs::kAmbientModeLockScreenInactivityTimeoutSeconds,
        ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds,
        ambient::prefs::kAmbientUiSettings,
        ambient::prefs::kAmbientModeAnimationPlaybackSpeed,
        ambient::prefs::kAmbientModeManagedScreensaverIdleTimeoutSeconds,
        ambient::prefs::
            kAmbientModeManagedScreensaverImageDisplayIntervalSeconds}) {
    if (pref_change_registrar_ &&
        pref_change_registrar_->IsObserved(pref_name)) {
      pref_change_registrar_->Remove(pref_name);
    }
    if (sign_in_pref_change_registrar_ &&
        sign_in_pref_change_registrar_->IsObserved(pref_name)) {
      sign_in_pref_change_registrar_->Remove(pref_name);
    }
  }
}

void AmbientController::OnManagedScreensaverLockScreenIdleTimeoutPrefChanged() {
  PrefService* pref_service = GetActivePrefService();
  CHECK(pref_service);
  ambient_ui_model_.SetLockScreenInactivityTimeout(
      base::Seconds(pref_service->GetInteger(
          ambient::prefs::kAmbientModeManagedScreensaverIdleTimeoutSeconds)));
}

void AmbientController::OnManagedScreensaverPhotoRefreshIntervalPrefChanged() {
  PrefService* pref_service = GetActivePrefService();
  CHECK(pref_service);
  ambient_ui_model_.SetPhotoRefreshInterval(
      base::Seconds(pref_service->GetInteger(
          ambient::prefs::
              kAmbientModeManagedScreensaverImageDisplayIntervalSeconds)));
}

void AmbientController::AddConsumerPrefObservers() {
  // Note: in case we ever want to enable the consumer screensaver on the
  // login screen we should change the pref_change_registrar here with
  // `GetActivePrefChangeRegistrar()` and the corresponding
  // `GetPrimaryUserPrefService()` with `GetActivePrefService()` in the actual
  // method calls.
  if (!pref_change_registrar_) {
    return;
  }

  pref_change_registrar_->Add(
      ambient::prefs::kAmbientModeLockScreenInactivityTimeoutSeconds,
      base::BindRepeating(
          &AmbientController::OnLockScreenInactivityTimeoutPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  pref_change_registrar_->Add(
      ambient::prefs::kAmbientModeLockScreenBackgroundTimeoutSeconds,
      base::BindRepeating(
          &AmbientController::OnLockScreenBackgroundTimeoutPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));

  pref_change_registrar_->Add(
      ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds,
      base::BindRepeating(&AmbientController::OnPhotoRefreshIntervalPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  pref_change_registrar_->Add(
      ambient::prefs::kAmbientUiSettings,
      base::BindRepeating(&AmbientController::OnAmbientUiSettingsChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  pref_change_registrar_->Add(
      ambient::prefs::kAmbientModeAnimationPlaybackSpeed,
      base::BindRepeating(&AmbientController::OnAnimationPlaybackSpeedChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  // Trigger the callbacks manually the first time to init AmbientUiModel.
  OnLockScreenInactivityTimeoutPrefChanged();
  OnLockScreenBackgroundTimeoutPrefChanged();
  OnPhotoRefreshIntervalPrefChanged();
  OnAnimationPlaybackSpeedChanged();
}

void AmbientController::OnEnabledPrefChanged() {
  RecordManagedScreensaverEnabledPref();

  if (!IsAmbientModeEnabled()) {
    DVLOG(1) << "Ambient mode disabled";
    ResetAmbientControllerResources();
    return;
  }

  DVLOG(1) << "Ambient mode enabled";

  // A second initialization can happen in the following cases:
  // 1) Ambient mode is enabled for the login screen via device policy on a
  // managed device (first initialization), A consumer user with an email with
  // @gmail.com logins into the device and has ambient mode enabled (Second
  // initialization).
  //
  // 2) Ambient mode is enabled for the login screen via device policy on a
  // managed device (first initialization), A managed user logins into the
  // device and the managed screensaver is enabled via user policy. (Second
  // initialization).

  if (is_initialized_) {
    // In case the mode is initialized we reset and start from a clean slate so
    // that we do not double allocate everything and always listen to the
    // correct prefs.
    // Note: We do not early return here as multiple calls to this method are
    // valid and depending upon the type of ambient mode being enabled we have
    // to do different things.
    ResetAmbientControllerResources();
  }
  is_initialized_ = true;

  if (IsAmbientModeManagedScreensaverEnabled()) {
    AddManagedScreensaverPolicyPrefObservers();
  } else {
    AddConsumerPrefObservers();
  }

  CreateUiLauncher();

  ambient_ui_model_observer_.Observe(&ambient_ui_model_);
  auto* power_manager_client = chromeos::PowerManagerClient::Get();
  DCHECK(power_manager_client);
  power_manager_client_observer_.Observe(power_manager_client);

  fingerprint_->AddFingerprintObserver(
      fingerprint_observer_receiver_.BindNewPipeAndPassRemote());

  // The policy update can happen on the login screen as well so we need to
  // trigger the state change to start the ambient mode if required.
  if (IsAmbientModeManagedScreensaverEnabled()) {
    OnLoginLockStateChanged(GetLockScreenState());
  }
}

void AmbientController::ResetAmbientControllerResources() {
  SetUiVisibilityClosed();

  RemoveAmbientModeSettingsPrefObservers();

  ambient_ui_model_observer_.Reset();
  power_manager_client_observer_.Reset();

  DestroyUiLauncher();

  if (fingerprint_observer_receiver_.is_bound()) {
    fingerprint_observer_receiver_.reset();
  }
  is_initialized_ = false;
}

void AmbientController::OnLockScreenInactivityTimeoutPrefChanged() {
  auto* pref_service = GetPrimaryUserPrefService();
  if (!pref_service)
    return;

  ambient_ui_model_.SetLockScreenInactivityTimeout(
      base::Seconds(pref_service->GetInteger(
          ambient::prefs::kAmbientModeLockScreenInactivityTimeoutSeconds)));
}

void AmbientController::OnLockScreenBackgroundTimeoutPrefChanged() {
  auto* pref_service = GetPrimaryUserPrefService();
  if (!pref_service)
    return;

  ambient_ui_model_.SetBackgroundLockScreenTimeout(
      base::Seconds(pref_service->GetInteger(
          ambient::prefs::kAmbientModeLockScreenBackgroundTimeoutSeconds)));
}

void AmbientController::OnPhotoRefreshIntervalPrefChanged() {
  auto* pref_service = GetPrimaryUserPrefService();
  if (!pref_service)
    return;

  ambient_ui_model_.SetPhotoRefreshInterval(
      base::Seconds(pref_service->GetInteger(
          ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds)));
}

void AmbientController::OnAmbientUiSettingsChanged() {
  DVLOG(4) << "AmbientUiSettings changed to "
           << GetCurrentUiSettings().ToString();
  // For a given topic category, the topics downloaded from IMAX and saved to
  // cache differ from theme to theme:
  // 1) Slideshow mode keeps primary/related photos paired within a topic,
  //    whereas animated themes split the photos into 2 separate topics.
  // 2) The resolution of the photos downloaded from FIFE may differ between
  //    themes, depending on the image assets' sizes in the animation file.
  // For this reason, it is better to not re-use the cache when switching
  // between themes.
  //
  // There are corner cases here where the theme may change and the program
  // crashes before the cache gets cleared below. This is intentionally not
  // accounted for because it's not worth the added complexity. If this
  // should happen, re-using the cache will still work without fatal behavior.
  // The UI may just not be optimal. Furthermore, the cache gradually gets
  // overwritten with topics reflecting the new theme anyways, so ambient mode
  // should not be stuck with a mismatched cache indefinitely.
  ambient_photo_cache::Clear(ambient_photo_cache::Store::kPrimary);

  // The |AmbientUiLauncher| implementation to use is largely dependent on
  // the current |AmbientUiSettings|, so this needs to be recreated.
  CreateUiLauncher();
}

void AmbientController::OnAnimationPlaybackSpeedChanged() {
  DCHECK(GetPrimaryUserPrefService());
  ambient_ui_model_.set_animation_playback_speed(
      GetPrimaryUserPrefService()->GetDouble(
          ambient::prefs::kAmbientModeAnimationPlaybackSpeed));
}

void AmbientController::RequestAccessToken(
    AmbientAccessTokenController::AccessTokenCallback callback,
    bool may_refresh_token_on_lock) {
  // Do not request access tokens when the ambient mode is in the managed mode
  // as we do not want to rely on any user information .
  if (IsAmbientModeManagedScreensaverEnabled()) {
    // Consume the callback to be resilient against dependencies on the callback
    // in the future.
    std::move(callback).Run("", "");
    return;
  }
  access_token_controller_.RequestAccessToken(std::move(callback),
                                              may_refresh_token_on_lock);
}

void AmbientController::DismissUI() {
  // Call `ClearPreTargetHandler` immediately so that `OnKeyEvent` has no
  // chance of being called and consuming the keypress.
  ClearPreTargetHandler();

  if (!IsAmbientModeEnabled()) {
    SetUiVisibilityClosed();
    return;
  }

  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kHidden) {
    // Double resetting crashes the UI, make sure it is running.
    if (inactivity_timer_.IsRunning()) {
      inactivity_timer_.Reset();
    }
    return;
  }

  if (LockScreen::HasInstance()) {
    SetUiVisibilityHidden();
    return;
  }

  SetUiVisibilityClosed();
}

AmbientBackendModel* AmbientController::GetAmbientBackendModel() {
  // This can legitimately be null. Some ambient UIs do not use photos at all
  // and hence, do not have an active |AmbientBackendModel|.
  // TODO(b/274164306): Move |AmbientBackendModel| references completely out
  // of |AmbientController|. The business logic should be migrated elsewhere
  // (likely somewhere within an |AmbientUiLauncher| implementation).
  return ambient_ui_launcher_->GetAmbientBackendModel();
}

AmbientWeatherModel* AmbientController::GetAmbientWeatherModel() {
  return ambient_weather_controller_->weather_model();
}

std::unique_ptr<views::Widget> AmbientController::CreateWidget(
    aura::Window* container) {
  if (ui_launcher_state_ != AmbientUiLauncherState::kRendering) {
    return nullptr;
  }

  CHECK(session_metrics_recorder_);
  session_metrics_recorder_->RegisterScreen();
  std::unique_ptr<AmbientContainerView> container_view;
  container_view = std::make_unique<AmbientContainerView>(
      GetCurrentUiSettings(), ambient_ui_launcher_->CreateView());
  auto* widget_delegate = new AmbientWidgetDelegate();
  widget_delegate->SetInitiallyFocusedView(container_view.get());

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = GetWidgetName();
  params.show_state = ui::mojom::WindowShowState::kFullscreen;
  params.parent = container;
  params.delegate = widget_delegate;
  params.visible_on_all_workspaces = true;

  // Do not change the video wake lock.
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  auto* contents_view = widget->SetContentsView(std::move(container_view));

  widget->SetVisibilityAnimationTransition(
      views::Widget::VisibilityTransition::ANIMATE_BOTH);
  ::wm::SetWindowVisibilityAnimationType(
      widget->GetNativeWindow(), ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ::wm::SetWindowVisibilityChangesAnimated(widget->GetNativeWindow());

  widget->Show();

  // Only announce for the primary window.
  if (Shell::GetPrimaryRootWindow() == container->GetRootWindow()) {
    contents_view->GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_ASH_SCREENSAVER_STARTS));
  }

  return widget;
}

void AmbientController::OnUiLauncherInitialized(bool success) {
  CHECK(session_metrics_recorder_);
  session_metrics_recorder_->SetInitStatus(success);
  if (!success) {
    // Success = false denotes a case where the screensaver is in a permanent
    // error state and such that the UI and any further attempts to launch the
    // UI will also result in this failure.
    // TODO (b/175142676) Add metrics for cases where success = false.
    LOG(ERROR) << "AmbientUiLauncher failed to initialize";
    SetUiVisibilityClosed();
    return;
  }
  ui_launcher_state_ = AmbientUiLauncherState::kRendering;
  CreateAndShowWidgets();
}

void AmbientController::CreateAndShowWidgets() {
  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kPreview) {
    preview_widget_created_at_ = base::Time::Now();
  }
  // Hide cursor.
  Shell::Get()->cursor_manager()->HideCursor();
  for (auto* root_window_controller :
       RootWindowController::root_window_controllers()) {
    root_window_controller->CreateAmbientWidget();
  }
}

void AmbientController::StopScreensaver() {
  CloseAllWidgets(close_widgets_immediately_);
  session_metrics_recorder_.reset();
  ui_launcher_init_callback_.Cancel();
  ui_launcher_state_ = AmbientUiLauncherState::kInactive;
  ambient_ui_launcher_->Finalize();
}

void AmbientController::MaybeStartScreenSaver() {
  // The screensaver may have already been started.
  if (IsUiLauncherActive()) {
    return;
  }

  if (!user_activity_observer_.IsObserving())
    user_activity_observer_.Observe(ui::UserActivityDetector::Get());

  // Add observer for assistant interaction model
  AssistantInteractionController::Get()->GetModel()->AddObserver(this);

  session_metrics_recorder_ = std::make_unique<AmbientSessionMetricsRecorder>(
      ambient_ui_launcher_->CreateMetricsDelegate(GetCurrentUiSettings()));

  SetUpPreTargetHandler();

  ui_launcher_init_callback_.Reset(
      base::BindOnce(&AmbientController::OnUiLauncherInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
  ui_launcher_state_ = AmbientUiLauncherState::kInitializing;
  ambient_ui_launcher_->Initialize(ui_launcher_init_callback_.callback());
}

AmbientUiSettings AmbientController::GetCurrentUiSettings() const {
  CHECK(GetActivePrefService());
  return AmbientUiSettings::ReadFromPrefService(*GetActivePrefService());
}

void AmbientController::MaybeDismissUIOnMouseMove() {
  // If the move was not an actual mouse move event or the screen saver widget
  // is not shown yet (images are not downloaded), don't dismiss.
  if (!last_mouse_event_was_move_ || !IsShowing()) {
    return;
  }

  // In preview mode, don't dismiss until the timer stops running (avoids
  // accidental dismissal).
  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kPreview) {
    auto elapsed = base::Time::Now() - preview_widget_created_at_;
    if (elapsed < kDismissPreviewOnMouseMoveDelay) {
      return;
    }
  }
  DismissUI();
}

void AmbientController::CreateUiLauncher() {
  if (IsUiLauncherActive()) {
    // There are no known use cases where the AmbientUiSettings selected by the
    // user can change while in the middle of an ambient session, but this is
    // handled gracefully just in case.
    LOG(DFATAL) << "Cannot reset the AmbientUiLauncher while it is active";
    return;
  }

  DestroyUiLauncher();

  if (IsAmbientModeManagedScreensaverEnabled()) {
    ambient_ui_launcher_ = std::make_unique<AmbientManagedSlideshowUiLauncher>(
        &delegate_, screensaver_images_policy_handler_.get());
  } else {
    switch (GetCurrentUiSettings().theme()) {
      case personalization_app::mojom::AmbientTheme::kSlideshow:
        ambient_ui_launcher_ =
            std::make_unique<AmbientSlideshowUiLauncher>(&delegate_);
        break;
      case personalization_app::mojom::AmbientTheme::kFeelTheBreeze:
      case personalization_app::mojom::AmbientTheme::kFloatOnBy:
        ambient_ui_launcher_ = std::make_unique<AmbientAnimationUiLauncher>(
            GetCurrentUiSettings(), &delegate_);
        break;
      case personalization_app::mojom::AmbientTheme::kVideo:
        ambient_ui_launcher_ = std::make_unique<AmbientVideoUiLauncher>(
            GetPrimaryUserPrefService(), &delegate_);
        break;
    }
  }

  ambient_ui_launcher_->SetObserver(this);
}

void AmbientController::DestroyUiLauncher() {
  ui_launcher_state_ = AmbientUiLauncherState::kInactive;
  ambient_ui_launcher_.reset();
}

bool AmbientController::IsUiLauncherActive() const {
  return ui_launcher_state_ != AmbientUiLauncherState::kInactive;
}

void AmbientController::OnReadyStateChanged(bool is_ready) {
  if (!is_ready) {
    // Close the UI if the launcher isn't ready. This is done so that we can
    // stop the current ui launcher session and prevent screenburn.
    SetUiVisibilityClosed();
    return;
  }
  // In case the ready state changes on the login/lock screen we should re-show
  // the ambient mode.
  OnLoginLockStateChanged(GetLockScreenState());
}

void AmbientController::MaybeStopUiEventPropagation(ui::Event* event) {
  // If ambient resources are still be loading and the UI has not started
  // rendering yet (which is usually just a few seconds), UI events such as
  // key presses should still be propagated to the current UI (ex: the lock
  // screen).
  if (IsShowing()) {
    event->StopPropagation();
  }
}

}  // namespace ash
