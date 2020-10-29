// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_metrics.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/power/power_status.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/buildflag.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/types/event_type.h"
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

void CloseAssistantUi() {
  DCHECK(AssistantUiController::Get());
  AssistantUiController::Get()->CloseUi(
      chromeos::assistant::AssistantExitPoint::kUnspecified);
}

std::unique_ptr<AmbientBackendController> CreateAmbientBackendController() {
#if BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
  return std::make_unique<AmbientBackendControllerImpl>();
#else
  return std::make_unique<FakeAmbientBackendControllerImpl>();
#endif  // BUILDFLAG(ENABLE_CROS_AMBIENT_MODE_BACKEND)
}

aura::Window* GetWidgetContainer() {
  return Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                             kShellWindowId_AmbientModeContainer);
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
    return power_status->IsBatteryCharging() || power_status->IsBatteryFull() ||
           (power_status->IsLinePowerConnected() &&
            !power_status->IsBatteryDischargingOnLinePower());
  } else {
    // Chromeboxes have no battery.
    return power_status->IsLinePowerConnected();
  }
}

bool IsUiHidden(AmbientUiVisibility visibility) {
  return visibility == AmbientUiVisibility::kHidden;
}

bool IsAmbientModeEnabled() {
  if (!AmbientClient::Get()->IsAmbientModeAllowed())
    return false;

  ash::SessionControllerImpl* controller = Shell::Get()->session_controller();
  PrefService* prefs = controller->GetActivePrefService();
  DCHECK(prefs);
  return prefs->GetBoolean(ambient::prefs::kAmbientModeEnabled);
}

class AmbientWidgetDelegate : public views::WidgetDelegate {
 public:
  AmbientWidgetDelegate() { SetCanMaximize(true); }
};

}  // namespace

// static
void AmbientController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  if (chromeos::features::IsAmbientModeEnabled()) {
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
  }
}

AmbientController::AmbientController(
    mojo::PendingRemote<device::mojom::Fingerprint> fingerprint)
    : fingerprint_(std::move(fingerprint)) {
  ambient_backend_controller_ = CreateAmbientBackendController();

  ambient_ui_model_observer_.Add(&ambient_ui_model_);
  // |SessionController| is initialized before |this| in Shell.
  session_observer_.Add(Shell::Get()->session_controller());

  // Checks the current lid state on initialization.
  auto* power_manager_client = chromeos::PowerManagerClient::Get();
  DCHECK(power_manager_client);
  power_manager_client_observer_.Add(power_manager_client);
  power_manager_client->RequestStatusUpdate();

  ambient_backend_model_observer_.Add(
      ambient_photo_controller_.ambient_backend_model());

  fingerprint_->AddFingerprintObserver(
      fingerprint_observer_receiver_.BindNewPipeAndPassRemote());
}

