// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_THUMBNAIL_CAPTURER_MAC_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_THUMBNAIL_CAPTURER_MAC_H_

#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/thumbnail_capturer.h"

// Returns true if the SCK thumbnail capturer is available and enabled.
bool ShouldUseThumbnailCapturerMac(DesktopMediaList::Type type);

// Creates a ThumbnailCaptureMac object. Must only be called is
// ShouldUseThumbnailCapturerMac() returns true.
std::unique_ptr<ThumbnailCapturer> CreateThumbnailCapturerMac(
    DesktopMediaList::Type type);

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_THUMBNAIL_CAPTURER_MAC_H_
