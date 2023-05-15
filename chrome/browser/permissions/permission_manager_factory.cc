// Copyright 2015 The Chromium Authors
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
#include "components/permissions/contexts/local_fonts_permission_context.h"
#include "components/permissions/contexts/window_management_permission_context.h"
#include "components/permissions/permission_manager.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/geolocation/geolocation_permission_context_delegate_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#endif

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
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  delegates.geolocation_manager = g_browser_process->geolocation_manager();
  DCHECK(delegates.geolocation_manager);
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
      embedder_support::CreateDefaultPermissionContexts(profile,
                                                        std::move(delegates));

  // Add additional Chrome specific permission contexts. Please add a comment
  // when adding new contexts here explaining why it can't be shared with other
  // Content embedders by adding it to CreateDefaultPermissionContexts().

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

  // TODO(crbug.com/878979): Still in development so we don't support it on
  // WebLayer yet.
  permission_contexts[ContentSettingsType::IDLE_DETECTION] =
      std::make_unique<IdleDetectionPermissionContext>(profile);

  // TODO(crbug.com/1043295): Still in development for Android so we don't
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

  // TODO(crbug.com/1025610): Move once Notifications are supported on WebLayer.
  permission_contexts[ContentSettingsType::NOTIFICATIONS] =
      std::make_unique<NotificationPermissionContext>(profile);

  // TODO(crbug.com/1091211): Move once supported on WebLayer.
  permission_contexts[ContentSettingsType::PERIODIC_BACKGROUND_SYNC] =
      std::make_unique<PeriodicBackgroundSyncPermissionContext>(profile);

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
  // We don't support Chrome OS and Windows for WebLayer yet so only the Android
  // specific logic is used on WebLayer.
  permission_contexts[ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER] =
      std::make_unique<ProtectedMediaIdentifierPermissionContext>(profile);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)

  // TODO(crbug.com/989663): Still in development so we don't support it on
  // WebLayer yet.
  permission_contexts[ContentSettingsType::STORAGE_ACCESS] =
      std::make_unique<StorageAccessGrantPermissionContext>(profile);

  permission_contexts[ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS] =
      std::make_unique<TopLevelStorageAccessPermissionContext>(profile);

  // TODO(crbug.com/897300): Still in development for Android so we don't
  // support it on WebLayer yet.
  permission_contexts[ContentSettingsType::WINDOW_MANAGEMENT] =
      std::make_unique<permissions::WindowManagementPermissionContext>(profile);

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
    : ProfileKeyedServiceFactory(
          "PermissionManagerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PermissionManagerFactory::~PermissionManagerFactory() {}

KeyedService* PermissionManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new permissions::PermissionManager(profile,
                                            CreatePermissionContexts(profile));
}
