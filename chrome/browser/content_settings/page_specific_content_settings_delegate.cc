// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_util.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#endif
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_uma_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_proxy.h"

using content_settings::PageSpecificContentSettings;

namespace chrome {

PageSpecificContentSettingsDelegate::PageSpecificContentSettingsDelegate(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

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

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  if (pscs == nullptr) {
    // There are cases, e.g. MPArch, where there is no active instance of
    // PageSpecificContentSettings for a frame.
    return;
  }

  PageSpecificContentSettings::MicrophoneCameraState state =
      pscs->GetMicrophoneCameraState();

  if ((state & PageSpecificContentSettings::CAMERA_ACCESSED) ||
      (state & PageSpecificContentSettings::MICROPHONE_ACCESSED)) {
    auto* permission_tracker =
        permissions::PermissionRecoverySuccessRateTracker::FromWebContents(
            web_contents());

    if (state & PageSpecificContentSettings::MICROPHONE_ACCESSED) {
      permission_tracker->TrackUsage(ContentSettingsType::MEDIASTREAM_MIC);
    }

    if (state & PageSpecificContentSettings::CAMERA_ACCESSED) {
      permission_tracker->TrackUsage(ContentSettingsType::MEDIASTREAM_CAMERA);
    }
  }
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

std::unique_ptr<BrowsingDataModel::Delegate>
PageSpecificContentSettingsDelegate::CreateBrowsingDataModelDelegate() {
  return ChromeBrowsingDataModelDelegate::CreateForStoragePartition(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
      web_contents()->GetPrimaryMainFrame()->GetStoragePartition());
}

namespace {
// By default, JavaScript, images and auto dark are allowed, and blockable mixed
// content is blocked in guest content
#if BUILDFLAG(ENABLE_EXTENSIONS)
void GetGuestViewDefaultContentSettingRules(
    bool incognito,
    RendererContentSettingRules* rules) {
  rules->image_rules.clear();
  rules->image_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
      std::string(), incognito));
  rules->auto_dark_content_rules.clear();
  rules->auto_dark_content_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
      std::string(), incognito));
  rules->script_rules.clear();
  rules->script_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
      std::string(), incognito));
  rules->mixed_content_rules.clear();
  rules->mixed_content_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK),
      std::string(), incognito));
}
#endif
}  // namespace

void PageSpecificContentSettingsDelegate::SetDefaultRendererContentSettingRules(
    content::RenderFrameHost* rfh,
    RendererContentSettingRules* rules) {
  bool is_off_the_record =
      web_contents()->GetBrowserContext()->IsOffTheRecord();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (guest_view::GuestViewBase::IsGuest(rfh)) {
    GetGuestViewDefaultContentSettingRules(is_off_the_record, rules);
    return;
  }
#endif
  // Always allow scripting in PDF renderers to retain the functionality of
  // the scripted messaging proxy in between the plugins in the PDF renderers
  // and the PDF extension UI. Content settings for JavaScript embedded in
  // PDFs are enforced by the PDF plugin.
  if (rfh->GetProcess()->IsPdf()) {
    rules->script_rules.clear();
    rules->script_rules.emplace_back(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
        std::string(), is_off_the_record);
  }
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

content::WebContents* PageSpecificContentSettingsDelegate::
    MaybeGetSyncedWebContentsForPictureInPicture(
        content::WebContents* web_contents) {
  DCHECK(web_contents);
  content::WebContents* parent_web_contents =
      PictureInPictureWindowManager::GetInstance()->GetWebContents();
  content::WebContents* child_web_contents =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();

  // For document picture-in-picture window, return the opener web contents.
  if (web_contents == child_web_contents) {
    DCHECK(parent_web_contents);
    return parent_web_contents;
  }

  // For browser window that has opened a document picture-in-picture window,
  // return the PiP window web contents.
  if ((web_contents == parent_web_contents) && child_web_contents) {
    return child_web_contents;
  }
  return nullptr;
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
  const base::Time grant_time = setting_info.metadata.last_modified();
  if (grant_time.is_null())
    return;
  permissions::PermissionUmaUtil::RecordTimeElapsedBetweenGrantAndUse(
      type, base::Time::Now() - grant_time);
  permissions::PermissionUmaUtil::RecordPermissionUsage(
      type, web_contents()->GetBrowserContext(), web_contents(),
      web_contents()->GetLastCommittedURL());
}

void PageSpecificContentSettingsDelegate::OnContentBlocked(
    ContentSettingsType type) {
  if (type == ContentSettingsType::POPUPS) {
    content_settings::RecordPopupsAction(
        content_settings::POPUPS_ACTION_DISPLAYED_BLOCKED_ICON_IN_OMNIBOX);
  }
}

void PageSpecificContentSettingsDelegate::PrimaryPageChanged(
    content::Page& page) {
  ClearPendingProtocolHandler();
}

}  // namespace chrome
