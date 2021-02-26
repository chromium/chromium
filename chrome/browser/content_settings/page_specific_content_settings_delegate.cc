// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/browsing_data/access_context_audit_service.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_util.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_uma_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"

namespace {

void RecordOriginStorageAccess(const url::Origin& origin,
                               AccessContextAuditDatabase::StorageAPIType type,
                               content::WebContents* web_contents) {
  auto* access_context_audit_service =
      AccessContextAuditServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (access_context_audit_service)
    access_context_audit_service->RecordStorageAPIAccess(
        origin, type, url::Origin::Create(web_contents->GetLastCommittedURL()));
}

}  // namespace

using content_settings::PageSpecificContentSettings;

namespace chrome {

PageSpecificContentSettingsDelegate::PageSpecificContentSettingsDelegate(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  auto* access_context_audit_service =
      AccessContextAuditServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (access_context_audit_service) {
    cookie_access_helper_ =
        std::make_unique<AccessContextAuditService::CookieAccessHelper>(
            access_context_audit_service);
  }
}

PageSpecificContentSettingsDelegate::~PageSpecificContentSettingsDelegate() =
    default;

// static
PageSpecificContentSettingsDelegate*
PageSpecificContentSettingsDelegate::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<PageSpecificContentSettingsDelegate*>(
      PageSpecificContentSettings::GetDelegateForWebContents(web_contents));
}

void PageSpecificContentSettingsDelegate::UpdateLocationBar() {
  content_settings::UpdateLocationBarUiForWebContents(web_contents());
}

void PageSpecificContentSettingsDelegate::SetContentSettingRules(
    content::RenderProcessHost* process,
    const RendererContentSettingRules& rules) {
  // |channel| may be null in tests.
  IPC::ChannelProxy* channel = process->GetChannel();
  if (!channel)
    return;

  mojo::AssociatedRemote<chrome::mojom::RendererConfiguration> rc_interface;
  channel->GetRemoteAssociatedInterface(&rc_interface);
  rc_interface->SetContentSettingRules(rules);
}

PrefService* PageSpecificContentSettingsDelegate::GetPrefs() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!profile)
    return nullptr;

  return profile->GetPrefs();
}

