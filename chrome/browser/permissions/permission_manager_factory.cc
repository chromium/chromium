// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager_factory.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/accessibility_permission_context.h"
#include "chrome/browser/background_fetch/background_fetch_permission_context.h"
#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/display_capture/display_capture_permission_context.h"
#include "chrome/browser/generic_sensor/sensor_permission_context.h"
#include "chrome/browser/idle/idle_detection_permission_context.h"
#include "chrome/browser/media/midi_permission_context.h"
#include "chrome/browser/media/midi_sysex_permission_context.h"
#include "chrome/browser/media/webrtc/camera_pan_tilt_zoom_permission_context.h"
#include "chrome/browser/media/webrtc/media_stream_device_permission_context.h"
#include "chrome/browser/nfc/chrome_nfc_permission_context_delegate.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/storage/durable_storage_permission_context.h"
#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/wake_lock/wake_lock_permission_context.h"
#include "chrome/browser/window_placement/window_placement_permission_context.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/background_sync/background_sync_permission_context.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/contexts/clipboard_read_write_permission_context.h"
#include "components/permissions/contexts/clipboard_sanitized_write_permission_context.h"
#include "components/permissions/contexts/file_handling_permission_context.h"
#include "components/permissions/contexts/font_access_permission_context.h"
#include "components/permissions/contexts/payment_handler_permission_context.h"
#include "components/permissions/contexts/webxr_permission_context.h"
#include "components/permissions/permission_manager.h"
#include "ppapi/buildflags/buildflags.h"

#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/geolocation/geolocation_permission_context_delegate_android.h"
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/permissions/contexts/nfc_permission_context_android.h"
#else
#include "chrome/browser/geolocation/geolocation_permission_context_delegate.h"
#include "components/permissions/contexts/geolocation_permission_context.h"
#include "components/permissions/contexts/nfc_permission_context.h"
#endif

