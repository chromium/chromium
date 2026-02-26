// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_H_

#include "build/build_config.h"

// A PictureInPictureWindow is an always-on-top window that displays content to
// the user. There are two types of PictureInPictureWindows: video
// picture-in-picture windows (`VideoOverlayWindowViews`) and document
// picture-in-picture (`PictureInPictureBrowserFrameView`). This class has
// shared logic for both to be controlled directly by the
// PictureInPictureWindowManager.
class PictureInPictureWindow {
 public:
  // When `tuck` is true, this forces the PictureInPictureWindow to be tucked
  // offscreen. When `tuck` is false, it returns the PictureInPictureWindow to
  // its original position.
  virtual void SetForcedTucking(bool tuck) = 0;

#if BUILDFLAG(IS_MAC)
  // Called when any browser window has entered fullscreen. This gives the
  // picture-in-picture window a chance to ensure that it is displayed on top of
  // the fullscreen window.
  virtual void OnAnyBrowserEnteredFullscreen() = 0;
#endif  // BUILDFLAG(IS_MAC)

 protected:
  PictureInPictureWindow() = default;
  virtual ~PictureInPictureWindow() = default;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_H_