HostContentSettingsMap* PageSpecificContentSettingsDelegate::GetSettingsMap() {
  return HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

ContentSetting PageSpecificContentSettingsDelegate::GetEmbargoSetting(
    const GURL& request_origin,
    ContentSettingsType permission) {
  return PermissionDecisionAutoBlockerFactory::GetForProfile(
             Profile::FromBrowserContext(web_contents()->GetBrowserContext()))
      ->GetEmbargoResult(request_origin, permission)
      .content_setting;
}

std::vector<storage::FileSystemType>
PageSpecificContentSettingsDelegate::GetAdditionalFileSystemTypes() {
  return browsing_data_file_system_util::GetAdditionalFileSystemTypes();
}

browsing_data::CookieHelper::IsDeletionDisabledCallback
PageSpecificContentSettingsDelegate::GetIsDeletionDisabledCallback() {
  return CookiesTreeModel::GetCookieDeletionDisabledCallback(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

bool PageSpecificContentSettingsDelegate::IsMicrophoneCameraStateChanged(
    PageSpecificContentSettings::MicrophoneCameraState microphone_camera_state,
    const std::string& media_stream_selected_audio_device,
    const std::string& media_stream_selected_video_device) {
  PrefService* prefs = GetPrefs();
  scoped_refptr<MediaStreamCaptureIndicator> media_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();

  if ((microphone_camera_state &
       PageSpecificContentSettings::MICROPHONE_ACCESSED) &&
      prefs->GetString(prefs::kDefaultAudioCaptureDevice) !=
          media_stream_selected_audio_device &&
      media_indicator->IsCapturingAudio(web_contents()))
    return true;

  if ((microphone_camera_state &
       PageSpecificContentSettings::CAMERA_ACCESSED) &&
      prefs->GetString(prefs::kDefaultVideoCaptureDevice) !=
          media_stream_selected_video_device &&
      media_indicator->IsCapturingVideo(web_contents()))
    return true;

  return false;
}

PageSpecificContentSettings::MicrophoneCameraState
PageSpecificContentSettingsDelegate::GetMicrophoneCameraState() {
  PageSpecificContentSettings::MicrophoneCameraState state =
      PageSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED;

  // Include capture devices in the state if there are still consumers of the
  // approved media stream.
  scoped_refptr<MediaStreamCaptureIndicator> media_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  if (media_indicator->IsCapturingAudio(web_contents()))
    state |= PageSpecificContentSettings::MICROPHONE_ACCESSED;
  if (media_indicator->IsCapturingVideo(web_contents()))
    state |= PageSpecificContentSettings::CAMERA_ACCESSED;

  return state;
}

void PageSpecificContentSettingsDelegate::OnContentAllowed(
    ContentSettingsType type) {
  if (!(type == ContentSettingsType::GEOLOCATION ||
        type == ContentSettingsType::MEDIASTREAM_CAMERA ||
        type == ContentSettingsType::MEDIASTREAM_MIC)) {
    return;
  }
  content_settings::SettingInfo setting_info;
  GetSettingsMap()->GetWebsiteSetting(web_contents()->GetLastCommittedURL(),
                                      web_contents()->GetLastCommittedURL(),
                                      type, &setting_info);
  const base::Time grant_time = GetSettingsMap()->GetSettingLastModifiedDate(
      setting_info.primary_pattern, setting_info.secondary_pattern, type);
  if (grant_time.is_null())
    return;
  permissions::PermissionUmaUtil::RecordTimeElapsedBetweenGrantAndUse(
      type, base::Time::Now() - grant_time);
}

void PageSpecificContentSettingsDelegate::OnContentBlocked(
    ContentSettingsType type) {
  if (type == ContentSettingsType::POPUPS) {
    content_settings::RecordPopupsAction(
        content_settings::POPUPS_ACTION_DISPLAYED_BLOCKED_ICON_IN_OMNIBOX);
  }
}

void PageSpecificContentSettingsDelegate::OnCacheStorageAccessAllowed(
    const url::Origin& origin) {
  RecordOriginStorageAccess(
      origin, AccessContextAuditDatabase::StorageAPIType::kCacheStorage,
      web_contents());
}

void PageSpecificContentSettingsDelegate::OnCookieAccessAllowed(
    const net::CookieList& accessed_cookies) {
  if (cookie_access_helper_) {
    cookie_access_helper_->RecordCookieAccess(
        accessed_cookies,
        url::Origin::Create(web_contents()->GetLastCommittedURL()));
  }
}

void PageSpecificContentSettingsDelegate::OnDomStorageAccessAllowed(
    const url::Origin& origin) {
  RecordOriginStorageAccess(
      origin, AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
      web_contents());
}

void PageSpecificContentSettingsDelegate::OnFileSystemAccessAllowed(
    const url::Origin& origin) {
  RecordOriginStorageAccess(
      origin, AccessContextAuditDatabase::StorageAPIType::kFileSystem,
      web_contents());
}

void PageSpecificContentSettingsDelegate::OnIndexedDBAccessAllowed(
    const url::Origin& origin) {
  RecordOriginStorageAccess(
      origin, AccessContextAuditDatabase::StorageAPIType::kIndexedDB,
      web_contents());
}

void PageSpecificContentSettingsDelegate::OnServiceWorkerAccessAllowed(
    const url::Origin& origin) {
  RecordOriginStorageAccess(
      origin, AccessContextAuditDatabase::StorageAPIType::kServiceWorker,
      web_contents());
}

void PageSpecificContentSettingsDelegate::OnWebDatabaseAccessAllowed(
    const url::Origin& origin) {
  RecordOriginStorageAccess(
      origin, AccessContextAuditDatabase::StorageAPIType::kWebDatabase,
      web_contents());
}

void PageSpecificContentSettingsDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  ClearPendingProtocolHandler();

  if (web_contents()->GetVisibleURL().SchemeIsHTTPOrHTTPS()) {
    content_settings::RecordPluginsAction(
        content_settings::PLUGINS_ACTION_TOTAL_NAVIGATIONS);
  }
}

}  // namespace chrome
