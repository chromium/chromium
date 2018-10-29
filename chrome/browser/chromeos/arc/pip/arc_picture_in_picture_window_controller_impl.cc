// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/pip/arc_picture_in_picture_window_controller_impl.h"

#include "chrome/browser/chromeos/arc/pip/arc_pip_bridge.h"

namespace arc {

ArcPictureInPictureWindowControllerImpl::
    ArcPictureInPictureWindowControllerImpl(arc::ArcPipBridge* arc_pip_bridge)
    : arc_pip_bridge_(arc_pip_bridge) {}

ArcPictureInPictureWindowControllerImpl::
    ~ArcPictureInPictureWindowControllerImpl() {
  Close(false, false);
}

gfx::Size ArcPictureInPictureWindowControllerImpl::Show() {
  // Should be a no-op on ARC. This is managed on the Android side.
  return gfx::Size();
}

void ArcPictureInPictureWindowControllerImpl::Close(
    bool should_pause_video,
    bool should_reset_pip_player) {
  // TODO(edcourtney): Currently, |should_pause_video| will always be false
  // here, but if that changes, we should pause the video on the Android side.
  arc_pip_bridge_->ClosePip();
}

void ArcPictureInPictureWindowControllerImpl::OnWindowDestroyed() {
  // Should be a no-op on ARC. This is managed on the Android side.
}

void ArcPictureInPictureWindowControllerImpl::SetPictureInPictureCustomControls(
    const std::vector<blink::PictureInPictureControlInfo>& info) {
  // Should be a no-op on ARC. This is managed on the Android side.
}

void ArcPictureInPictureWindowControllerImpl::EmbedSurface(
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  // Should be a no-op on ARC. This is managed on the Android side.
}

content::OverlayWindow*
ArcPictureInPictureWindowControllerImpl::GetWindowForTesting() {
  // Should be a no-op on ARC. This is managed on the Android side.
  return nullptr;
}

void ArcPictureInPictureWindowControllerImpl::UpdateLayerBounds() {
  // Should be a no-op on ARC. This is managed on the Android side.
}

bool ArcPictureInPictureWindowControllerImpl::IsPlayerActive() {
  // Should be a no-op on ARC. This is managed on the Android side.
  return false;
}

content::WebContents*
ArcPictureInPictureWindowControllerImpl::GetInitiatorWebContents() {
  // Should be a no-op on ARC. This is managed on the Android side.
  return nullptr;
}

void ArcPictureInPictureWindowControllerImpl::UpdatePlaybackState(
    bool is_playing,
    bool reached_end_of_stream) {
  // Should be a no-op on ARC. This is managed on the Android side.
}

bool ArcPictureInPictureWindowControllerImpl::TogglePlayPause() {
  // Should be a no-op on ARC. This is managed on the Android side.
  return false;
}

void ArcPictureInPictureWindowControllerImpl::CustomControlPressed(
    const std::string& control_id) {
  // Should be a no-op on ARC. This is managed on the Android side.
}

void ArcPictureInPictureWindowControllerImpl::SetAlwaysHidePlayPauseButton(
    bool is_visible) {
  // Should be a no-op on ARC. This is managed on the Android side.
}

}  // namespace arc
