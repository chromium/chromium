// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_WINDOW_MANAGER_H_
#define ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_WINDOW_MANAGER_H_

#include "ash/webui/camera_app_ui/camera_app_helper.mojom.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/views/widget/widget_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ash {

// A manager to manage the camera usage ownership between multiple camera app
// windows. The clients should only use this object as a singleton instance and
// should only access it on the UI thread.
class CameraAppWindowManager : public views::WidgetObserver {
 public:
  CameraAppWindowManager(const CameraAppWindowManager&) = delete;
  CameraAppWindowManager& operator=(const CameraAppWindowManager&) = delete;
  ~CameraAppWindowManager() override;

  static CameraAppWindowManager* GetInstance();

  void SetCameraUsageMonitor(
      aura::Window* window,
      mojo::PendingRemote<camera_app::mojom::CameraUsageOwnershipMonitor>
          usage_monitor,
      base::OnceCallback<void(bool)> callback);

  void SetDevToolsEnabled(bool enabled);

  bool IsDevToolsEnabled();

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  friend class CameraAppWindowManagerTest;

  CameraAppWindowManager();

  friend struct base::DefaultSingletonTraits<CameraAppWindowManager>;
  enum class TransferState {
    kIdle,
    kSuspending,
    kResuming,
  };
  void OnMonitorMojoConnectionError(views::Widget* widget);
  void SuspendCameraUsage();
  void OnSuspendedCameraUsage(views::Widget* prev_owner);
  void ResumeCameraUsage();
  void OnResumedCameraUsage(views::Widget* prev_owner);
  void ResumeNextOrIdle();

  // Whether dev tools window should be opened when opening CCA window.
  bool dev_tools_enabled_ = false;

  base::flat_map<views::Widget*,
                 mojo::Remote<camera_app::mojom::CameraUsageOwnershipMonitor>>
      camera_usage_monitors_;

  // Whether the |owner_| is transferring the camera usage.
  TransferState transfer_state_ = TransferState::kIdle;

  // The widget which has the camera usage ownership currently.
  raw_ptr<views::Widget, ExperimentalAsh> owner_ = nullptr;

  // For the pending camera usage owner, there are three possible values:
  // 1. absl::nullopt: When there is no pending owner. Transfer can stop.
  // 2. nullptr:       When there should be no active window after the transfer
  //                   is stopped.
  // 3. non-null:      When there is another window which should own camera
  //                   usage.
  absl::optional<views::Widget*> pending_transfer_;
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_WINDOW_MANAGER_H_