AmbientController::~AmbientController() {
  if (container_view_)
    container_view_->GetWidget()->CloseNow();
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
      if (!power_status_observer_.IsObserving(PowerStatus::Get())) {
        power_status_observer_.Add(PowerStatus::Get());
      }

      if (!user_activity_observer_.IsObserving(ui::UserActivityDetector::Get()))
        user_activity_observer_.Add(ui::UserActivityDetector::Get());

      StartRefreshingImages();
      break;
    case AmbientUiVisibility::kHidden:
    case AmbientUiVisibility::kClosed:
      CloseWidget(/*immediately=*/false);

      // TODO(wutao): This will clear the image cache currently. It will not
      // work with `kHidden` if the token has expired and ambient mode is shown
      // again.
      StopRefreshingImages();

      // We close the Assistant UI after ambient screen not being shown to sync
      // states to |AssistantUiController|. This will be a no-op if the
      // |kAmbientAssistant| feature is disabled, or the Assistant UI has
      // already been closed.
      CloseAssistantUi();

      // Should do nothing if the wake lock has already been released.
      ReleaseWakeLock();

      // |start_time_| may be empty in case of |AmbientUiVisibility::kHidden| if
      // ambient mode has just started.
      if (start_time_) {
        auto elapsed = base::Time::Now() - start_time_.value();
        DVLOG(2) << "Exit ambient mode. Elapsed time: " << elapsed;
        ambient::RecordAmbientModeTimeElapsed(
            /*time_delta=*/elapsed,
            /*tablet_mode=*/Shell::Get()->IsInTabletMode());
        start_time_.reset();
      }

      if (visibility == AmbientUiVisibility::kHidden) {
        if (LockScreen::HasInstance()) {
          // Add observer for user activity.
          if (!user_activity_observer_.IsObserving(
                  ui::UserActivityDetector::Get())) {
            user_activity_observer_.Add(ui::UserActivityDetector::Get());
          }

          // Start timer to show ambient mode.
          inactivity_timer_.Start(
              FROM_HERE, kAutoShowWaitTimeInterval,
              base::BindOnce(&AmbientController::OnAutoShowTimeOut,
                             weak_ptr_factory_.GetWeakPtr()));
        }
      } else {
        DCHECK(visibility == AmbientUiVisibility::kClosed);
        inactivity_timer_.Stop();
        if (user_activity_observer_.IsObserving(
                ui::UserActivityDetector::Get())) {
          user_activity_observer_.Remove(ui::UserActivityDetector::Get());
        }
        if (power_status_observer_.IsObserving(PowerStatus::Get()))
          power_status_observer_.Remove(PowerStatus::Get());
      }

      break;
  }
}

void AmbientController::OnAutoShowTimeOut() {
  DCHECK(IsUiHidden(ambient_ui_model_.ui_visibility()));
  DCHECK(!container_view_);

  // Show ambient screen after time out.
  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kShown);
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

void AmbientController::OnFirstSessionStarted() {
  if (IsAmbientModeEnabled())
    ambient_photo_controller_.ScheduleFetchBackupImages();
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

void AmbientController::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  DVLOG(1) << "ScreenBrightnessChanged: "
           << (change.has_percent() ? change.percent() : -1);

  if (!change.has_percent())
    return;

  constexpr double kMinBrightness = 0.01;
  if (change.percent() < kMinBrightness) {
    if (is_screen_off_)
      return;

    DVLOG(1) << "Screen is off, close ambient mode.";
    is_screen_off_ = true;
    // If screen is off, turn everything off. This covers:
    //   1. Manually turn screen off.
    //   2. Clicking tablet power button.
    //   3. Close lid.
    // Need to specially close the widget immediately here to be able to close
    // the UI before device goes to suspend. Otherwise when opening lid after
    // lid closed, there may be a flash of the old window before previous
    // closing finished.
    CloseWidget(/*immediately=*/true);
    CloseUi();
    return;
  }

  // change.percent() > kMinBrightness
  if (!is_screen_off_)
    return;
  is_screen_off_ = false;

  // Reset image failures to allow retrying ambient mode because screen has
  // turned back on.
  GetAmbientBackendModel()->ResetImageFailures();

  // If screen is back on, turn on ambient mode for lock screen.
  if (LockScreen::HasInstance())
    ShowHiddenUi();
}

void AmbientController::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& idle_state) {
  DVLOG(1) << "ScreenIdleStateChanged: dimmed(" << idle_state.dimmed()
           << ") off(" << idle_state.off() << ")";

  if (!IsAmbientModeEnabled())
    return;

  // "off" state should already be handled by the screen brightness handler.
  if (idle_state.off())
    return;

  if (!idle_state.dimmed())
    return;

  // Do not show the UI if lockscreen is active. The inactivity monitor should
  // have activated ambient mode.
  if (LockScreen::HasInstance())
    return;

  // Do not show UI if loading images was unsuccessful.
  if (GetAmbientBackendModel()->ImageLoadingFailed()) {
    VLOG(1) << "Skipping ambient mode activation due to prior failure";
    return;
  }

  ShowUi();
}

