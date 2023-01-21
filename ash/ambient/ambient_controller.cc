// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_weather_controller.h"
#include "ash/ambient/metrics/ambient_multi_screen_metrics_recorder.h"
#include "ash/ambient/model/ambient_animation_photo_config.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
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
#include "ash/public/cpp/ambient/ambient_metrics.h"
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
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/buildflag.h"
#include "cc/paint/skottie_wrapper.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
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
    // If battery is full or battery is charging, that implies power is
    // connected. Also return true if a power source is connected and
    // battery is not discharging.
    return power_status->IsBatteryCharging() ||
           (power_status->IsLinePowerConnected() &&
            power_status->GetBatteryPercent() > 95.f);
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

bool IsAmbientModeEnabled() {
  if (!AmbientClient::Get()->IsAmbientModeAllowed())
    return false;

  auto* pref_service = GetPrimaryUserPrefService();
  return pref_service &&
         pref_service->GetBoolean(ambient::prefs::kAmbientModeEnabled);
}

class AmbientWidgetDelegate : public views::WidgetDelegate {
 public:
  AmbientWidgetDelegate() {
    SetCanMaximize(true);
    SetOwnedByWidget(true);
  }
};

}  // namespace

// static
void AmbientController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  if (features::IsAmbientModeEnabled()) {
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

    registry->RegisterIntegerPref(
        ambient::prefs::kAmbientAnimationTheme,
        static_cast<int>(kDefaultAmbientAnimationTheme));

    registry->RegisterDoublePref(
        ambient::prefs::kAmbientModeAnimationPlaybackSpeed,
        kAnimationPlaybackSpeed);
  }
}

AmbientController::AmbientController(
    mojo::PendingRemote<device::mojom::Fingerprint> fingerprint)
    : ambient_weather_controller_(std::make_unique<AmbientWeatherController>()),
      fingerprint_(std::move(fingerprint)) {
  ambient_backend_controller_ = CreateAmbientBackendController();

  // |SessionController| is initialized before |this| in Shell. Necessary to
  // bind observer here to monitor |OnActiveUserPrefServiceChanged|.
  session_observer_.Observe(Shell::Get()->session_controller());
  backlights_forced_off_observation_.Observe(
      Shell::Get()->backlights_forced_off_setter());
}

AmbientController::~AmbientController() {
  CloseUi(/*immediately=*/true);
}

