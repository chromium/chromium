// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_THUMBNAIL_CAPTURER_MAC_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_THUMBNAIL_CAPTURER_MAC_H_

#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/thumbnail_capturer.h"

// Returns true if the SCK thumbnail capturer is available and enabled.
bool ShouldUseThumbnailCapturerMac(DesktopMediaList::Type type);

#include "base/functional/callback_helpers.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
struct DesktopMediaID;
class WebContents;
}  // namespace content

// Used for dependency injection.
using PipWebContentsGetter = base::RepeatingCallback<content::WebContents*()>;
using PipWindowToExcludeForScreenCaptureGetter =
    base::RepeatingCallback<std::optional<content::DesktopMediaID::Id>(
        content::DesktopMediaID::Id)>;

// Creates a ThumbnailCapturerMac object. Must only be called if
// ShouldUseThumbnailCapturerMac() returns true. `web_contents` is used to
// determine if a potential PiP window should be excluded from the thumbnail or
// not.
std::unique_ptr<ThumbnailCapturer> CreateThumbnailCapturerMac(
    DesktopMediaList::Type type,
    content::WebContents* web_contents);

std::unique_ptr<ThumbnailCapturer> CreateThumbnailCapturerMacForTesting(
    DesktopMediaList::Type type,
    content::GlobalRenderFrameHostId render_frame_host_id =
        content::GlobalRenderFrameHostId(),
    PipWebContentsGetter pip_web_contents_getter = base::NullCallback(),
    PipWindowToExcludeForScreenCaptureGetter
        pip_window_to_exclude_for_screen_capture_getter = base::NullCallback());

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_THUMBNAIL_CAPTURER_MAC_H_
