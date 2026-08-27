// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_VIDEO_OVERLAY_WINDOW_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_VIDEO_OVERLAY_WINDOW_H_

#include <memory>

#include "content/public/browser/overlay_window.h"

namespace content {
class VideoPictureInPictureWindowController;
}  // namespace content

// Creates a platform-specific VideoOverlayWindow for video Picture-in-Picture.
std::unique_ptr<content::VideoOverlayWindow> CreateVideoOverlayWindow(
    content::VideoPictureInPictureWindowController* controller);

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_VIDEO_OVERLAY_WINDOW_H_
