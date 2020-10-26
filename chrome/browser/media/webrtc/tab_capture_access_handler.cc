// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/tab_capture_access_handler.h"

#include <utility>

#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

TabCaptureAccessHandler::TabCaptureAccessHandler() {
}

TabCaptureAccessHandler::~TabCaptureAccessHandler() {
}

bool TabCaptureAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return type == blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE ||
         type == blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE;
}

bool TabCaptureAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return false;
}

void TabCaptureAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  blink::MediaStreamDevices devices;
  std::unique_ptr<content::MediaStreamUI> ui;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  extensions::TabCaptureRegistry* tab_capture_registry =
      extensions::TabCaptureRegistry::Get(profile);
  if (!tab_capture_registry) {
    NOTREACHED();
    std::move(callback).Run(
        devices, blink::mojom::MediaStreamRequestResult::INVALID_STATE,
        std::move(ui));
    return;
  }
  if (!profile->GetPrefs()->GetBoolean(prefs::kScreenCaptureAllowed)) {
    std::move(callback).Run(
        devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::move(ui));
    return;
  }
  // |extension| may be null if the tabCapture starts with
  // tabCapture.getMediaStreamId().
  // TODO(crbug.com/831722): Deprecate tabCaptureRegistry soon.
  const std::string extension_id = extension ? extension->id() : "";
  const bool tab_capture_allowed = tab_capture_registry->VerifyRequest(
      request.render_process_id, request.render_frame_id, extension_id);

  if (request.audio_type ==
          blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE &&
      tab_capture_allowed) {
    devices.push_back(blink::MediaStreamDevice(
        blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE, std::string(),
        std::string()));
  }

  if (request.video_type ==
          blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE &&
      tab_capture_allowed) {
    devices.push_back(blink::MediaStreamDevice(
        blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE, std::string(),
        std::string()));
  }

  if (!devices.empty()) {
    ui = MediaCaptureDevicesDispatcher::GetInstance()
             ->GetMediaStreamCaptureIndicator()
             ->RegisterMediaStream(web_contents, devices);
  }
  UpdateExtensionTrusted(request, extension);
  std::move(callback).Run(
      devices,
      devices.empty() ? blink::mojom::MediaStreamRequestResult::INVALID_STATE
                      : blink::mojom::MediaStreamRequestResult::OK,
      std::move(ui));
}