void AmbientController::OnAuthScanDone(
    device::mojom::ScanResult scan_result,
    const base::flat_map<std::string, std::vector<std::string>>& matches) {
  DismissUI();
}

void AmbientController::OnUserActivity(const ui::Event* event) {
  DismissUI();
}

void AmbientController::AddAmbientViewDelegateObserver(
    AmbientViewDelegateObserver* observer) {
  delegate_.AddObserver(observer);
}

void AmbientController::RemoveAmbientViewDelegateObserver(
    AmbientViewDelegateObserver* observer) {
  delegate_.RemoveObserver(observer);
}

void AmbientController::ShowUi() {
  DVLOG(1) << __func__;

  // TODO(meilinw): move the eligibility check to the idle entry point once
  // implemented: b/149246117.
  if (!IsAmbientModeEnabled()) {
    LOG(WARNING) << "Ambient mode is not allowed.";
    return;
  }

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kShown);
}

void AmbientController::ShowHiddenUi() {
  DVLOG(1) << __func__;

  if (!IsAmbientModeEnabled()) {
    LOG(WARNING) << "Ambient mode is not allowed.";
    return;
  }

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kHidden);
}

void AmbientController::CloseUi() {
  DVLOG(1) << __func__;

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kClosed);
}

void AmbientController::ToggleInSessionUi() {
  if (ambient_ui_model_.ui_visibility() == AmbientUiVisibility::kClosed)
    ShowUi();
  else
    CloseUi();
}

bool AmbientController::IsShown() const {
  return container_view_ && container_view_->IsDrawn();
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
          FROM_HERE, kLockScreenDelay, base::BindOnce([]() {
            Shell::Get()->session_controller()->LockScreen();
          }));
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

void AmbientController::CloseWidget(bool immediately) {
  if (!container_view_)
    return;

  if (immediately)
    container_view_->GetWidget()->CloseNow();
  else
    container_view_->GetWidget()->Close();

  container_view_ = nullptr;
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
    inactivity_timer_.Reset();
    return;
  }

  if (LockScreen::HasInstance()) {
    ShowHiddenUi();
    return;
  }

  CloseUi();
}

AmbientBackendModel* AmbientController::GetAmbientBackendModel() {
  return ambient_photo_controller_.ambient_backend_model();
}

void AmbientController::OnImagesReady() {
  CreateAndShowWidget();
}

void AmbientController::OnImagesFailed() {
  LOG(ERROR) << "Ambient mode failed to start";
  CloseUi();
}

std::unique_ptr<AmbientContainerView> AmbientController::CreateContainerView() {
  DCHECK(!container_view_);

  auto container = std::make_unique<AmbientContainerView>(&delegate_);
  container_view_ = container.get();
  return container;
}

void AmbientController::CreateAndShowWidget() {
  DCHECK(!container_view_);

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = GetWidgetName();
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.parent = GetWidgetContainer();
  params.delegate = new AmbientWidgetDelegate();

  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  widget->SetContentsView(CreateContainerView());

  widget->SetVisibilityAnimationTransition(
      views::Widget::VisibilityTransition::ANIMATE_BOTH);
  ::wm::SetWindowVisibilityAnimationType(
      widget->GetNativeWindow(), ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ::wm::SetWindowVisibilityChangesAnimated(widget->GetNativeWindow());

  widget->Show();

  // Hide cursor.
  Shell::Get()->cursor_manager()->HideCursor();

  // Requests keyboard focus for |container_view_| to receive keyboard events.
  container_view_->RequestFocus();
}

void AmbientController::StartRefreshingImages() {
  ambient_photo_controller_.StartScreenUpdate();
}

void AmbientController::StopRefreshingImages() {
  ambient_photo_controller_.StopScreenUpdate();
}

void AmbientController::set_backend_controller_for_testing(
    std::unique_ptr<AmbientBackendController> backend_controller) {
  ambient_backend_controller_ = std::move(backend_controller);
}

constexpr base::TimeDelta AmbientController::kAutoShowWaitTimeInterval;
}  // namespace ash
