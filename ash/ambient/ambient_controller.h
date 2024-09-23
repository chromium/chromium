// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/ambient_weather_controller.h"
#include "ash/ambient/managed/screensaver_images_policy_handler.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/resources/ambient_dlc_background_installer.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/screen_backlight_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/power_status.h"
#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/fingerprint.mojom.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/events/event_handler.h"
#include "ui/views/widget/widget.h"

class PrefRegistrySimple;

namespace ash {

// Delay for dismissing screensaver preview on mouse move.
constexpr base::TimeDelta kDismissPreviewOnMouseMoveDelay = base::Seconds(3);

class AmbientBackendController;
class AmbientContainerView;
class AmbientPhotoController;
class AmbientSessionMetricsRecorder;
class AmbientUiSettings;

// Class to handle all ambient mode functionalities.
class ASH_EXPORT AmbientController
    : public AmbientUiModelObserver,
      public ScreenBacklightObserver,
      public SessionObserver,
      public PowerStatus::Observer,
      public chromeos::PowerManagerClient::Observer,
      public device::mojom::FingerprintObserver,
      public ui::UserActivityObserver,
      public ui::EventHandler,
      public AssistantInteractionModelObserver,
      public AmbientUiLauncher::Observer {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit AmbientController(
      mojo::PendingRemote<device::mojom::Fingerprint> fingerprint);

  AmbientController(const AmbientController&) = delete;
  AmbientController& operator=(const AmbientController&) = delete;

  ~AmbientController() override;

  // AmbientUiModelObserver:
  void OnAmbientUiVisibilityChanged(AmbientUiVisibility visibility) override;

  // Screen backlights off observer:
  void OnBacklightsForcedOffChanged(bool backlights_forced_off) override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnSigninScreenPrefServiceInitialized(PrefService* pref_service) override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  // chromeos::PowerManagerClient::Observer:
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& idle_state) override;
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // fingerprint::mojom::FingerprintObserver:
  void OnAuthScanDone(
      const device::mojom::FingerprintMessagePtr msg,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override;
  void OnSessionFailed() override {}
  void OnRestarted() override {}
  void OnStatusChanged(device::mojom::BiometricsManagerStatus state) override {}
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool enroll_session_complete,
                        int percent_complete) override {}

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // AssistantInteractionModelObserver:
  void OnInteractionStateChanged(InteractionState interaction_state) override;

  // AmbientUiLauncher::Observer:
  void OnReadyStateChanged(bool is_ready) override;

  // Invoked by the `LockScreen` to notify ambient mode that either the login or
  // lock screen has been created.
  void OnLoginOrLockScreenCreated();

  // Set the ui state to begin showing ambient mode. After calling this
  // function, there will be a delay while content downloads or reads from disk
  // until ambient mode widget is actually constructed.
  void SetUiVisibilityShouldShow();

  // Set the ui state to begin showing preview ambient mode. After calling this
  // function, there will be a delay while content downloads or reads from disk
  // until ambient mode widget is actually constructed.
  void SetUiVisibilityPreview();

  // Ui will be enabled but media will not load and UI will not be shown
  // immediately. Typically used by lock screen. If there is no user activity Ui
  // will be transitioned to shown state after a short delay.
  void SetUiVisibilityHidden();

  void SetUiVisibilityClosed(bool immediately = false);

  // |minutes| is the number of minutes to run screen saver before putting the
  // device into sleep. |minutes| with a value 0 means forever.
  void SetScreenSaverDuration(int minutes);

  void StartTimerToReleaseWakeLock();

  // Returns true if current state should be visible in UI. When this becomes
  // true, there is a delay while media loads before `IsAmbientRunning` will
  // also return true.
  bool ShouldShowAmbientUi() const;

  // Returns true if ambient is actually started and visible. Implies
  // `ShouldShowAmbientUi`. Media has already loaded and widget is
  // constructed.
  bool IsShowing() const;

  void RequestAccessToken(
      AmbientAccessTokenController::AccessTokenCallback callback,
      bool may_refresh_token_on_lock = false);

  // Creates a widget and |AmbientContainerView| for the container.
  std::unique_ptr<views::Widget> CreateWidget(aura::Window* container);

