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
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"
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

// Returns the parent container of ambient widget. Will return a nullptr for
// the in-session UI when lock-screen is currently not shown.
aura::Window* GetWidgetContainer() {
  if (ambient::util::IsShowing(LockScreen::ScreenType::kLock)) {
    return Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                               kShellWindowId_LockScreenContainer);
  }
  return nullptr;
}

// Returns the name of the ambient widget.
std::string GetWidgetName() {
  if (ambient::util::IsShowing(LockScreen::ScreenType::kLock))
    return "LockScreenAmbientModeContainer";
  return "InSessionAmbientModeContainer";
}

// Returns true if the device is currently connected to a charger.
bool IsChargerConnected() {
  return (PowerStatus::Get()->IsBatteryCharging() ||
          PowerStatus::Get()->IsBatteryFull()) &&
         PowerStatus::Get()->IsLinePowerConnected();
}

bool IsUiHidden(AmbientUiVisibility visibility) {
  return visibility == AmbientUiVisibility::kHidden;
}

bool IsLockScreenUi(AmbientUiMode mode) {
  return mode == AmbientUiMode::kLockScreenUi;
}

bool IsAmbientModeEnabled() {
  if (!AmbientClient::Get()->IsAmbientModeAllowed())
    return false;

  ash::SessionControllerImpl* controller = Shell::Get()->session_controller();
  PrefService* prefs = controller->GetActivePrefService();
  DCHECK(prefs);
  return prefs->GetBoolean(ambient::prefs::kAmbientModeEnabled);
}

}  // namespace

// AmbientController::InactivityMonitor----------------------------------

// Monitors the events when ambient screen is hidden, and shows up the screen
// automatically if the device has been inactive for a specific amount of time.
class AmbientController::InactivityMonitor : public ui::EventHandler {
 public:
  using AutoShowCallback = base::OnceCallback<void()>;

  InactivityMonitor(base::WeakPtr<views::Widget> target_widget,
                    AutoShowCallback callback)
      : target_widget_(target_widget) {
    timer_.Start(FROM_HERE, kAutoShowWaitTimeInterval, std::move(callback));

    DCHECK(target_widget_);
    target_widget_->GetNativeWindow()->AddPreTargetHandler(this);
  }

  ~InactivityMonitor() override {
    if (target_widget_) {
      target_widget_->GetNativeWindow()->RemovePreTargetHandler(this);
    }
  }

  InactivityMonitor(const InactivityMonitor&) = delete;
  InactivityMonitor& operator=(const InactivityMonitor&) = delete;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    // Restarts the timer upon events from the target widget.
    timer_.Reset();
  }

 private:
  base::WeakPtr<views::Widget> target_widget_;
  // Will be canceled when out-of-scope.
  base::OneShotTimer timer_;
};

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

