// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager_factory.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/background_fetch/background_fetch_permission_context.h"
#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/display_capture/captured_surface_control_permission_context.h"
#include "chrome/browser/display_capture/display_capture_permission_context.h"
#include "chrome/browser/geolocation/geolocation_permission_context_delegate.h"
#include "chrome/browser/idle/idle_detection_permission_context.h"
#include "chrome/browser/media/webrtc/chrome_camera_pan_tilt_zoom_permission_context_delegate.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_device_permission_context.h"
#include "chrome/browser/nfc/chrome_nfc_permission_context_delegate.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/storage/durable_storage_permission_context.h"
#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/top_level_storage_access_api/top_level_storage_access_permission_context.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/background_sync/background_sync_permission_context.h"
#include "components/embedder_support/permission_context_utils.h"
#include "components/permissions/contexts/automatic_fullscreen_permission_context.h"
#include "components/permissions/contexts/keyboard_lock_permission_context.h"
#include "components/permissions/contexts/local_fonts_permission_context.h"
#include "components/permissions/contexts/pointer_lock_permission_context.h"
#include "components/permissions/contexts/speaker_selection_permission_context.h"
#include "components/permissions/contexts/web_app_installation_permission_context.h"
#include "components/permissions/contexts/window_management_permission_context.h"
#include "components/permissions/permission_manager.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/buildflags.h"

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/geolocation/geolocation_permission_context_delegate_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
#include "chrome/browser/printing/web_api/web_printing_permission_context.h"
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

namespace {

permissions::PermissionManager::PermissionContextMap CreatePermissionContexts(
    Profile* profile) {
  embedder_support::PermissionContextDelegates delegates;

#if BUILDFLAG(IS_ANDROID)
  delegates.geolocation_permission_context_delegate =
      std::make_unique<GeolocationPermissionContextDelegateAndroid>(profile);
#else
  delegates.geolocation_permission_context_delegate =
      std::make_unique<GeolocationPermissionContextDelegate>(profile);
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  if (features::IsOsLevelGeolocationPermissionSupportEnabled()) {
    delegates.geolocation_system_permission_manager =
        device::GeolocationSystemPermissionManager::GetInstance();
    DCHECK(delegates.geolocation_system_permission_manager);
  }
#endif
  delegates.media_stream_device_enumerator =
      MediaCaptureDevicesDispatcher::GetInstance();
  delegates.camera_pan_tilt_zoom_permission_context_delegate =
      std::make_unique<ChromeCameraPanTiltZoomPermissionContextDelegate>(
          profile);
  delegates.nfc_permission_context_delegate =
      std::make_unique<ChromeNfcPermissionContextDelegate>();

  // Create default permission contexts initially.
  permissions::PermissionManager::PermissionContextMap permission_contexts =
      embedder_support::CreateDefaultPermissionContexts(
          profile, profile->IsRegularProfile(), std::move(delegates));

  // Add additional Chrome specific permission contexts. Please add a comment
  // when adding new contexts here explaining why it can't be shared with other
  // Content embedders by adding it to CreateDefaultPermissionContexts().

  // TODO(crbug.com/40941384): Still in development for Android so we don't
  // support it on WebLayer yet.
  permission_contexts[ContentSettingsType::AUTOMATIC_FULLSCREEN] =
      std::make_unique<permissions::AutomaticFullscreenPermissionContext>(
          profile);

  // Depends on Chrome-only DownloadRequestLimiter.
  permission_contexts[ContentSettingsType::BACKGROUND_FETCH] =
      std::make_unique<BackgroundFetchPermissionContext>(profile);

  // TODO(crbug.com/40418135): Still in development for Android so we don't
  // support it on WebLayer yet.
  permission_contexts[ContentSettingsType::DISPLAY_CAPTURE] =
      std::make_unique<DisplayCapturePermissionContext>(profile);

  // TODO(crbug.com/40703864): Permission is granted based on browser heuristics
  // (e.g. site engagement) and is not planned for WebLayer until it supports
  // installing PWAs.
  permission_contexts[ContentSettingsType::DURABLE_STORAGE] =
      std::make_unique<DurableStoragePermissionContext>(profile);

  // TODO(crbug.com/40591477): Still in development so we don't support it on
  // WebLayer yet.
  permission_contexts[ContentSettingsType::IDLE_DETECTION] =
      std::make_unique<IdleDetectionPermissionContext>(profile);

  permission_contexts[ContentSettingsType::KEYBOARD_LOCK] =
      std::make_unique<permissions::KeyboardLockPermissionContext>(profile);

  // TODO(crbug.com/40115199): Still in development for Android so we don't
  // support it on WebLayer yet.
  permission_contexts[ContentSettingsType::LOCAL_FONTS] =
      std::make_unique<LocalFontsPermissionContext>(profile);

  // Depends on Chrome specific policies not available on WebLayer.
  permission_contexts[ContentSettingsType::MEDIASTREAM_CAMERA] =
      std::make_unique<MediaStreamDevicePermissionContext>(
          profile, ContentSettingsType::MEDIASTREAM_CAMERA);
  permission_contexts[ContentSettingsType::MEDIASTREAM_MIC] =
      std::make_unique<MediaStreamDevicePermissionContext>(
          profile, ContentSettingsType::MEDIASTREAM_MIC);

  permission_contexts[ContentSettingsType::SPEAKER_SELECTION] =
      std::make_unique<SpeakerSelectionPermissionContext>(profile);

  // TODO(crbug.com/40659287): Move once Notifications are supported on
  // WebLayer.
  permission_contexts[ContentSettingsType::NOTIFICATIONS] =
      std::make_unique<NotificationPermissionContext>(profile);

  // TODO(crbug.com/40697624): Move once supported on WebLayer.
  permission_contexts[ContentSettingsType::PERIODIC_BACKGROUND_SYNC] =
      std::make_unique<PeriodicBackgroundSyncPermissionContext>(profile);

  permission_contexts[ContentSettingsType::POINTER_LOCK] =
      std::make_unique<permissions::PointerLockPermissionContext>(profile);

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
  // We don't support Chrome OS and Windows for WebLayer yet so only the Android
  // specific logic is used on WebLayer.
  permission_contexts[ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER] =
      std::make_unique<ProtectedMediaIdentifierPermissionContext>(profile);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)

