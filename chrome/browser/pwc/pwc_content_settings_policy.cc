// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/pwc_content_settings_policy.h"

namespace pwc {

PwcContentSettingPolicy GetPwcContentSettingPolicy(ContentSettingsType type) {
  switch (type) {
    // Web platform permissions queried through PermissionManager and delegated
    // to the component embedder delegate (e.g. GlicPwcPermissionDelegate). If
    // the delegate does not explicitly handle or grant them,
    // PrivilegedWebContents::GetPermissionStatus() defaults to DENIED with
    // FEATURE_POLICY.
    //
    // These correspond to the types recognized by
    // permissions::PermissionUtil::IsPermission().
    case ContentSettingsType::GEOLOCATION:
    case ContentSettingsType::GEOLOCATION_WITH_OPTIONS:
    case ContentSettingsType::NOTIFICATIONS:
    case ContentSettingsType::MIDI:
    case ContentSettingsType::MIDI_SYSEX:
    case ContentSettingsType::PERSISTENT_STORAGE:
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::BACKGROUND_SYNC:
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
    case ContentSettingsType::SENSORS:
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
    case ContentSettingsType::LOCAL_NETWORK_ACCESS:
    case ContentSettingsType::LOCAL_NETWORK:
    case ContentSettingsType::LOOPBACK_NETWORK:
      return PwcContentSettingPolicy::kDelegated;

    // Site settings that are not web permissions and are never queried through
    // PermissionManager. PrivilegedWebContents bypasses or enforces these
    // elsewhere:
    // - COOKIES: Force-allowed via CookieSettingOverride::kForceAllowCookies in
    //   NavigationURLLoaderImpl/RenderFrameHostImpl and DOM storage access in
    //   ContentSettingsManagerImpl.
    // - JAVASCRIPT_JIT, JAVASCRIPT_OPTIMIZER: User site settings are skipped in
    //   SiteInfo::Create for privileged WebContents.
    // - JAVASCRIPT, IMAGES: Handled via renderer rules; PWC does not attach
    //   PageSpecificContentSettings to block execution.
    // - ANTI_ABUSE: Website setting for Trust Tokens, not a permission.
    case ContentSettingsType::COOKIES:
    case ContentSettingsType::IMAGES:
    case ContentSettingsType::JAVASCRIPT:
    case ContentSettingsType::JAVASCRIPT_JIT:
    case ContentSettingsType::JAVASCRIPT_OPTIMIZER:
    case ContentSettingsType::ANTI_ABUSE:

    // Device and API chooser guards, window creation, and feature controls
    // not queried through PermissionManager (handled by embedder delegates,
    // dedicated ObjectPermissionContextBase choosers, or blocked in PWC).
    case ContentSettingsType::POPUPS:
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
    case ContentSettingsType::PROTOCOL_HANDLERS:
    case ContentSettingsType::BLUETOOTH_GUARD:
    case ContentSettingsType::USB_GUARD:
    case ContentSettingsType::SERIAL_GUARD:
    case ContentSettingsType::HID_GUARD:
    case ContentSettingsType::BLUETOOTH_SCANNING:
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
    case ContentSettingsType::FILE_SYSTEM_READ_GUARD:
    case ContentSettingsType::DISPLAY_MEDIA_SYSTEM_AUDIO:
    case ContentSettingsType::ALL_SCREEN_CAPTURE:
    case ContentSettingsType::SMART_CARD_GUARD:
    case ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS:
    case ContentSettingsType::SUB_APPS_WITHOUT_PROMPTS:
    case ContentSettingsType::DIRECT_SOCKETS:
    case ContentSettingsType::CONTROLLED_FRAME:
    case ContentSettingsType::AUTO_PICTURE_IN_PICTURE:
    case ContentSettingsType::AUTOPLAY:
    case ContentSettingsType::MIXEDSCRIPT:
    case ContentSettingsType::SOUND:
    case ContentSettingsType::ADS:
    case ContentSettingsType::AUTO_DARK_WEB_CONTENT:
    case ContentSettingsType::REQUEST_DESKTOP_SITE:
    case ContentSettingsType::DEVICE_ATTRIBUTES:
    case ContentSettingsType::INLINE_CUE_MENU:
    case ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION:
    case ContentSettingsType::FILE_SYSTEM_ACCESS_RESTORE_PERMISSION:
    case ContentSettingsType::FEDERATED_IDENTITY_API:
    case ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION:

    // Internal metadata, chooser data, and heuristics cache entries that are
    // not permission types and are never queried through PermissionManager.
    case ContentSettingsType::DEFAULT:
    case ContentSettingsType::AUTO_SELECT_CERTIFICATE:
    case ContentSettingsType::SSL_CERT_DECISIONS:
    case ContentSettingsType::APP_BANNER:
    case ContentSettingsType::SITE_ENGAGEMENT:
    case ContentSettingsType::USB_CHOOSER_DATA:
    case ContentSettingsType::IMPORTANT_SITE_INFO:
    case ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA:
    case ContentSettingsType::ADS_DATA:
    case ContentSettingsType::PASSWORD_PROTECTION:
    case ContentSettingsType::MEDIA_ENGAGEMENT:
    case ContentSettingsType::CLIENT_HINTS:
    case ContentSettingsType::INTENT_PICKER_DISPLAY:
    case ContentSettingsType::SERIAL_CHOOSER_DATA:
    case ContentSettingsType::HID_CHOOSER_DATA:
    case ContentSettingsType::LEGACY_COOKIE_ACCESS:
    case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
    case ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA:
    case ContentSettingsType::PERMISSION_AUTOREVOCATION_DATA:
    case ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY:
    case ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA:
    case ContentSettingsType::FEDERATED_IDENTITY_SHARING:
    case ContentSettingsType::HTTP_ALLOWED:
    case ContentSettingsType::FORMFILL_METADATA:
    case ContentSettingsType::NOTIFICATION_INTERACTIONS:
    case ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW:
    case ContentSettingsType::
        FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS:
    case ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS:
    case ContentSettingsType::FEDERATED_IDENTITY_IDENTITY_PROVIDER_REGISTRATION:
    case ContentSettingsType::HTTPS_ENFORCED:
    case ContentSettingsType::COOKIE_CONTROLS_METADATA:
    case ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS:
    case ContentSettingsType::LEGACY_COOKIE_SCOPE:
    case ContentSettingsType::ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER:
    case ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS:
    case ContentSettingsType::ON_DEVICE_SPEECH_RECOGNITION_LANGUAGES_DOWNLOADED:
    case ContentSettingsType::INITIALIZED_TRANSLATIONS:
    case ContentSettingsType::SUSPICIOUS_NOTIFICATION_IDS:
    case ContentSettingsType::PERMISSION_ACTIONS_HISTORY:
    case ContentSettingsType::SUSPICIOUS_NOTIFICATION_SHOW_ORIGINAL:
    case ContentSettingsType::SUSPICIOUS_SITE_WARNING_DATA:
    case ContentSettingsType::WEBAUTHN_AMBIENT_SIGNIN_DATA:
      return PwcContentSettingPolicy::kBlocked;
  }
}

}  // namespace pwc