namespace {
permissions::PermissionManager::PermissionContextMap CreatePermissionContexts(
    Profile* profile) {
  permissions::PermissionManager::PermissionContextMap permission_contexts;
  permission_contexts[ContentSettingsType::MIDI_SYSEX] =
      std::make_unique<MidiSysexPermissionContext>(profile);
  permission_contexts[ContentSettingsType::MIDI] =
      std::make_unique<MidiPermissionContext>(profile);
  permission_contexts[ContentSettingsType::NOTIFICATIONS] =
      std::make_unique<NotificationPermissionContext>(profile);
#if !defined(OS_ANDROID)
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<permissions::GeolocationPermissionContext>(
          profile,
          std::make_unique<GeolocationPermissionContextDelegate>(profile));
#else
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<permissions::GeolocationPermissionContextAndroid>(
          profile,
          std::make_unique<GeolocationPermissionContextDelegateAndroid>(
              profile));
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_ANDROID)
  permission_contexts[ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER] =
      std::make_unique<ProtectedMediaIdentifierPermissionContext>(profile);
#endif
  permission_contexts[ContentSettingsType::DURABLE_STORAGE] =
      std::make_unique<DurableStoragePermissionContext>(profile);
  permission_contexts[ContentSettingsType::MEDIASTREAM_MIC] =
      std::make_unique<MediaStreamDevicePermissionContext>(
          profile, ContentSettingsType::MEDIASTREAM_MIC);
  permission_contexts[ContentSettingsType::MEDIASTREAM_CAMERA] =
      std::make_unique<MediaStreamDevicePermissionContext>(
          profile, ContentSettingsType::MEDIASTREAM_CAMERA);
  permission_contexts[ContentSettingsType::BACKGROUND_SYNC] =
      std::make_unique<BackgroundSyncPermissionContext>(profile);
  permission_contexts[ContentSettingsType::SENSORS] =
      std::make_unique<SensorPermissionContext>(profile);
  permission_contexts[ContentSettingsType::ACCESSIBILITY_EVENTS] =
      std::make_unique<AccessibilityPermissionContext>(profile);
  permission_contexts[ContentSettingsType::CLIPBOARD_READ_WRITE] =
      std::make_unique<permissions::ClipboardReadWritePermissionContext>(
          profile);
  permission_contexts[ContentSettingsType::CLIPBOARD_SANITIZED_WRITE] =
      std::make_unique<permissions::ClipboardSanitizedWritePermissionContext>(
          profile);
  permission_contexts[ContentSettingsType::PAYMENT_HANDLER] =
      std::make_unique<payments::PaymentHandlerPermissionContext>(profile);
  permission_contexts[ContentSettingsType::BACKGROUND_FETCH] =
      std::make_unique<BackgroundFetchPermissionContext>(profile);
  permission_contexts[ContentSettingsType::IDLE_DETECTION] =
      std::make_unique<IdleDetectionPermissionContext>(profile);
  permission_contexts[ContentSettingsType::PERIODIC_BACKGROUND_SYNC] =
      std::make_unique<PeriodicBackgroundSyncPermissionContext>(profile);
  permission_contexts[ContentSettingsType::WAKE_LOCK_SCREEN] =
      std::make_unique<WakeLockPermissionContext>(
          profile, ContentSettingsType::WAKE_LOCK_SCREEN);
  permission_contexts[ContentSettingsType::WAKE_LOCK_SYSTEM] =
      std::make_unique<WakeLockPermissionContext>(
          profile, ContentSettingsType::WAKE_LOCK_SYSTEM);
  auto nfc_delegate = std::make_unique<ChromeNfcPermissionContextDelegate>();
#if !defined(OS_ANDROID)
  permission_contexts[ContentSettingsType::NFC] =
      std::make_unique<permissions::NfcPermissionContext>(
          profile, std::move(nfc_delegate));
#else
  permission_contexts[ContentSettingsType::NFC] =
      std::make_unique<permissions::NfcPermissionContextAndroid>(
          profile, std::move(nfc_delegate));
#endif
  permission_contexts[ContentSettingsType::VR] =
      std::make_unique<permissions::WebXrPermissionContext>(
          profile, ContentSettingsType::VR);
  permission_contexts[ContentSettingsType::AR] =
      std::make_unique<permissions::WebXrPermissionContext>(
          profile, ContentSettingsType::AR);
  permission_contexts[ContentSettingsType::STORAGE_ACCESS] =
      std::make_unique<StorageAccessGrantPermissionContext>(profile);
  permission_contexts[ContentSettingsType::CAMERA_PAN_TILT_ZOOM] =
      std::make_unique<CameraPanTiltZoomPermissionContext>(profile);
  permission_contexts[ContentSettingsType::WINDOW_PLACEMENT] =
      std::make_unique<WindowPlacementPermissionContext>(profile);
  permission_contexts[ContentSettingsType::FONT_ACCESS] =
      std::make_unique<FontAccessPermissionContext>(profile);
  permission_contexts[ContentSettingsType::DISPLAY_CAPTURE] =
      std::make_unique<DisplayCapturePermissionContext>(profile);
  permission_contexts[ContentSettingsType::FILE_HANDLING] =
      std::make_unique<FileHandlingPermissionContext>(profile);
  return permission_contexts;
}
}  // namespace

// static
permissions::PermissionManager* PermissionManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<permissions::PermissionManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PermissionManagerFactory* PermissionManagerFactory::GetInstance() {
  return base::Singleton<PermissionManagerFactory>::get();
}

PermissionManagerFactory::PermissionManagerFactory()
    : BrowserContextKeyedServiceFactory(
        "PermissionManagerFactory",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PermissionManagerFactory::~PermissionManagerFactory() {
}

KeyedService* PermissionManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new permissions::PermissionManager(profile,
                                            CreatePermissionContexts(profile));
}

content::BrowserContext*
PermissionManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
