// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/extension_media_access_handler.h"

#include <utility>

#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace {

// This is a short-term solution to grant camera and/or microphone access to
// extensions:
// 1. Virtual keyboard extension.
// 2. Flutter gesture recognition extension.
// 3. TODO(smus): Airbender experiment 1.
// 4. TODO(smus): Airbender experiment 2.
// 5. Hotwording component extension.
// 6. XKB input method component extension.
// 7. M17n/T13n/CJK input method component extension.
// Once http://crbug.com/292856 is fixed, remove this whitelist.
bool IsMediaRequestWhitelistedForExtension(
    const extensions::Extension* extension) {
  return extension->id() == "mppnpdlheglhdfmldimlhpnegondlapf" ||
         extension->id() == "jokbpnebhdcladagohdnfgjcpejggllo" ||
         extension->id() == "clffjmdilanldobdnedchkdbofoimcgb" ||
         extension->id() == "nnckehldicaciogcbchegobnafnjkcne" ||
         extension->id() == "nbpagnldghgfoolbancepceaanlmhfmd" ||
         extension->id() == "jkghodnilhceideoidjikpgommlajknk" ||
         extension->id() == "gjaehgfemfahhmlgpdfknkhdnemmolop";
}

}  // namespace

ExtensionMediaAccessHandler::ExtensionMediaAccessHandler() {
}

ExtensionMediaAccessHandler::~ExtensionMediaAccessHandler() {
}

bool ExtensionMediaAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return extension &&
         (extension->is_platform_app() ||
          IsMediaRequestWhitelistedForExtension(extension)) &&
         (type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
          type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
}

bool ExtensionMediaAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(
      type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE
          ? extensions::APIPermission::kAudioCapture
          : extensions::APIPermission::kVideoCapture);
}

void ExtensionMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  bool audio_allowed =
      request.audio_type ==
          blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
      extension->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kAudioCapture) &&
      GetDevicePolicy(profile, extension->url(), prefs::kAudioCaptureAllowed,
                      prefs::kAudioCaptureAllowedUrls) != ALWAYS_DENY;
  bool video_allowed =
      request.video_type ==
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE &&
      extension->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kVideoCapture) &&
      GetDevicePolicy(profile, extension->url(), prefs::kVideoCaptureAllowed,
                      prefs::kVideoCaptureAllowedUrls) != ALWAYS_DENY;

  CheckDevicesAndRunCallback(web_contents, request, std::move(callback),
                             audio_allowed, video_allowed);
}
