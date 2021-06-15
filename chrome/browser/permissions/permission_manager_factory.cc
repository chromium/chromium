// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager_factory.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/background_fetch/background_fetch_permission_context.h"
#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/display_capture/display_capture_permission_context.h"
#include "chrome/browser/geolocation/geolocation_permission_context_delegate.h"
#include "chrome/browser/idle/idle_detection_permission_context.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_device_permission_context.h"
#include "chrome/browser/nfc/chrome_nfc_permission_context_delegate.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/storage/durable_storage_permission_context.h"
#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/window_placement/window_placement_permission_context.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/background_sync/background_sync_permission_context.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/contexts/accessibility_permission_context.h"
#include "components/permissions/contexts/camera_pan_tilt_zoom_permission_context.h"
#include "components/permissions/contexts/clipboard_read_write_permission_context.h"
#include "components/permissions/contexts/clipboard_sanitized_write_permission_context.h"
#include "components/permissions/contexts/font_access_permission_context.h"
#include "components/permissions/contexts/geolocation_permission_context.h"
#include "components/permissions/contexts/midi_permission_context.h"
#include "components/permissions/contexts/midi_sysex_permission_context.h"
#include "components/permissions/contexts/payment_handler_permission_context.h"
#include "components/permissions/contexts/sensor_permission_context.h"
#include "components/permissions/contexts/wake_lock_permission_context.h"
#include "components/permissions/contexts/webxr_permission_context.h"
#include "components/permissions/permission_manager.h"
#include "ppapi/buildflags/buildflags.h"

#if defined(OS_ANDROID) || defined(OS_CHROMEOS) || defined(OS_WIN)
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#endif  // defined(OS_ANDROID) || defined(OS_CHROMEOS) || defined(OS_WIN)

#if defined(OS_ANDROID)
#include "chrome/browser/geolocation/geolocation_permission_context_delegate_android.h"
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/permissions/contexts/nfc_permission_context_android.h"
#else
#include "chrome/browser/web_applications/components/file_handling_permission_context.h"
#include "components/permissions/contexts/nfc_permission_context.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_MAC)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/permissions/contexts/geolocation_permission_context_mac.h"
#endif  // defined(OS_MAC)

