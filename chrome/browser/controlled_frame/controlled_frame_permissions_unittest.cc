// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_types.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace controlled_frame {

class ControlledFramePermissionsTest : public ::testing::Test {
 public:
  ControlledFramePermissionsTest() = default;
};

// This test verifies that all of the permissions / capabilities listed here
// have been vetted as operational or disabled for Controlled Frame. When a new
// permission is being added to one of the following types, the permission
// should be evaluated in the same way to understand if changes are necessary
// to ensure that the permission works well for the Controlled Frame API.
// Once it's vetted, you can add it to the appropriate switch statement in this
// test, then seek review from a Controlled Frame code owner.
//
// Here's a list of the concerns to consider for each permission:
//
//   - Should the permission be disabled in Controlled Frame?
//     If a permission or capability does not support isolating its permission
//     grant/deny state from the default storage partition / profile, then it
//     should be disabled in Controlled Frame. This helps prevent leaking user
//     data from the default storage partition into an IWA context.
//
//   - Does the permission ensure that the embedder also handles a permission
//     request?
//     Capabilities should be able to convey their permission request state
//     into an embedding context as a way to verify that the permission should
//     be granted. This helps the Controlled Frame API's permissionrequest
//     event to work as expected.
//
//   - Does the capability require a permissions policy?
//     Controlled Frame embedders, such as Isolated Web Apps, need to have the
//     appropriate permissions policy granted as well. In order to ensure that
//     policy is granted, the capability must verify that the embedder has the
//     new policy enabled, too.
//
// For more info about adding a new permission, see
// [add_new_permission.md](https://crsrc.org/c/components/permissions/add_new_permission.md).
TEST_F(ControlledFramePermissionsTest, Verify) {
  std::vector<int> web_view_permissions;
  for (int i = WEB_VIEW_PERMISSION_TYPE_MIN_VALUE;
       i <= WEB_VIEW_PERMISSION_TYPE_MAX_VALUE; ++i) {
    switch (i) {
      case WEB_VIEW_PERMISSION_TYPE_UNKNOWN:
      case WEB_VIEW_PERMISSION_TYPE_DOWNLOAD:
      case WEB_VIEW_PERMISSION_TYPE_FILESYSTEM:
      case WEB_VIEW_PERMISSION_TYPE_FULLSCREEN:
      case WEB_VIEW_PERMISSION_TYPE_GEOLOCATION:
      case WEB_VIEW_PERMISSION_TYPE_HID:
      case WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG:
      case WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN:
      case WEB_VIEW_PERMISSION_TYPE_MEDIA:
      case WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW:
      case WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK:
        break;
      default:
        web_view_permissions.push_back(i);
    }
  }
  EXPECT_TRUE(web_view_permissions.empty())
      << "found unknown WEB_VIEW_PERMISSION_TYPE: "
      << testing::PrintToString(web_view_permissions);

  std::vector<ContentSettingsType> content_settings_types;
  for (int i = static_cast<int>(ContentSettingsType::kMinValue);
       i <= static_cast<int>(ContentSettingsType::kMaxValue); ++i) {
    switch (static_cast<ContentSettingsType>(i)) {
      case ContentSettingsType::GEOLOCATION:
      case ContentSettingsType::NOTIFICATIONS:
      case ContentSettingsType::MIDI:
      case ContentSettingsType::MIDI_SYSEX:
      case ContentSettingsType::DURABLE_STORAGE:
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::MEDIASTREAM_MIC:
      case ContentSettingsType::BACKGROUND_SYNC:
      case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      case ContentSettingsType::SENSORS:
      case ContentSettingsType::DEPRECATED_ACCESSIBILITY_EVENTS:
      case ContentSettingsType::CLIPBOARD_READ_WRITE:
      case ContentSettingsType::CLIPBOARD_SANITIZED_WRITE:
      case ContentSettingsType::PAYMENT_HANDLER:
      case ContentSettingsType::BACKGROUND_FETCH:
      case ContentSettingsType::PERIODIC_BACKGROUND_SYNC:
      case ContentSettingsType::WAKE_LOCK_SCREEN:
      case ContentSettingsType::WAKE_LOCK_SYSTEM:
      case ContentSettingsType::NFC:
      case ContentSettingsType::VR:
      case ContentSettingsType::AR:
      case ContentSettingsType::HAND_TRACKING:
      case ContentSettingsType::SMART_CARD_DATA:
      case ContentSettingsType::STORAGE_ACCESS:
      case ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      case ContentSettingsType::WINDOW_MANAGEMENT:
      case ContentSettingsType::LOCAL_FONTS:
      case ContentSettingsType::IDLE_DETECTION:
      case ContentSettingsType::DISPLAY_CAPTURE:
      case ContentSettingsType::CAPTURED_SURFACE_CONTROL:
      case ContentSettingsType::WEB_PRINTING:
      case ContentSettingsType::SPEAKER_SELECTION:
      case ContentSettingsType::KEYBOARD_LOCK:
      case ContentSettingsType::POINTER_LOCK:
      case ContentSettingsType::AUTOMATIC_FULLSCREEN:
      case ContentSettingsType::WEB_APP_INSTALLATION:
      case ContentSettingsType::USB_GUARD:
      case ContentSettingsType::SERIAL_GUARD:
      case ContentSettingsType::BLUETOOTH_GUARD:
      case ContentSettingsType::BLUETOOTH_SCANNING:
      case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      case ContentSettingsType::HID_GUARD:
      case ContentSettingsType::SMART_CARD_GUARD:
      case ContentSettingsType::DEFAULT:
      case ContentSettingsType::COOKIES:
      case ContentSettingsType::IMAGES:
      case ContentSettingsType::JAVASCRIPT:
      case ContentSettingsType::POPUPS:
      case ContentSettingsType::AUTO_SELECT_CERTIFICATE:
      case ContentSettingsType::MIXEDSCRIPT:
      case ContentSettingsType::PROTOCOL_HANDLERS:
      case ContentSettingsType::DEPRECATED_PPAPI_BROKER:
      case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      case ContentSettingsType::SSL_CERT_DECISIONS:
      case ContentSettingsType::APP_BANNER:
      case ContentSettingsType::SITE_ENGAGEMENT:
      case ContentSettingsType::USB_CHOOSER_DATA:
      case ContentSettingsType::AUTOPLAY:
      case ContentSettingsType::IMPORTANT_SITE_INFO:
      case ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA:
      case ContentSettingsType::ADS:
      case ContentSettingsType::ADS_DATA:
      case ContentSettingsType::PASSWORD_PROTECTION:
      case ContentSettingsType::MEDIA_ENGAGEMENT:
      case ContentSettingsType::SOUND:
      case ContentSettingsType::CLIENT_HINTS:
      case ContentSettingsType::INTENT_PICKER_DISPLAY:
      case ContentSettingsType::SERIAL_CHOOSER_DATA:
      case ContentSettingsType::HID_CHOOSER_DATA:
      case ContentSettingsType::LEGACY_COOKIE_ACCESS:
      case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
      case ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA:
      case ContentSettingsType::FILE_SYSTEM_READ_GUARD:
      case ContentSettingsType::INSECURE_PRIVATE_NETWORK:
      case ContentSettingsType::PERMISSION_AUTOREVOCATION_DATA:
      case ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY:
      case ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA:
      case ContentSettingsType::FEDERATED_IDENTITY_SHARING:
      case ContentSettingsType::JAVASCRIPT_JIT:
      case ContentSettingsType::HTTP_ALLOWED:
      case ContentSettingsType::FORMFILL_METADATA:
      case ContentSettingsType::DEPRECATED_FEDERATED_IDENTITY_ACTIVE_SESSION:
      case ContentSettingsType::AUTO_DARK_WEB_CONTENT:
      case ContentSettingsType::REQUEST_DESKTOP_SITE:
      case ContentSettingsType::FEDERATED_IDENTITY_API:
      case ContentSettingsType::NOTIFICATION_INTERACTIONS:
      case ContentSettingsType::REDUCED_ACCEPT_LANGUAGE:
      case ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW:
      case ContentSettingsType::PRIVATE_NETWORK_GUARD:
      case ContentSettingsType::PRIVATE_NETWORK_CHOOSER_DATA:
      case ContentSettingsType::
          FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS:
      case ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS:
      case ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION:
      case ContentSettingsType::
          FEDERATED_IDENTITY_IDENTITY_PROVIDER_REGISTRATION:
      case ContentSettingsType::ANTI_ABUSE:
      case ContentSettingsType::THIRD_PARTY_STORAGE_PARTITIONING:
      case ContentSettingsType::HTTPS_ENFORCED:
      case ContentSettingsType::ALL_SCREEN_CAPTURE:
      case ContentSettingsType::COOKIE_CONTROLS_METADATA:
      case ContentSettingsType::TPCD_HEURISTICS_GRANTS:
      case ContentSettingsType::TPCD_METADATA_GRANTS:
      case ContentSettingsType::TPCD_TRIAL:
      case ContentSettingsType::TOP_LEVEL_TPCD_TRIAL:
      case ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL:
      case ContentSettingsType::AUTO_PICTURE_IN_PICTURE:
      case ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION:
      case ContentSettingsType::FILE_SYSTEM_ACCESS_RESTORE_PERMISSION:
      case ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS:
      case ContentSettingsType::DIRECT_SOCKETS:
      case ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS:
      case ContentSettingsType::TRACKING_PROTECTION:
      case ContentSettingsType::DISPLAY_MEDIA_SYSTEM_AUDIO:
      case ContentSettingsType::JAVASCRIPT_OPTIMIZER:
      case ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL:
      case ContentSettingsType::DIRECT_SOCKETS_PRIVATE_NETWORK_ACCESS:
        break;

      default:
        content_settings_types.push_back(static_cast<ContentSettingsType>(i));
    }
  }
  EXPECT_TRUE(content_settings_types.empty())
      << "found unknown ContentSettingsType: "
      << testing::PrintToString(content_settings_types);

  std::vector<blink::PermissionType> permission_types;
  // PermissionType is a hand-written enum where ::NUM, the max value, is
  // invalid.
  for (int i = static_cast<int>(blink::PermissionType::MIN_VALUE);
       i != static_cast<int>(blink::PermissionType::NUM); ++i) {
    // PermissionType entries are marked deprecated by commenting them out.
    // Skip these now-invalid values here.
    if (i == 2 || i == 11 || i == 13 || i == 14 || i == 15 || i == 32) {
      continue;
    }
    switch (static_cast<blink::PermissionType>(i)) {
      case blink::PermissionType::MIDI_SYSEX:
      case blink::PermissionType::NOTIFICATIONS:
      case blink::PermissionType::GEOLOCATION:
      case blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      case blink::PermissionType::MIDI:
      case blink::PermissionType::DURABLE_STORAGE:
      case blink::PermissionType::AUDIO_CAPTURE:
      case blink::PermissionType::VIDEO_CAPTURE:
      case blink::PermissionType::BACKGROUND_SYNC:
      case blink::PermissionType::SENSORS:
      case blink::PermissionType::PAYMENT_HANDLER:
      case blink::PermissionType::BACKGROUND_FETCH:
      case blink::PermissionType::IDLE_DETECTION:
      case blink::PermissionType::PERIODIC_BACKGROUND_SYNC:
      case blink::PermissionType::WAKE_LOCK_SCREEN:
      case blink::PermissionType::WAKE_LOCK_SYSTEM:
      case blink::PermissionType::NFC:
      case blink::PermissionType::CLIPBOARD_READ_WRITE:
      case blink::PermissionType::CLIPBOARD_SANITIZED_WRITE:
      case blink::PermissionType::VR:
      case blink::PermissionType::AR:
      case blink::PermissionType::STORAGE_ACCESS_GRANT:
      case blink::PermissionType::CAMERA_PAN_TILT_ZOOM:
      case blink::PermissionType::WINDOW_MANAGEMENT:
      case blink::PermissionType::LOCAL_FONTS:
      case blink::PermissionType::DISPLAY_CAPTURE:
      case blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS:
      case blink::PermissionType::CAPTURED_SURFACE_CONTROL:
      case blink::PermissionType::SMART_CARD:
      case blink::PermissionType::WEB_PRINTING:
      case blink::PermissionType::SPEAKER_SELECTION:
      case blink::PermissionType::KEYBOARD_LOCK:
      case blink::PermissionType::POINTER_LOCK:
      case blink::PermissionType::AUTOMATIC_FULLSCREEN:
      case blink::PermissionType::HAND_TRACKING:
      case blink::PermissionType::WEB_APP_INSTALLATION:
        break;

      default:
        permission_types.push_back(static_cast<blink::PermissionType>(i));
    }
  }
  EXPECT_TRUE(permission_types.empty())
      << "found unknown PermissionType: "
      << testing::PrintToString(permission_types);
}

}  // namespace controlled_frame