  AmbientBackendModel* GetAmbientBackendModel();
  AmbientWeatherModel* GetAmbientWeatherModel();

  AmbientBackendController* ambient_backend_controller() {
    return ambient_backend_controller_.get();
  }

  AmbientUiLauncher* ambient_ui_launcher() {
    return ambient_ui_launcher_.get();
  }

  AmbientWeatherController* ambient_weather_controller() {
    return ambient_weather_controller_.get();
  }

  AmbientUiModel* ambient_ui_model() { return &ambient_ui_model_; }

  AmbientViewDelegate* ambient_view_delegate() { return &delegate_; }

  AmbientAccessTokenController* access_token_controller() {
    return &access_token_controller_;
  }

  void set_backend_controller_for_testing(
      std::unique_ptr<AmbientBackendController> backend_controller) {
    ambient_backend_controller_ = std::move(backend_controller);
  }

 private:
  // Enum to indicate which state the lock screen is in. This is used
  // by `OnLoginLockScreenStateChanged` method as a parameter to pass
  // the correct information to the method.
  enum LockScreenState { kLogin, kLocked, kUnlocked };

  // Tracks the progression of states with `AmbientUiLauncher`.
  enum class AmbientUiLauncherState {
    // Waiting for `Initialize()` to finish.
    kInitializing,
    // `Initialize()` has completed successfully.
    kRendering,
    // After `Finalize()` (not in the middle of launching or rendering an
    // ambient session).
    kInactive,
  };

  friend class AmbientAshTestBase;
  friend class AmbientControllerTest;
  FRIEND_TEST_ALL_PREFIXES(AmbientControllerTest,
                           BindsObserversWhenAmbientEnabled);
  FRIEND_TEST_ALL_PREFIXES(AmbientControllerTest, BindsObserversWhenAmbientOn);

  AmbientPhotoController* ambient_photo_controller() {
    return ambient_ui_launcher_->GetAmbientPhotoController();
  }

  // Hide or close Ambient mode UI.
  void DismissUI();

  // Creates and shows a full-screen widget for each root window to show the
  // ambient UI.
  void CreateAndShowWidgets();

  void StartRefreshingImages();
  void StopScreensaver();
  void MaybeStartScreenSaver();
  void MaybeDismissUIOnMouseMove();
  AmbientUiSettings GetCurrentUiSettings() const;

  // Invoked when the auto-show timer in |InactivityMonitor| gets fired after
  // device being inactive for a specific amount of time.
  void OnAutoShowTimeOut();

  // Creates (if not created) and acquires |wake_lock_|. Unbalanced call
  // without subsequently |ReleaseWakeLock| will have no effect.
  void AcquireWakeLock();

  // Release |wake_lock_|. Unbalanced release call will have no effect.
  void ReleaseWakeLock();

  void CloseAllWidgets(bool immediately);

  // Start receiving mouse/key/touch events from `ash::Shell`.
  void SetUpPreTargetHandler();

  // Stop receiving mouse/key/touch events from `ash::Shell`.
  void ClearPreTargetHandler();

  // Removes any and all ambient mode ui model related settings pref observers
  void RemoveAmbientModeSettingsPrefObservers();

  // Adds/Removes pref observers
  void AddManagedScreensaverPolicyPrefObservers();
  void AddConsumerPrefObservers();

  // Invoked when the Ambient mode prefs state changes.
  void OnEnabledPrefChanged();
  void OnLockScreenInactivityTimeoutPrefChanged();
  void OnLockScreenBackgroundTimeoutPrefChanged();
  void OnPhotoRefreshIntervalPrefChanged();
  void OnAmbientUiSettingsChanged();
  void OnAnimationPlaybackSpeedChanged();

  // Resets the resources allocated by the ambient controller.
  void ResetAmbientControllerResources();

  // Invoked when preferences change via policy updates.
  void OnManagedScreensaverLockScreenIdleTimeoutPrefChanged();
  void OnManagedScreensaverPhotoRefreshIntervalPrefChanged();

  void CreateUiLauncher();
  void DestroyUiLauncher();
  bool IsUiLauncherActive() const;
  void OnUiLauncherInitialized(bool success);

  void OnLoginLockStateChanged(LockScreenState state);

  LockScreenState GetLockScreenState();

