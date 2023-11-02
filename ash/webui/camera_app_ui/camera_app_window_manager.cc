// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/camera_app_window_manager.h"

#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

CameraAppWindowManager::~CameraAppWindowManager() = default;

// static
CameraAppWindowManager* CameraAppWindowManager::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::Singleton<CameraAppWindowManager>::get();
}

void CameraAppWindowManager::SetCameraUsageMonitor(
    aura::Window* window,
    mojo::PendingRemote<camera_app::mojom::CameraUsageOwnershipMonitor>
        usage_monitor,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (widget == nullptr) {
    // For some rare use cases (e.g. Launch CCA via devtool inspector), the
    // widget will be null. Therefore, returns here to let CCA know that
    // window-retaled operations is not supported.
    std::move(callback).Run(false);
    return;
  }

  mojo::Remote<camera_app::mojom::CameraUsageOwnershipMonitor> remote(
      std::move(usage_monitor));
  remote.set_disconnect_handler(
      base::BindOnce(&CameraAppWindowManager::OnMonitorMojoConnectionError,
                     base::Unretained(this), widget));
  camera_usage_monitors_.emplace(widget, std::move(remote));

  if (!widget->HasObserver(this)) {
    widget->AddObserver(this);
  }
  std::move(callback).Run(true);

  if (widget->IsVisible()) {
    OnWidgetActivationChanged(widget, true);
  }
}

void CameraAppWindowManager::SetDevToolsEnabled(bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dev_tools_enabled_ = enabled;
}

bool CameraAppWindowManager::IsDevToolsEnabled() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return dev_tools_enabled_;
}

void CameraAppWindowManager::OnWidgetVisibilityChanged(views::Widget* widget,
                                                       bool visible) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // This event will be triggered and the |visible| will be set to:
  // * True:
  //     1. When the window is restored from minimized.
  // * False:
  //     1. When the window is minimized.
  //     2. When the launcher is opened in tablet mode.
  if (visible ||
      camera_usage_monitors_.find(widget) == camera_usage_monitors_.end()) {
    return;
  }

  if (pending_transfer_.has_value() && widget == *pending_transfer_) {
    pending_transfer_ = absl::nullopt;
    // It is possible that |*pending_transfer_| == |owner_|. For example: when a
    // widget is activated while it is suspending. Therefore, we cannot return
    // here.
  }

  if (widget != owner_) {
    return;
  }

  switch (transfer_state_) {
    case TransferState::kIdle:
      SuspendCameraUsage();
      break;
    case TransferState::kSuspending:
      break;
    case TransferState::kResuming:
      pending_transfer_ = nullptr;
      break;
  }
}

void CameraAppWindowManager::OnWidgetActivationChanged(views::Widget* widget,
                                                       bool active) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // This event will be triggered and the |active| will be set to:
  // * True:
  //     1. When the window is restored from minimized.
  // * False:
  //     1. When the window is minimized.
  //     2. When the launcher is opened (half/fully).
  //     3. When the window lost focus by clicking other windows.
  //     4. When the screen is locked.
  //     5. When entering the overview mode.
  if (!active ||
      camera_usage_monitors_.find(widget) == camera_usage_monitors_.end()) {
    return;
  }

  switch (transfer_state_) {
    case TransferState::kIdle:
      if (owner_ == nullptr) {
        owner_ = widget;
        ResumeCameraUsage();
      } else if (owner_ != widget) {
        pending_transfer_ = widget;
        SuspendCameraUsage();
      }
      break;
    case TransferState::kSuspending:
      pending_transfer_ = widget;
      break;
    case TransferState::kResuming:
      if (owner_ == widget) {
        pending_transfer_ = absl::nullopt;
      } else {
        pending_transfer_ = widget;
      }
      break;
  }
}

void CameraAppWindowManager::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  widget->RemoveObserver(this);
}

CameraAppWindowManager::CameraAppWindowManager() = default;

void CameraAppWindowManager::OnMonitorMojoConnectionError(
    views::Widget* widget) {
  camera_usage_monitors_.erase(widget);

  if (pending_transfer_.has_value() && widget == *pending_transfer_) {
    pending_transfer_ = absl::nullopt;
  }
  if (widget == owner_) {
    ResumeNextOrIdle();
  }
}

void CameraAppWindowManager::SuspendCameraUsage() {
  DCHECK_NE(owner_, nullptr);
  auto it = camera_usage_monitors_.find(owner_);
  DCHECK(it != camera_usage_monitors_.end());

  transfer_state_ = TransferState::kSuspending;
  it->second->OnCameraUsageOwnershipChanged(
      false,
      base::BindRepeating(&CameraAppWindowManager::OnSuspendedCameraUsage,
                          base::Unretained(this), owner_));
}

void CameraAppWindowManager::OnSuspendedCameraUsage(views::Widget* prev_owner) {
  // TODO(crbug.com/1143535): Avoid logging here when we either have test
  // coverage to simulate the scenario which can hit this or we are confident
  // enough the case won't happen.
  if (prev_owner != owner_) {
    LOG(ERROR) << "The suspension has been interrupted.";
    return;
  }
  ResumeNextOrIdle();
}

void CameraAppWindowManager::ResumeCameraUsage() {
  DCHECK_NE(owner_, nullptr);
  auto it = camera_usage_monitors_.find(owner_);
  DCHECK(it != camera_usage_monitors_.end());

  transfer_state_ = TransferState::kResuming;
  it->second->OnCameraUsageOwnershipChanged(
      true, base::BindRepeating(&CameraAppWindowManager::OnResumedCameraUsage,
                                base::Unretained(this), owner_));
}

void CameraAppWindowManager::OnResumedCameraUsage(views::Widget* prev_owner) {
  // TODO(crbug.com/1143535): Avoid logging here when we either have test
  // coverage to simulate the scenario which can hit this or we are confident
  // enough the case won't happen.
  if (prev_owner != owner_) {
    LOG(ERROR) << "The resume has been interrupted.";
    return;
  }

  if (pending_transfer_.has_value()) {
    SuspendCameraUsage();
  } else {
    transfer_state_ = TransferState::kIdle;
  }
}

void CameraAppWindowManager::ResumeNextOrIdle() {
  auto next_owner(pending_transfer_);
  pending_transfer_ = absl::nullopt;
  if (next_owner.has_value()) {
    owner_ = *next_owner;
  } else {
    owner_ = nullptr;
  }

  if (owner_ != nullptr) {
    ResumeCameraUsage();
  } else {
    transfer_state_ = TransferState::kIdle;
  }
}

}  // namespace ash
