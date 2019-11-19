// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_DEVICES_UTIL_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_DEVICES_UTIL_H_

#include <memory>

#include "base/strings/string_util.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

// Helper to get list of media stream devices for desktop capture in |devices|.
// Registers to display notification if |display_notification| is true.
// Returns an instance of MediaStreamUI to be passed to content layer.
std::unique_ptr<content::MediaStreamUI> GetDevicesForDesktopCapture(
    content::WebContents* web_contents,
    blink::MediaStreamDevices* devices,
    const content::DesktopMediaID& media_id,
    blink::mojom::MediaStreamType devices_video_type,
    blink::mojom::MediaStreamType devices_audio_type,
    bool capture_audio,
    bool disable_local_echo,
    bool display_notification,
    const base::string16& application_title,
    const base::string16& registered_extension_name);

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_DEVICES_UTIL_H_