AmbientController::AmbientController() {
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
          /*ui_mode=*/ambient_ui_model_.ui_mode(),
          /*tablet_mode=*/Shell::Get()->IsInTabletMode());

      DCHECK(!start_time_);
      start_time_ = base::Time::Now();

      // Resets the monitor and cancels the timer upon shown.
      inactivity_monitor_.reset();

      if (IsChargerConnected()) {
        // Requires wake lock to prevent display from sleeping.
        AcquireWakeLock();
      }
      // Observes the |PowerStatus| on the battery charging status change for
      // the current ambient session.
      if (!power_status_observer_.IsObserving(PowerStatus::Get())) {
        power_status_observer_.Add(PowerStatus::Get());
      }

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
        // Creates the monitor and starts the auto-show timer upon hidden.
        DCHECK(!inactivity_monitor_);
        if (LockScreen::HasInstance()) {
          inactivity_monitor_ = std::make_unique<InactivityMonitor>(
              LockScreen::Get()->widget()->GetWeakPtr(),
              base::BindOnce(&AmbientController::OnAutoShowTimeOut,
                             weak_ptr_factory_.GetWeakPtr()));
        }
      } else {
        DCHECK(visibility == AmbientUiVisibility::kClosed);
        inactivity_monitor_.reset();
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
  if (!IsAmbientModeEnabled()) {
    VLOG(1) << "Ambient mode is not allowed.";
    return;
  }

  if (locked) {
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

    ShowUi(AmbientUiMode::kLockScreenUi);
  } else {
    // Ambient screen will be destroyed along with the lock screen when user
    // logs in.
    CloseUi();
  }
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
  // If screen is back on, turn on ambient mode for lock screen.
  if (LockScreen::HasInstance())
    ShowUi(AmbientUiMode::kLockScreenUi);
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

  auto* session_controller = Shell::Get()->session_controller();
  if (session_controller->CanLockScreen() &&
      session_controller->ShouldLockScreenAutomatically()) {
    if (!session_controller->IsScreenLocked()) {
      // TODO(b/161469136): revise this behavior after further discussion.
      Shell::Get()->session_controller()->LockScreen();
    }
  } else {
    ShowUi(AmbientUiMode::kInSessionUi);
  }
}

void AmbientController::AddAmbientViewDelegateObserver(
    AmbientViewDelegateObserver* observer) {
  delegate_.AddObserver(observer);
}

void AmbientController::RemoveAmbientViewDelegateObserver(
    AmbientViewDelegateObserver* observer) {
  delegate_.RemoveObserver(observer);
}

void AmbientController::ShowUi(AmbientUiMode mode) {
  DVLOG(1) << "ShowUi: " << mode;

  // TODO(meilinw): move the eligibility check to the idle entry point once
  // implemented: b/149246117.
  if (!IsAmbientModeEnabled()) {
    LOG(WARNING) << "Ambient mode is not allowed.";
    return;
  }

  ambient_ui_model_.SetUiMode(mode);
  switch (mode) {
    case AmbientUiMode::kInSessionUi:
      ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kShown);
      break;
    case AmbientUiMode::kLockScreenUi:
      ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kHidden);
      break;
  }
}

void AmbientController::CloseUi() {
  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kClosed);
}

void AmbientController::HideLockScreenUi() {
  DCHECK(IsLockScreenUi(ambient_ui_model_.ui_mode()));

  ambient_ui_model_.SetUiVisibility(AmbientUiVisibility::kHidden);
}

void AmbientController::ToggleInSessionUi() {
  if (!container_view_)
    ShowUi(AmbientUiMode::kInSessionUi);
  else
    CloseUi();
}

bool AmbientController::IsShown() const {
  return container_view_ && container_view_->IsDrawn();
}

void AmbientController::OnBackgroundPhotoEvents() {
  // Dismisses the ambient screen when user interacts with the background photo.
  if (IsLockScreenUi(ambient_ui_model_.ui_mode()))
    HideLockScreenUi();
  else
    CloseUi();
}

void AmbientController::UpdateUiMode(AmbientUiMode ui_mode) {
  ambient_ui_model_.SetUiMode(ui_mode);
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
}

void AmbientController::ReleaseWakeLock() {
  if (!wake_lock_)
    return;

  wake_lock_->CancelWakeLock();
  VLOG(1) << "Released wake lock";
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

AmbientBackendModel* AmbientController::GetAmbientBackendModel() {
  return ambient_photo_controller_.ambient_backend_model();
}

void AmbientController::OnImagesChanged() {
  if (!container_view_)
    CreateAndShowWidget();
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

  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  widget->SetContentsView(CreateContainerView());

  widget->SetVisibilityAnimationTransition(
      views::Widget::VisibilityTransition::ANIMATE_BOTH);
  ::wm::SetWindowVisibilityAnimationType(
      widget->GetNativeWindow(), ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ::wm::SetWindowVisibilityChangesAnimated(widget->GetNativeWindow());

  widget->Show();

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
