// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/extension_media_access_handler.h"

#include <utility>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/utils/extension_utils.h"

using extensions::mojom::APIPermissionID;

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
// 8. Accessibility Common extension (used for Dictation)
// 9. Dictation Connector component extension.
// Once http://crbug.com/40333126 is fixed, remove this allowlist.
// Note that if an extension is included here, then the permission request is
// evaluated based on whether the extension has audioCapture or videoCapture
// permission. If it's not included here, then the request is handled by
// other means (e.g. it could still be granted by showing a permission prompt to
// the user).
bool IsMediaRequestHandledByManifestForExtension(
    const extensions::Extension* extension) {
  if (extensions::IsExtensionAllowlistedByCommandLine(*extension)) {
    // The extension is granted broad extension permissions for testing
    // (including the audio/video capture permissions), so have the extension
    // system handle the request.
    return true;
  }

  return extension->id() == extension_misc::kKeyboardExtensionId ||
         extension->id() == "jokbpnebhdcladagohdnfgjcpejggllo" ||
         extension->id() == "clffjmdilanldobdnedchkdbofoimcgb" ||
         extension->id() == "nnckehldicaciogcbchegobnafnjkcne" ||
         extension->id() == "nbpagnldghgfoolbancepceaanlmhfmd" ||
         extension->id() == "jkghodnilhceideoidjikpgommlajknk" ||
         extension->id() == "gjaehgfemfahhmlgpdfknkhdnemmolop" ||
         extension->id() == "egfdjlfmgnehecnclamagfafdccgfndp" ||
         extension->id() == extension_misc::kDictationConnectorExtensionId;
}

}  // namespace

ExtensionMediaAccessHandler::ExtensionMediaAccessHandler() = default;

ExtensionMediaAccessHandler::~ExtensionMediaAccessHandler() = default;

bool ExtensionMediaAccessHandler::SupportsStreamType(
    content::RenderFrameHost* render_frame_host,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return extension &&
         (extension->is_platform_app() ||
          IsMediaRequestHandledByManifestForExtension(extension)) &&
         (type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
          type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
}

bool ExtensionMediaAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(
      type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE
          ? APIPermissionID::kAudioCapture
          : APIPermissionID::kVideoCapture);
}

void ExtensionMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  content_settings::SettingInfo audio_info;
  PermissionSetting audio_setting = map->GetPermissionSetting(
      extension->url(), extension->url(), ContentSettingsType::MEDIASTREAM_MIC,
      &audio_info);
  auto* audio_permission_info =
      content_settings::PermissionSettingsRegistry::GetInstance()->Get(
          ContentSettingsType::MEDIASTREAM_MIC);
  bool audio_blocked_by_policy =
      audio_info.source == content_settings::SettingSource::kPolicy &&
      audio_permission_info->delegate().IsBlocked(audio_setting);
  bool audio_allowed =
      request.audio_type ==
          blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
      extension->permissions_data()->HasAPIPermission(
          APIPermissionID::kAudioCapture) &&
      !audio_blocked_by_policy;

  content_settings::SettingInfo video_info;
  PermissionSetting video_setting = map->GetPermissionSetting(
      extension->url(), extension->url(),
      ContentSettingsType::MEDIASTREAM_CAMERA, &video_info);
  auto* video_permission_info =
      content_settings::PermissionSettingsRegistry::GetInstance()->Get(
          ContentSettingsType::MEDIASTREAM_CAMERA);
  bool video_blocked_by_policy =
      video_info.source == content_settings::SettingSource::kPolicy &&
      video_permission_info->delegate().IsBlocked(video_setting);
  bool video_allowed =
      request.video_type ==
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE &&
      extension->permissions_data()->HasAPIPermission(
          APIPermissionID::kVideoCapture) &&
      !video_blocked_by_policy;

  CheckDevicesAndRunCallback(web_contents, request, std::move(callback),
                             audio_allowed, video_allowed);
}