void AmbientController::OnAmbientUiVisibilityChanged(
    AmbientUiVisibility visibility) {
  switch (visibility) {
    case AmbientUiVisibility::kShown:
      // Record metrics on ambient mode usage.
      ambient::RecordAmbientModeActivation(
          /*ui_mode=*/LockScreen::HasInstance() ? AmbientUiMode::kLockScreenUi
                                                : AmbientUiMode::kInSessionUi,
          /*tablet_mode=*/Shell::Get()->IsInTabletMode());

      DCHECK(!start_time_);
      start_time_ = base::Time::Now();

      // Cancels the timer upon shown.
      inactivity_timer_.Stop();

      if (IsChargerConnected()) {
        // Requires wake lock to prevent display from sleeping.
        AcquireWakeLock();
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
      bool ambient_ui_was_rendering =
          Shell::GetPrimaryRootWindowController()->HasAmbientWidget();
      CloseAllWidgets(close_widgets_immediately_);

      // TODO(wutao): This will clear the image cache currently. It will not
      // work with `kHidden` if the token has expired and ambient mode is shown
      // again.
      StopRefreshingImages();

      // Should do nothing if the wake lock has already been released.
      ReleaseWakeLock();

      Shell::Get()->RemovePreTargetHandler(this);

      // Should stop observing AssistantInteractionModel when ambient screen is
      // not shown.
      AssistantInteractionController::Get()->GetModel()->RemoveObserver(this);

      frame_rate_controller_.reset();
      multi_screen_metrics_recorder_.reset();

      // |start_time_| may be empty in case of |AmbientUiVisibility::kHidden| if
      // ambient mode has just started.
      if (start_time_) {
        auto elapsed = base::Time::Now() - start_time_.value();
        AmbientAnimationTheme theme = GetCurrentTheme();
        DVLOG(2) << "Exit ambient mode. Elapsed time: " << elapsed;
        ambient::RecordAmbientModeTimeElapsed(
            elapsed, Shell::Get()->IsInTabletMode(), theme);

        if (!ambient_ui_was_rendering &&
            elapsed >= ambient::kMetricsStartupTimeMax) {
          LOG(ERROR) << "Ambient UI completely failed to start";
          ambient::RecordAmbientModeStartupTime(elapsed, theme);
        }

        start_time_.reset();
      }

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
  ShowUi();
}

void AmbientController::OnLockStateChanged(bool locked) {
  if (!locked) {
    // Ambient screen will be destroyed along with the lock screen when user
    // logs in.
    CloseUi();
    return;
  }

  if (!IsAmbientModeEnabled()) {
    VLOG(1) << "Ambient mode is not allowed.";
    return;
  }

  // Reset image failures to allow retrying ambient mode after lock state
  // changes.
  GetAmbientBackendModel()->ResetImageFailures();

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

  if (!IsShown()) {
    // When lock screen starts, we don't immediately show the UI. The Ui is
    // hidden and will show after a delay.
    ShowHiddenUi();
  }
}

void AmbientController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (!AmbientClient::Get()->IsAmbientModeAllowed() ||
      GetPrimaryUserPrefService() != pref_service) {
    return;
  }

  // Do not continue if pref_change_registrar has already been set up. This
  // prevents re-binding observers when secondary profiles are activated.
  if (pref_change_registrar_)
    return;

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);

  pref_change_registrar_->Add(
      ambient::prefs::kAmbientModeEnabled,
      base::BindRepeating(&AmbientController::OnEnabledPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  OnEnabledPrefChanged();
}

void AmbientController::OnPowerStatusChanged() {
  if (ambient_ui_model_.ui_visibility() != AmbientUiVisibility::kShown) {
    // No action needed if ambient screen is not shown.
    return;
  }

  if (IsChargerConnected()) {
    AcquireWakeLock();
  } else {
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

    CloseUi(/*immediately=*/true);
    return;
  }

  if (idle_state.dimmed()) {
    // Do not show the UI if lockscreen is active. The inactivity monitor should
    // have activated ambient mode.
    if (LockScreen::HasInstance())
      return;

    // Do not show UI if loading images was unsuccessful.
    if (GetAmbientBackendModel()->ImageLoadingFailed()) {
      VLOG(1) << "Skipping ambient mode activation due to prior failure";
      GetAmbientBackendModel()->ResetImageFailures();
      return;
    }

    ShowUi();
    return;
  }

  if (LockScreen::HasInstance() &&
      ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kClosed) {
    // Restart hidden ui if the screen is back on and lockscreen is shown.
    ShowHiddenUi();
  }
}

void AmbientController::OnBacklightsForcedOffChanged(bool forced_off) {
  if (forced_off) {
    CloseUi(/*immediately=*/true);
  }
  if (!forced_off && LockScreen::HasInstance() &&
      ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kClosed) {
    // Restart hidden ui if the screen is back on and lockscreen is shown.
    ShowHiddenUi();
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
  CloseUi(/*immediately=*/true);
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
  if (event->IsMouseEvent() || event->IsTouchEvent() || event->IsKeyEvent() ||
      event->IsFlingScrollEvent()) {
    return;
  }
  // While |kPreview| is loading, don't |DismissUI| on user activity.
  // Users can still |DismissUI| with mouse, touch, key or assistant events.
  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kPreview &&
      !Shell::GetPrimaryRootWindowController()->HasAmbientWidget()) {
    return;
  }
  DismissUI();
}

void AmbientController::OnKeyEvent(ui::KeyEvent* event) {
  // Prevent dispatching key press event to the login UI.
  event->StopPropagation();
  // |DismissUI| only on |ET_KEY_PRESSED|. Otherwise it won't be possible to
  // start the preview by pressing "enter" key. It'll be cancelled immediately
  // on |ET_KEY_RELEASED|.
  if (event->type() == ui::ET_KEY_PRESSED) {
    DismissUI();
  }
}

void AmbientController::OnMouseEvent(ui::MouseEvent* event) {
  // Prevent dispatching mouse event to the windows behind screen saver.
  event->StopPropagation();
  // |DismissUI| on actual mouse move only if the screen saver widget is shown
  // (images are downloaded).
  if (event->type() == ui::ET_MOUSE_MOVED) {
    if (last_mouse_event_was_move_ &&
        Shell::GetPrimaryRootWindowController()->HasAmbientWidget()) {
      DismissUI();
    }
    last_mouse_event_was_move_ = true;
    return;
  }

  if (event->IsAnyButton()) {
    DismissUI();
  }
  last_mouse_event_was_move_ = false;
}

void AmbientController::OnTouchEvent(ui::TouchEvent* event) {
  // Prevent dispatching touch event to the windows behind screen saver.
  event->StopPropagation();
  DismissUI();
}

void AmbientController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state == InteractionState::kActive) {
    // Assistant is active.
    DismissUI();
  }
}

