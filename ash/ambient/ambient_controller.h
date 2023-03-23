// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_photo_cache.h"
#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/constants/ambient_theme.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/screen_backlight_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/power_status.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/events/event_handler.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class PrefRegistrySimple;

namespace ash {

// Delay for dismissing screensaver preview on mouse move.
constexpr base::TimeDelta kDismissPreviewOnMouseMoveDelay = base::Seconds(3);

class AmbientAnimationFrameRateController;
class AmbientAnimationProgressTracker;
class AmbientBackendController;
class AmbientContainerView;
class AmbientMultiScreenMetricsRecorder;
class AmbientPhotoController;
class AmbientUiSettings;
class AmbientWeatherController;

// Class to handle all ambient mode functionalities.
class ASH_EXPORT AmbientController
    : public AmbientUiModelObserver,
      public AmbientBackendModelObserver,
      public ScreenBacklightObserver,
      public SessionObserver,
      public PowerStatus::Observer,
      public chromeos::PowerManagerClient::Observer,
      public device::mojom::FingerprintObserver,
      public ui::UserActivityObserver,
      public ui::EventHandler,
      public AssistantInteractionModelObserver {
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

  void ShowUi();
  void StartScreenSaverPreview();
  // Ui will be enabled but not shown immediately. If there is no user activity
  // Ui will be shown after a short delay.
  void ShowHiddenUi();
  void CloseUi(bool immediately = false);

  void ToggleInSessionUi();

  // Returns true if ambient mode containers are visible or are being
  // constructed.
  bool IsShown() const;

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

  AmbientPhotoCache* ambient_photo_cache() { return photo_cache_.get(); }

  void set_backend_controller_for_testing(
      std::unique_ptr<AmbientBackendController> backend_controller) {
    ambient_backend_controller_ = std::move(backend_controller);
  }

 private:
  friend class AmbientAshTestBase;
  friend class AmbientControllerTest;
  FRIEND_TEST_ALL_PREFIXES(AmbientControllerTest,
                           BindsObserversWhenAmbientEnabled);
  FRIEND_TEST_ALL_PREFIXES(AmbientControllerTest, BindsObserversWhenAmbientOn);

  AmbientPhotoController* ambient_photo_controller() {
    return ambient_photo_controller_.get();
  }

  AmbientPhotoCache* get_backup_photo_cache_for_testing() {
    return backup_photo_cache_.get();
  }

  // Hide or close Ambient mode UI.
  void DismissUI();

  // AmbientBackendModelObserver overrides:
  void OnImagesReady() override;
  void OnImagesFailed() override;

  // Creates and shows a full-screen widget for each root window to show the
  // ambient UI.
  void CreateAndShowWidgets();

  void StartRefreshingImages();
  void StopScreensaver();
  void MaybeStartScreenSaver();
  void MaybeDismissUIOnMouseMove();
  AmbientUiSettings GetCurrentUiSettings() const;
  void SetUiSettingsForExperimentation();

  // Invoked when the auto-show timer in |InactivityMonitor| gets fired after
  // device being inactive for a specific amount of time.
  void OnAutoShowTimeOut();

  // Creates (if not created) and acquires |wake_lock_|. Unbalanced call
  // without subsequently |ReleaseWakeLock| will have no effect.
  void AcquireWakeLock();

  // Release |wake_lock_|. Unbalanced release call will have no effect.
  void ReleaseWakeLock();

  void CloseAllWidgets(bool immediately);

  // Removes any and all ambient mode ui model related settings pref observers
  void RemoveAmbientModeSettingsPrefObservers();

  // Adds/Removes managed pref observers
  void AddManagedScreensaverPolicyPrefObservers();
  void AddAmbientModeUserSettingsPolicyPrefObservers();

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
  void OnManagedScreensaverEnabledPrefChanged();
  void OnManagedScreensaverLockScreenIdleTimeoutPrefChanged();
  void OnManagedScreensaverPhotoRefreshIntervalPrefChanged();

  void CreateUiLauncher();
  void DestroyUiLauncher();
  bool IsUiLauncherActive() const;

  AmbientAccessTokenController* access_token_controller_for_testing() {
    return &access_token_controller_;
  }

  AmbientViewDelegateImpl delegate_{this};
  AmbientUiModel ambient_ui_model_;

  AmbientAccessTokenController access_token_controller_;
  std::unique_ptr<AmbientBackendController> ambient_backend_controller_;
  std::unique_ptr<AmbientPhotoCache> photo_cache_;
  std::unique_ptr<AmbientPhotoCache> backup_photo_cache_;
  std::unique_ptr<AmbientPhotoController> ambient_photo_controller_;
  std::unique_ptr<AmbientWeatherController> ambient_weather_controller_;
  std::unique_ptr<AmbientAnimationProgressTracker>
      ambient_animation_progress_tracker_;
  std::unique_ptr<AmbientAnimationFrameRateController> frame_rate_controller_;

  // Monitors the device inactivity and controls the auto-show of ambient.
  base::OneShotTimer inactivity_timer_;

  // Lazily initialized on the first call of |AcquireWakeLock|.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  base::ScopedObservation<AmbientUiModel, AmbientUiModelObserver>
      ambient_ui_model_observer_{this};
  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      ambient_backend_model_observer_{this};
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

  // Used to record Ambient mode engagement metrics.
  absl::optional<base::Time> start_time_ = absl::nullopt;

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

  // ui::ET_MOUSE_MOVE is fired before many mouse events. An event is an actual
  // mouse move event only if the last event was ui::ET_MOUSE_MOVE too. Used
  // to keep track of the last event and identify a true mouse move event.
  // TODO(safarli): Remove this workaround when b/266234711 is fixed.
  bool last_mouse_event_was_move_ = false;

  // Flag used to prevent multiple calls to OnEnabledPrefChanged initializing
  // the controller.
  bool is_initialized_ = false;

  std::unique_ptr<AmbientMultiScreenMetricsRecorder>
      multi_screen_metrics_recorder_;
  std::unique_ptr<AmbientUiLauncher> ambient_ui_launcher_;

  base::WeakPtrFactory<AmbientController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_CONTROLLER_H_