namespace {

permissions::PermissionManager::PermissionContextMap CreatePermissionContexts(
    Profile* profile) {
  permissions::PermissionManager::PermissionContextMap permission_contexts;

  // TODO(crbug.com/1218451): Move common permission contexts into
  // //components/embedder_support and share this code with //weblayer.
  permission_contexts[ContentSettingsType::ACCESSIBILITY_EVENTS] =
      std::make_unique<permissions::AccessibilityPermissionContext>(profile);
  permission_contexts[ContentSettingsType::BACKGROUND_SYNC] =
      std::make_unique<BackgroundSyncPermissionContext>(profile);
  permission_contexts[ContentSettingsType::CAMERA_PAN_TILT_ZOOM] =
      std::make_unique<permissions::CameraPanTiltZoomPermissionContext>(
          profile, MediaCaptureDevicesDispatcher::GetInstance());
  permission_contexts[ContentSettingsType::CLIPBOARD_READ_WRITE] =
      std::make_unique<permissions::ClipboardReadWritePermissionContext>(
          profile);
  permission_contexts[ContentSettingsType::CLIPBOARD_SANITIZED_WRITE] =
      std::make_unique<permissions::ClipboardSanitizedWritePermissionContext>(
          profile);
#if defined(OS_ANDROID)
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<permissions::GeolocationPermissionContextAndroid>(
          profile,
          std::make_unique<GeolocationPermissionContextDelegateAndroid>(
              profile));
#elif defined(OS_MAC)
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<permissions::GeolocationPermissionContextMac>(
          profile,
          std::make_unique<GeolocationPermissionContextDelegate>(profile),
          g_browser_process->platform_part()->geolocation_manager());
#else
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<permissions::GeolocationPermissionContext>(
          profile,
          std::make_unique<GeolocationPermissionContextDelegate>(profile));
#endif
  permission_contexts[ContentSettingsType::MIDI] =
      std::make_unique<permissions::MidiPermissionContext>(profile);
  permission_contexts[ContentSettingsType::MIDI_SYSEX] =
      std::make_unique<permissions::MidiSysexPermissionContext>(profile);
  auto nfc_delegate = std::make_unique<ChromeNfcPermissionContextDelegate>();
#if defined(OS_ANDROID)
  permission_contexts[ContentSettingsType::NFC] =
      std::make_unique<permissions::NfcPermissionContextAndroid>(
          profile, std::move(nfc_delegate));
#else
  permission_contexts[ContentSettingsType::NFC] =
      std::make_unique<permissions::NfcPermissionContext>(
          profile, std::move(nfc_delegate));
#endif  // defined(OS_ANDROID)
  permission_contexts[ContentSettingsType::PAYMENT_HANDLER] =
      std::make_unique<payments::PaymentHandlerPermissionContext>(profile);
  permission_contexts[ContentSettingsType::SENSORS] =
      std::make_unique<permissions::SensorPermissionContext>(profile);
  permission_contexts[ContentSettingsType::WAKE_LOCK_SCREEN] =
      std::make_unique<permissions::WakeLockPermissionContext>(
          profile, ContentSettingsType::WAKE_LOCK_SCREEN);
  permission_contexts[ContentSettingsType::WAKE_LOCK_SYSTEM] =
      std::make_unique<permissions::WakeLockPermissionContext>(
          profile, ContentSettingsType::WAKE_LOCK_SYSTEM);

  // Add additional Chrome specific permission contexts. Please add a comment
  // when adding new contexts here explaining why it can't be shared with other
  // Content embedders by adding it to CreateDefaultPermissionContexts().

  // TODO(crbug.com/1057106): Move this to default contexts once WebLayer
  // supports WebXR.
  permission_contexts[ContentSettingsType::AR] =
      std::make_unique<permissions::WebXrPermissionContext>(
          profile, ContentSettingsType::AR);

  // Depends on Chrome-only DownloadRequestLimiter.
  permission_contexts[ContentSettingsType::BACKGROUND_FETCH] =
      std::make_unique<BackgroundFetchPermissionContext>(profile);

  // TODO(crbug.com/487935): Still in development for Android so we don't
  // support it on WebLayer yet.
  permission_contexts[ContentSettingsType::DISPLAY_CAPTURE] =
      std::make_unique<DisplayCapturePermissionContext>(profile);

  // TODO(crbug.com/1101999): Permission is granted based on browser heuristics
  // (e.g. site engagement) and is not planned for WebLayer until it supports
  // installing PWAs.
  permission_contexts[ContentSettingsType::DURABLE_STORAGE] =
      std::make_unique<DurableStoragePermissionContext>(profile);

#if !defined(OS_ANDROID)
  // TODO(crbug.com/1101999): File Handling is not available on Android and is
  // only relevant for installed PWAs.
  permission_contexts[ContentSettingsType::FILE_HANDLING] =
      std::make_unique<FileHandlingPermissionContext>(profile);
#endif  // !defined(OS_ANDROID)

  // TODO(crbug.com/1043295): Still in development for Android so we don't
  // support it on WebLayer yet.
  permission_contexts[ContentSettingsType::FONT_ACCESS] =
      std::make_unique<FontAccessPermissionContext>(profile);

  // TODO(crbug.com/878979): Still in development so we don't support it on
  // WebLayer yet.
  permission_contexts[ContentSettingsType::IDLE_DETECTION] =
      std::make_unique<IdleDetectionPermissionContext>(profile);

  // Depends on Chrome specific policies not available on WebLayer.
  permission_contexts[ContentSettingsType::MEDIASTREAM_CAMERA] =
      std::make_unique<MediaStreamDevicePermissionContext>(
          profile, ContentSettingsType::MEDIASTREAM_CAMERA);
  permission_contexts[ContentSettingsType::MEDIASTREAM_MIC] =
      std::make_unique<MediaStreamDevicePermissionContext>(
          profile, ContentSettingsType::MEDIASTREAM_MIC);

  // TODO(crbug.com/1025610): Move once Notifications are supported on WebLayer.
  permission_contexts[ContentSettingsType::NOTIFICATIONS] =
      std::make_unique<NotificationPermissionContext>(profile);

  // TODO(crbug.com/1091211): Move once supported on WebLayer.
  permission_contexts[ContentSettingsType::PERIODIC_BACKGROUND_SYNC] =
      std::make_unique<PeriodicBackgroundSyncPermissionContext>(profile);

#if defined(OS_CHROMEOS) || defined(OS_ANDROID) || defined(OS_WIN)
  // We don't support Chrome OS and Windows for WebLayer yet so only the Android
  // specific logic is used on WebLayer.
  permission_contexts[ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER] =
      std::make_unique<ProtectedMediaIdentifierPermissionContext>(profile);
#endif  // defined(OS_CHROMEOS) || defined(OS_ANDROID) || defined(OS_WIN)

  // TODO(crbug.com/989663): Still in development so we don't support it on
  // WebLayer yet.
  permission_contexts[ContentSettingsType::STORAGE_ACCESS] =
      std::make_unique<StorageAccessGrantPermissionContext>(profile);

  // TODO(crbug.com/1057106): Move this to default contexts once WebLayer
  // supports WebXR.
  permission_contexts[ContentSettingsType::VR] =
      std::make_unique<permissions::WebXrPermissionContext>(
          profile, ContentSettingsType::VR);

  // TODO(crbug.com/897300): Still in development for Android so we don't
  // support it on WebLayer yet.
  permission_contexts[ContentSettingsType::WINDOW_PLACEMENT] =
      std::make_unique<WindowPlacementPermissionContext>(profile);

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
