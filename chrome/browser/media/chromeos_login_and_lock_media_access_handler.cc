// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/chromeos_login_and_lock_media_access_handler.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_dialogs.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/render_frame_host.h"
#include "url/gurl.h"

ChromeOSLoginAndLockMediaAccessHandler::
    ChromeOSLoginAndLockMediaAccessHandler() = default;

ChromeOSLoginAndLockMediaAccessHandler::
    ~ChromeOSLoginAndLockMediaAccessHandler() = default;

bool ChromeOSLoginAndLockMediaAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  if (!web_contents)
    return false;
  // Check if the `web_contents` corresponds to the login screen.
  auto* host = ash::LoginDisplayHost::default_host();
  if (host && web_contents == host->GetOobeWebContents()) {
    return true;
  }
  // Check if the `web_contents` corresponds to the reauthentication dialog that
  // is shown on the lock screen.
  ash::LockScreenStartReauthDialog* lock_screen_online_reauth_dialog =
      ash::LockScreenStartReauthDialog::GetInstance();
  return !!lock_screen_online_reauth_dialog &&
         web_contents == lock_screen_online_reauth_dialog->GetWebContents();
}

bool ChromeOSLoginAndLockMediaAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  if (type != blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE)
    return false;

  const ash::CrosSettings* const settings = ash::CrosSettings::Get();
  if (!settings)
    return false;

  // The following checks are for SAML logins.
  const base::Value::List* allowed_urls_list;
  if (!settings->GetList(ash::kLoginVideoCaptureAllowedUrls,
                         &allowed_urls_list))
    return false;

  for (const auto& base_value : *allowed_urls_list) {
    const std::string* value = base_value.GetIfString();
    if (value) {
      const ContentSettingsPattern pattern =
          ContentSettingsPattern::FromString(*value);
      // Force administrators to specify more-specific patterns by ignoring the
      // global wildcard pattern.
      if (pattern == ContentSettingsPattern::Wildcard()) {
        VLOG(1) << "Ignoring wildcard URL pattern: " << *value;
        continue;
      }
      if (pattern.IsValid() && pattern.Matches(security_origin.GetURL())) {
        return true;
      }
    }
  }
  return false;
}

void ChromeOSLoginAndLockMediaAccessHandler::HandleRequest(
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
          request.url_origin,
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, extension);

  CheckDevicesAndRunCallback(web_contents, request, std::move(callback),
                             audio_allowed, video_allowed);
}
