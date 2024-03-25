// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_WINDOW_STATE_CONTROLLER_H_
#define ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_WINDOW_STATE_CONTROLLER_H_

#include <queue>
#include <vector>

#include "ash/webui/camera_app_ui/camera_app_helper.mojom.h"
#include "base/containers/enum_set.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class CameraAppWindowStateController
    : public camera_app::mojom::WindowStateController,
      public views::WidgetObserver {
 public:
  using WindowStateType = camera_app::mojom::WindowStateType;
  using WindowStateMonitor = camera_app::mojom::WindowStateMonitor;

  explicit CameraAppWindowStateController(views::Widget* widget);
  CameraAppWindowStateController(const CameraAppWindowStateController&) =
      delete;
  CameraAppWindowStateController& operator=(
      const CameraAppWindowStateController&) = delete;
  ~CameraAppWindowStateController() override;

  void AddReceiver(
      mojo::PendingReceiver<camera_app::mojom::WindowStateController> receiver);

  // camera_app::mojom::WindowStateController implementations.
  void AddMonitor(
      mojo::PendingRemote<camera_app::mojom::WindowStateMonitor> monitor,
      AddMonitorCallback callback) override;
  void GetWindowState(GetWindowStateCallback callback) override;
  void Minimize(MinimizeCallback callback) override;
  void Restore(RestoreCallback callback) override;
  void Maximize(MaximizeCallback callback) override;
  void Fullscreen(FullscreenCallback callback) override;
  void Focus(FocusCallback callback) override;

  // views::WidgetObserver implementations.
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  using WindowStateTypeSet = base::EnumSet<WindowStateType,
                                           WindowStateType::kMinValue,
                                           WindowStateType::kMaxValue>;

  void OnWindowStateChanged();
  void OnWindowFocusChanged(bool is_focus);
  WindowStateTypeSet GetCurrentWindowStates() const;

  raw_ptr<views::Widget> widget_;
  WindowStateTypeSet window_states_;
  mojo::ReceiverSet<camera_app::mojom::WindowStateController> receivers_;
  std::vector<mojo::Remote<WindowStateMonitor>> monitors_;
  std::queue<base::OnceClosure> minimize_callbacks_;
  std::queue<base::OnceClosure> restore_callbacks_;
  std::queue<base::OnceClosure> maximize_callbacks_;
  std::queue<base::OnceClosure> fullscreen_callbacks_;
  std::queue<base::OnceClosure> focus_callbacks_;
};

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_WINDOW_STATE_CONTROLLER_H_