void AmbientController::ShowUi() {
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

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kShown);
}

void AmbientController::StartScreenSaverPreview() {
  if (!IsAmbientModeEnabled()) {
    LOG(WARNING) << "Ambient mode is not allowed.";
    return;
  }

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kPreview);
}

void AmbientController::ShowHiddenUi() {
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

void AmbientController::CloseUi(bool immediately) {
  DVLOG(1) << __func__;

  close_widgets_immediately_ = immediately;
  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kClosed);
}

void AmbientController::ToggleInSessionUi() {
  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kClosed)
    ShowUi();
  else
    CloseUi();
}

bool AmbientController::IsShown() const {
  return ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kShown ||
         ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kPreview;
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
}

void AmbientController::CloseAllWidgets(bool immediately) {
  for (auto* root_window_controller :
       RootWindowController::root_window_controllers()) {
    root_window_controller->CloseAmbientWidget(immediately);
  }
}

void AmbientController::OnEnabledPrefChanged() {
  if (IsAmbientModeEnabled()) {
    DVLOG(1) << "Ambient mode enabled";

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
        base::BindRepeating(
            &AmbientController::OnPhotoRefreshIntervalPrefChanged,
            weak_ptr_factory_.GetWeakPtr()));

    pref_change_registrar_->Add(
        ambient::prefs::kAmbientAnimationTheme,
        base::BindRepeating(&AmbientController::OnAnimationThemePrefChanged,
                            weak_ptr_factory_.GetWeakPtr()));

    pref_change_registrar_->Add(
        ambient::prefs::kAmbientModeAnimationPlaybackSpeed,
        base::BindRepeating(&AmbientController::OnAnimationPlaybackSpeedChanged,
                            weak_ptr_factory_.GetWeakPtr()));

    // Trigger the callbacks manually the first time to init AmbientUiModel.
    OnLockScreenInactivityTimeoutPrefChanged();
    OnLockScreenBackgroundTimeoutPrefChanged();
    OnPhotoRefreshIntervalPrefChanged();
    OnAnimationThemePrefChanged();
    OnAnimationPlaybackSpeedChanged();

    DCHECK(AmbientClient::Get());
    ambient_photo_controller_ = std::make_unique<AmbientPhotoController>(
        *AmbientClient::Get(), access_token_controller_, delegate_,
        // The type of photo config specified here is actually irrelevant as it
        // always gets reset with the correct configuration anyways in
        // StartRefreshingImages() before ambient mode starts.
        CreateAmbientSlideshowPhotoConfig());

    ambient_ui_model_observer_.Observe(&ambient_ui_model_);

    ambient_backend_model_observer_.Observe(GetAmbientBackendModel());

    auto* power_manager_client = chromeos::PowerManagerClient::Get();
    DCHECK(power_manager_client);
    power_manager_client_observer_.Observe(power_manager_client);

    fingerprint_->AddFingerprintObserver(
        fingerprint_observer_receiver_.BindNewPipeAndPassRemote());

    ambient_animation_progress_tracker_ =
        std::make_unique<AmbientAnimationProgressTracker>();
  } else {
    DVLOG(1) << "Ambient mode disabled";

    CloseUi();

    ambient_animation_progress_tracker_.reset();

    for (const auto* pref_name :
         {ambient::prefs::kAmbientModeLockScreenBackgroundTimeoutSeconds,
          ambient::prefs::kAmbientModeLockScreenInactivityTimeoutSeconds,
          ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds,
          ambient::prefs::kAmbientAnimationTheme,
          ambient::prefs::kAmbientModeAnimationPlaybackSpeed}) {
      if (pref_change_registrar_->IsObserved(pref_name))
        pref_change_registrar_->Remove(pref_name);
    }

    ambient_ui_model_observer_.Reset();
    ambient_backend_model_observer_.Reset();
    power_manager_client_observer_.Reset();

    if (fingerprint_observer_receiver_.is_bound())
      fingerprint_observer_receiver_.reset();

    ambient_photo_controller_.reset();
    current_theme_from_pref_.reset();
  }
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

