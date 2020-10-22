// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_CONTROLLER_H_

#include <memory>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/system/power/power_status.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/fingerprint.mojom.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class PrefRegistrySimple;

namespace ash {

class AmbientBackendController;
class AmbientContainerView;
class AmbientPhotoController;
class AmbientViewDelegateObserver;

// Class to handle all ambient mode functionalities.
class ASH_EXPORT AmbientController
    : public AmbientUiModelObserver,
      public AmbientBackendModelObserver,
      public SessionObserver,
      public PowerStatus::Observer,
      public chromeos::PowerManagerClient::Observer,
      public device::mojom::FingerprintObserver,
      public ui::UserActivityObserver {
 public:
  static constexpr base::TimeDelta kAutoShowWaitTimeInterval =
      base::TimeDelta::FromSeconds(7);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit AmbientController(
      mojo::PendingRemote<device::mojom::Fingerprint> fingerprint);
  ~AmbientController() override;

  // AmbientUiModelObserver:
  void OnAmbientUiVisibilityChanged(AmbientUiVisibility visibility) override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;
  void OnFirstSessionStarted() override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  // chromeos::PowerManagerClient::Observer:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& idle_state) override;

  // fingerprint::mojom::FingerprintObserver:
  void OnAuthScanDone(
      device::mojom::ScanResult scan_result,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override;
  void OnSessionFailed() override {}
  void OnRestarted() override {}
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool enroll_session_complete,
                        int percent_complete) override {}

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

  void AddAmbientViewDelegateObserver(AmbientViewDelegateObserver* observer);
  void RemoveAmbientViewDelegateObserver(AmbientViewDelegateObserver* observer);

  void ShowUi();
  // Ui will be enabled but not shown immediately. If there is no user activity
  // Ui will be shown after a short delay.
  void ShowHiddenUi();
  void CloseUi();

  void ToggleInSessionUi();

  // Returns true if the |container_view_| is currently visible.
  bool IsShown() const;

  void RequestAccessToken(
      AmbientAccessTokenController::AccessTokenCallback callback,
      bool may_refresh_token_on_lock = false);

  AmbientBackendModel* GetAmbientBackendModel();

  AmbientBackendController* ambient_backend_controller() {
    return ambient_backend_controller_.get();
  }

  AmbientPhotoController* ambient_photo_controller() {
    return &ambient_photo_controller_;
  }

  AmbientUiModel* ambient_ui_model() { return &ambient_ui_model_; }

 private:
  friend class AmbientAshTestBase;

  // Hide or close Ambient mode UI.
  void DismissUI();

  // AmbientBackendModelObserver overrides:
  void OnImagesReady() override;
  void OnImagesFailed() override;

  // Initializes the |container_view_|. Called in |CreateWidget()| to create the
  // contents view.
  std::unique_ptr<AmbientContainerView> CreateContainerView();

  // TODO(meilinw): reuses the lock-screen widget: b/156531168, b/157175030.
  // Creates and shows a full-screen widget responsible for showing
  // the ambient UI.
  void CreateAndShowWidget();

  void StartRefreshingImages();
  void StopRefreshingImages();

  // Invoked when the auto-show timer in |InactivityMonitor| gets fired after
  // device being inactive for a specific amount of time.
  void OnAutoShowTimeOut();

  void set_backend_controller_for_testing(
      std::unique_ptr<AmbientBackendController> photo_client);

  // Creates (if not created) and acquires |wake_lock_|. Unbalanced call
  // without subsequently |ReleaseWakeLock| will have no effect.
  void AcquireWakeLock();

  // Release |wake_lock_|. Unbalanced release call will have no effect.
  void ReleaseWakeLock();

  void CloseWidget(bool immediately);

  // Invoked when the |kAmbientModeEnabled| pref state changed.
  void OnEnabledStateChanged();

  AmbientContainerView* get_container_view_for_testing() {
    return container_view_;
  }

  AmbientAccessTokenController* access_token_controller_for_testing() {
    return &access_token_controller_;
  }

  // Owned by |RootView| of its parent widget.
  AmbientContainerView* container_view_ = nullptr;

  AmbientViewDelegateImpl delegate_{this};
  AmbientUiModel ambient_ui_model_;

  AmbientAccessTokenController access_token_controller_;
  std::unique_ptr<AmbientBackendController> ambient_backend_controller_;
  AmbientPhotoController ambient_photo_controller_;

  // Monitors the device inactivity and controls the auto-show of ambient.
  base::OneShotTimer inactivity_timer_;

  // Lazily initialized on the first call of |AcquireWakeLock|.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  ScopedObserver<AmbientUiModel, AmbientUiModelObserver>
      ambient_ui_model_observer_{this};
  ScopedObserver<AmbientBackendModel, AmbientBackendModelObserver>
      ambient_backend_model_observer_{this};
  ScopedObserver<SessionControllerImpl, SessionObserver> session_observer_{
      this};
  ScopedObserver<PowerStatus, PowerStatus::Observer> power_status_observer_{
      this};
  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_{this};
  ScopedObserver<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observer_{this};

  bool is_screen_off_ = false;

  // Observes user profile prefs for ambient.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Used to record Ambient mode engagement metrics.
  base::Optional<base::Time> start_time_ = base::nullopt;

  base::OneShotTimer delayed_lock_timer_;

  mojo::Remote<device::mojom::Fingerprint> fingerprint_;
  mojo::Receiver<device::mojom::FingerprintObserver>
      fingerprint_observer_receiver_{this};

  base::WeakPtrFactory<AmbientController> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AmbientController);
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_CONTROLLER_H_
