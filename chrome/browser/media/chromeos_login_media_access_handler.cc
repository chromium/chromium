// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/chromeos_login_media_access_handler.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/url_constants.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/render_frame_host.h"
#include "url/gurl.h"

ChromeOSLoginMediaAccessHandler::ChromeOSLoginMediaAccessHandler() {}

ChromeOSLoginMediaAccessHandler::~ChromeOSLoginMediaAccessHandler() {}

bool ChromeOSLoginMediaAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  if (!web_contents)
    return false;
  chromeos::LoginDisplayHost* host = chromeos::LoginDisplayHost::default_host();
  return host && web_contents == host->GetOobeWebContents();
}

bool ChromeOSLoginMediaAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  if (type != blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE)
    return false;

  // When creating new user (including supervised user), we must be able to use
  // the camera to capture a user image.
  if (security_origin.spec() == chrome::kChromeUIOobeURL)
    return true;

  const chromeos::CrosSettings* const settings = chromeos::CrosSettings::Get();
  if (!settings)
    return false;

  // The following checks are for SAML logins.
  const base::Value* const raw_list_value =
      settings->GetPref(chromeos::kLoginVideoCaptureAllowedUrls);
  if (!raw_list_value)
    return false;

  const base::ListValue* list_value;
  const bool is_list = raw_list_value->GetAsList(&list_value);
  DCHECK(is_list);
  for (const auto& base_value : *list_value) {
    std::string value;
    if (base_value.GetAsString(&value)) {
      const ContentSettingsPattern pattern =
          ContentSettingsPattern::FromString(value);
      // Force administrators to specify more-specific patterns by ignoring the
      // global wildcard pattern.
      if (pattern == ContentSettingsPattern::Wildcard()) {
        VLOG(1) << "Ignoring wildcard URL pattern: " << value;
        continue;
      }
      if (pattern.IsValid() && pattern.Matches(security_origin))
        return true;
    }
  }
  return false;
}

void ChromeOSLoginMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  bool audio_allowed = false;
  bool video_allowed =
      request.video_type ==
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE &&
      CheckMediaAccessPermission(
          content::RenderFrameHost::FromID(request.render_process_id,
                                           request.render_frame_id),
          request.security_origin,
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, extension);

  CheckDevicesAndRunCallback(web_contents, request, std::move(callback),
                             audio_allowed, video_allowed);
}
