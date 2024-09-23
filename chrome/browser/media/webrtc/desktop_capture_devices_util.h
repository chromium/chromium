// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_DEVICES_UTIL_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_DEVICES_UTIL_H_

#include <memory>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

BASE_DECLARE_FEATURE(kSuppressLocalAudioPlaybackForSystemAudio);

// Helper to get the list of media stream devices for desktop capture and store
// them in |out_devices|. Registers to display notification if
// |display_notification| is true. Returns an instance of MediaStreamUI to be
// passed to content layer.
std::unique_ptr<content::MediaStreamUI> GetDevicesForDesktopCapture(
    const content::MediaStreamRequest& request,
    content::WebContents* web_contents,
    const content::DesktopMediaID& media_id,
    bool capture_audio,
    bool disable_local_echo,
    bool suppress_local_audio_playback,
    bool display_notification,
    const std::u16string& application_title,
    bool captured_surface_control_active,
    blink::mojom::StreamDevices& out_devices);

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_DEVICES_UTIL_H_
