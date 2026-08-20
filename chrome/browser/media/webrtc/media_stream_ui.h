// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_UI_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_UI_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

// Interface to display custom UI during screen-capture (tab/window/screen).
class MediaStreamUI {
 public:
  // Called when stream capture is stopped.
  virtual ~MediaStreamUI() = default;

  // Called when screen capture starts.
  // |stop_callback| is a callback to stop the stream.
  // |source_callback| is a callback to change the desktop capture source.
  // Returns the platform-dependent window ID for the UI, or 0 if not
  // applicable.
  // |media_ids| represent the display-surfaces whose capture has started.
  virtual gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) = 0;

  // Called when Region Capture starts/stops, or when the cropped area changes.
  virtual void OnRegionCaptureRectChanged(
      const std::optional<gfx::Rect>& region_capture_rect) {}
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_UI_H_