void AmbientController::OnAnimationThemePrefChanged() {
  absl::optional<AmbientAnimationTheme> previous_theme_from_pref =
      current_theme_from_pref_;
  DCHECK(GetPrimaryUserPrefService());
  int current_theme_as_int = GetPrimaryUserPrefService()->GetInteger(
      ambient::prefs::kAmbientAnimationTheme);
  // Gracefully handle pref having invalid value in case pref storage is
  // corrupted somehow.
  if (current_theme_as_int < 0 ||
      current_theme_as_int >
          static_cast<int>(AmbientAnimationTheme::kMaxValue)) {
    LOG(WARNING) << "Loaded invalid ambient theme from pref storage: "
                 << current_theme_as_int << ". Default to "
                 << ToString(kDefaultAmbientAnimationTheme);
    current_theme_as_int = static_cast<int>(kDefaultAmbientAnimationTheme);
  }
  current_theme_from_pref_ =
      static_cast<AmbientAnimationTheme>(current_theme_as_int);

  if (previous_theme_from_pref.has_value()) {
    DVLOG(4) << "AmbientAnimationTheme changed from "
             << ToString(*previous_theme_from_pref) << " to "
             << ToString(*current_theme_from_pref_);
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
    DCHECK(ambient_photo_controller_);
    ambient_photo_controller_->ClearCache();
  } else {
    DVLOG(4) << "AmbientAnimationTheme initialized to "
             << ToString(*current_theme_from_pref_);
  }
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
  access_token_controller_.RequestAccessToken(std::move(callback),
                                              may_refresh_token_on_lock);
}

void AmbientController::DismissUI() {
  if (!IsAmbientModeEnabled()) {
    CloseUi();
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
    ShowHiddenUi();
    return;
  }

  CloseUi();
}

AmbientBackendModel* AmbientController::GetAmbientBackendModel() {
  DCHECK(ambient_photo_controller_);
  return ambient_photo_controller_->ambient_backend_model();
}

AmbientWeatherModel* AmbientController::GetAmbientWeatherModel() {
  return ambient_weather_controller_->weather_model();
}

void AmbientController::OnImagesReady() {
  CreateAndShowWidgets();
}

void AmbientController::OnImagesFailed() {
  LOG(ERROR) << "Ambient mode failed to start";
  CloseUi();
}