  // TODO(crbug.com/40638427): Still in development so we don't support it on
  // WebLayer yet.
  permission_contexts[ContentSettingsType::STORAGE_ACCESS] =
      std::make_unique<StorageAccessGrantPermissionContext>(profile);

  permission_contexts[ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS] =
      std::make_unique<TopLevelStorageAccessPermissionContext>(profile);

  // TODO(crbug.com/40092782): Still in development for Android so we don't
  // support it on WebLayer yet.
  permission_contexts[ContentSettingsType::WINDOW_MANAGEMENT] =
      std::make_unique<permissions::WindowManagementPermissionContext>(profile);

  permission_contexts[ContentSettingsType::CAPTURED_SURFACE_CONTROL] =
      std::make_unique<permissions::CapturedSurfaceControlPermissionContext>(
          profile);

  permission_contexts[ContentSettingsType::WEB_APP_INSTALLATION] =
      std::make_unique<permissions::WebAppInstallationPermissionContext>(
          profile);

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
  permission_contexts[ContentSettingsType::WEB_PRINTING] =
      std::make_unique<WebPrintingPermissionContext>(profile);
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)

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
  static base::NoDestructor<PermissionManagerFactory> instance;
  return instance.get();
}

PermissionManagerFactory::PermissionManagerFactory()
    : ProfileKeyedServiceFactory(
          "PermissionManagerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PermissionManagerFactory::~PermissionManagerFactory() = default;

std::unique_ptr<KeyedService>
PermissionManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<permissions::PermissionManager>(
      profile, CreatePermissionContexts(profile));
}
