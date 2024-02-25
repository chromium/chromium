// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/pip/arc_picture_in_picture_window_controller_impl.h"

#include "chrome/browser/ash/arc/pip/arc_pip_bridge.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/window.h"

namespace arc {

ArcPictureInPictureWindowControllerImpl::
    ArcPictureInPictureWindowControllerImpl(arc::ArcPipBridge* arc_pip_bridge)
    : arc_pip_bridge_(arc_pip_bridge) {}

ArcPictureInPictureWindowControllerImpl::
    ~ArcPictureInPictureWindowControllerImpl() {
  Close(false);
}

void ArcPictureInPictureWindowControllerImpl::Show() {
  // Should be a no-op on ARC. This is managed on the Android side.
}

void ArcPictureInPictureWindowControllerImpl::FocusInitiator() {
  // Should be a no-op on ARC. This is managed on the Android side.
}

void ArcPictureInPictureWindowControllerImpl::Close(bool should_pause_video) {
  // TODO(edcourtney): Currently, |should_pause_video| will always be false
  // here, but if that changes, we should pause the video on the Android side.
  arc_pip_bridge_->ClosePip();
}

void ArcPictureInPictureWindowControllerImpl::CloseAndFocusInitiator() {
  // Should be a no-op on ARC. This is managed on the Android side.
}

void ArcPictureInPictureWindowControllerImpl::OnWindowDestroyed(
    bool should_pause_video) {
  // Should be a no-op on ARC. This is managed on the Android side.
}

content::WebContents*
ArcPictureInPictureWindowControllerImpl::GetWebContents() {
  // Should be a no-op on ARC. This is managed on the Android side.
  return nullptr;
}

std::optional<gfx::Rect>
ArcPictureInPictureWindowControllerImpl::GetWindowBounds() {
  for (auto* window : ChromeShelfController::instance()->GetArcWindows()) {
    if (window->GetProperty(chromeos::kWindowStateTypeKey) ==
        chromeos::WindowStateType::kPip) {
      return window->GetBoundsInScreen();
    }
  }
  return std::nullopt;
}

content::WebContents*
ArcPictureInPictureWindowControllerImpl::GetChildWebContents() {
  return nullptr;
}

std::optional<url::Origin>
ArcPictureInPictureWindowControllerImpl::GetOrigin() {
  return std::nullopt;
}

}  // namespace arc