std::unique_ptr<views::Widget> AmbientController::CreateWidget(
    aura::Window* container) {
  AmbientAnimationTheme current_theme = GetCurrentTheme();
  auto container_view = std::make_unique<AmbientContainerView>(
      &delegate_, ambient_animation_progress_tracker_.get(),
      AmbientAnimationStaticResources::Create(current_theme,
                                              /*serializable=*/true),
      multi_screen_metrics_recorder_.get(), frame_rate_controller_.get());
  auto* widget_delegate = new AmbientWidgetDelegate();
  widget_delegate->SetInitiallyFocusedView(container_view.get());

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = GetWidgetName();
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.parent = container;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
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

  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kShown) {
    DCHECK(start_time_);
    ambient::RecordAmbientModeStartupTime(base::Time::Now() - *start_time_,
                                          current_theme);
  }

  // Only announce for the primary window.
  if (Shell::GetPrimaryRootWindow() == container->GetRootWindow()) {
    contents_view->GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_ASH_SCREENSAVER_STARTS));
  }

  return widget;
}

void AmbientController::CreateAndShowWidgets() {
  // Hide cursor.
  Shell::Get()->cursor_manager()->HideCursor();
  for (auto* root_window_controller :
       RootWindowController::root_window_controllers()) {
    root_window_controller->CreateAmbientWidget();
  }
}

void AmbientController::StartRefreshingImages() {
  DCHECK(ambient_photo_controller_);
  // There is no use case for switching themes "on-the-fly" while ambient mode
  // is rendering. Thus, it's sufficient to just reinitialize the
  // model/controller with the appropriate config each time before calling
  // StartScreenUpdate().
  DCHECK(!ambient_photo_controller_->IsScreenUpdateActive());
  AmbientAnimationTheme current_theme = GetCurrentTheme();
  DVLOG(4) << "Loaded ambient theme " << ToString(current_theme);

  AmbientPhotoConfig photo_config;
  std::unique_ptr<AmbientTopicQueue::Delegate> topic_queue_delegate;
  if (current_theme == AmbientAnimationTheme::kSlideshow) {
    photo_config = CreateAmbientSlideshowPhotoConfig();
    topic_queue_delegate =
        std::make_unique<AmbientTopicQueueSlideshowDelegate>();
  } else {
    scoped_refptr<cc::SkottieWrapper> animation =
        AmbientAnimationStaticResources::Create(current_theme,
                                                /*serializable=*/false)
            ->GetSkottieWrapper();
    photo_config =
        CreateAmbientAnimationPhotoConfig(animation->GetImageAssetMetadata());
    topic_queue_delegate = std::make_unique<AmbientTopicQueueAnimationDelegate>(
        animation->GetImageAssetMetadata());
  }
  ambient_photo_controller_->ambient_backend_model()->SetPhotoConfig(
      std::move(photo_config));
  ambient_photo_controller_->StartScreenUpdate(std::move(topic_queue_delegate));
}

void AmbientController::StopRefreshingImages() {
  DCHECK(ambient_photo_controller_);
  ambient_photo_controller_->StopScreenUpdate();
}

void AmbientController::MaybeStartScreenSaver() {
  // The screensaver may have already been started.
  if (ambient_photo_controller_->IsScreenUpdateActive())
    return;

  if (!user_activity_observer_.IsObserving())
    user_activity_observer_.Observe(ui::UserActivityDetector::Get());

  // Add observer for assistant interaction model
  AssistantInteractionController::Get()->GetModel()->AddObserver(this);

  multi_screen_metrics_recorder_ =
      std::make_unique<AmbientMultiScreenMetricsRecorder>(GetCurrentTheme());
  frame_rate_controller_ =
      std::make_unique<AmbientAnimationFrameRateController>(
          Shell::Get()->frame_throttling_controller());

  Shell::Get()->AddPreTargetHandler(this);
  StartRefreshingImages();
}

AmbientAnimationTheme AmbientController::GetCurrentTheme() const {
  DCHECK(current_theme_from_pref_);
  return *current_theme_from_pref_;
}

}  // namespace ash
