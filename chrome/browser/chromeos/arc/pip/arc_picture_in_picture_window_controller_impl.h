// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_PIP_ARC_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_PIP_ARC_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class WebContents;
class OverlayWindow;

}  // namespace content

namespace arc {

class ArcPipBridge;

// Implementation of PictureInPictureWindowController for ARC. This does nothing
// for most of the methods, as most of it is managed on the Android side.
class ArcPictureInPictureWindowControllerImpl
    : public content::PictureInPictureWindowController {
 public:
  explicit ArcPictureInPictureWindowControllerImpl(
      arc::ArcPipBridge* arc_pip_bridge);
  ~ArcPictureInPictureWindowControllerImpl() override;

  // PictureInPictureWindowController:
  void Show() override;
  void Close(bool should_pause_video) override;
  void CloseAndFocusInitiator() override;
  void OnWindowDestroyed() override;
  content::OverlayWindow* GetWindowForTesting() override;
  void UpdateLayerBounds() override;
  bool IsPlayerActive() override;
  content::WebContents* GetInitiatorWebContents() override;
  bool TogglePlayPause() override;
  void UpdatePlaybackState(bool is_playing,
                           bool reached_end_of_stream) override;
  void SetAlwaysHidePlayPauseButton(bool is_visible) override;
  void SkipAd() override;
  void NextTrack() override;
  void PreviousTrack() override;

 private:
  arc::ArcPipBridge* const arc_pip_bridge_;

  DISALLOW_COPY_AND_ASSIGN(ArcPictureInPictureWindowControllerImpl);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_PIP_ARC_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