  // Returns the active pref change registrar. Note: The registar for user
  // profile `pref_change_registrar_` will always be the active pref change
  // registrar when the user is logged in, when the user is not logged in
  // this will return the `sign_in_pref_change_registrar_`.
  PrefChangeRegistrar* GetActivePrefChangeRegistrar();

  void MaybeStopUiEventPropagation(ui::Event* event);

  AmbientViewDelegateImpl delegate_{this};
  AmbientUiModel ambient_ui_model_;

  AmbientAccessTokenController access_token_controller_;
  AmbientBackgroundDlcInstaller background_dlc_installer_;
  std::unique_ptr<AmbientBackendController> ambient_backend_controller_;
  std::unique_ptr<AmbientWeatherController> ambient_weather_controller_;

  // Monitors the device inactivity and controls the auto-show of ambient.
  base::OneShotTimer inactivity_timer_;

  // Monitors the running time since screen saver starts.
  base::OneShotTimer screensaver_running_timer_;

  // Lazily initialized on the first call of |AcquireWakeLock|.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  base::ScopedObservation<AmbientUiModel, AmbientUiModelObserver>
      ambient_ui_model_observer_{this};
  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      session_observer_{this};
  base::ScopedObservation<PowerStatus, PowerStatus::Observer>
      power_status_observer_{this};
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_{this};
  base::ScopedObservation<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observer_{this};

  base::ScopedObservation<BacklightsForcedOffSetter, ScreenBacklightObserver>
      backlights_forced_off_observation_{this};

  // Observes user profile prefs for ambient.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Observes sign-in profile prefs for ambient mode device policy changes.
  std::unique_ptr<PrefChangeRegistrar> sign_in_pref_change_registrar_;

  // Records the time when preview widgets are created.
  base::Time preview_widget_created_at_;

  base::OneShotTimer delayed_lock_timer_;

  mojo::Remote<device::mojom::Fingerprint> fingerprint_;
  mojo::Receiver<device::mojom::FingerprintObserver>
      fingerprint_observer_receiver_{this};

  // Set when |SuspendImminent| is called and cleared when |SuspendDone| is
  // called. Used to prevent Ambient mode from reactivating while device is
  // going to suspend.
  bool is_suspend_imminent_ = false;

  // Set to the off value in |ScreenIdleState| when ScreenIdleState() is
  // called. Used to prevent Ambient mode starting after screen is off.
  bool is_screen_off_ = false;

  bool close_widgets_immediately_ = false;

  // ui::EventType::kMouseMove is fired before many mouse events. An event is an
  // actual mouse move event only if the last event was
  // ui::EventType::kMouseMove too. Used to keep track of the last event and
  // identify a true mouse move event.
  // TODO(safarli): Remove this workaround when b/266234711 is fixed.
  bool last_mouse_event_was_move_ = false;

  // Flag used to handle calls to OnEnabledPrefChanged initializing
  // the controller.
  bool is_initialized_ = false;

  // Flag used to monitor if receiving events, such as mouse/key/touch, from
  // `ash::Shell`.
  bool is_receiving_pretarget_events_ = false;

  AmbientUiLauncherState ui_launcher_state_ = AmbientUiLauncherState::kInactive;

  std::unique_ptr<AmbientSessionMetricsRecorder> session_metrics_recorder_;

  // The policy handler for downloading policy set images. This lives in the
  // ambient controller because it needs to outlive the disable policy update
  // so that it is able to actually clean up the images when the policy is
  // disabled.
  //
  // The sequence of operations are as follows which happen on policy update
  // 1. Admin sets ambient mode policy to disabled
  // 2. Ambient mode is dismissed
  // 3. ManagedSlideshowUiLauncher is destroyed
  // 4. Other policy values (photo interval, inactivity time, images) are unset
  //    and sent as part of the policy update.
  //
  // Now at point 4 the policy handler needs to be alive so that it can react to
  // the unset images call and clean up the images from disk.
  std::unique_ptr<ScreensaverImagesPolicyHandler>
      screensaver_images_policy_handler_;

  std::unique_ptr<AmbientUiLauncher> ambient_ui_launcher_;
  base::CancelableOnceCallback<void(bool)> ui_launcher_init_callback_;

  base::WeakPtrFactory<AmbientController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_CONTROLLER_H_
