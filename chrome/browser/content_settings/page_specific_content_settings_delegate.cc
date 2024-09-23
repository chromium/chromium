// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"

#include "base/feature_list.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_util.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/permissions/permission_uma_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "ipc/ipc_channel_proxy.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

using content_settings::PageSpecificContentSettings;

PageSpecificContentSettingsDelegate::PageSpecificContentSettingsDelegate(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  if (base::FeatureList::IsEnabled(
          content_settings::features::kImprovedSemanticsActivityIndicators)) {
    media_observation_.Observe(MediaCaptureDevicesDispatcher::GetInstance()
                                   ->GetMediaStreamCaptureIndicator()
                                   .get());
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

void PageSpecificContentSettingsDelegate::OnIsCapturingVideoChanged(
    content::WebContents* web_contents,
    bool is_capturing_video) {
  OnCapturingStateChanged(web_contents, ContentSettingsType::MEDIASTREAM_CAMERA,
                          is_capturing_video);
}

void PageSpecificContentSettingsDelegate::OnIsCapturingAudioChanged(
    content::WebContents* web_contents,
    bool is_capturing_audio) {
  OnCapturingStateChanged(web_contents, ContentSettingsType::MEDIASTREAM_MIC,
                          is_capturing_audio);
}

void PageSpecificContentSettingsDelegate::OnCapturingStateChanged(
    content::WebContents* web_contents,
    ContentSettingsType type,
    bool is_capturing) {
  DCHECK(web_contents);

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents->GetPrimaryMainFrame());

  if (pscs == nullptr) {
    // There are cases, e.g. MPArch, where there is no active instance of
    // PageSpecificContentSettings for a frame.
    return;
  }

  pscs->OnCapturingStateChanged(type, is_capturing);

  content::WebContents* pip_web_contents =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  if (pip_web_contents && pip_web_contents != web_contents) {
    OnCapturingStateChanged(pip_web_contents, type, is_capturing);
  }
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

  if (state.Has(PageSpecificContentSettings::kCameraAccessed) ||
      state.Has(PageSpecificContentSettings::kMicrophoneAccessed)) {
    auto* permission_tracker =
        permissions::PermissionRecoverySuccessRateTracker::FromWebContents(
            web_contents());

    if (state.Has(PageSpecificContentSettings::kMicrophoneAccessed)) {
      permission_tracker->TrackUsage(ContentSettingsType::MEDIASTREAM_MIC);
    }

    if (state.Has(PageSpecificContentSettings::kCameraAccessed)) {
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
  rules->mixed_content_rules.clear();
  rules->mixed_content_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK),
      content_settings::ProviderType::kNone, incognito));
}
#endif
}  // namespace

void PageSpecificContentSettingsDelegate::SetDefaultRendererContentSettingRules(
    content::RenderFrameHost* rfh,
    RendererContentSettingRules* rules) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool is_off_the_record =
      web_contents()->GetBrowserContext()->IsOffTheRecord();

  if (guest_view::GuestViewBase::IsGuest(rfh)) {
    GetGuestViewDefaultContentSettingRules(is_off_the_record, rules);
    return;
  }
#endif
}

PageSpecificContentSettings::MicrophoneCameraState
PageSpecificContentSettingsDelegate::GetMicrophoneCameraState() {
  PageSpecificContentSettings::MicrophoneCameraState state;

  // Include capture devices in the state if there are still consumers of the
  // approved media stream.
  scoped_refptr<MediaStreamCaptureIndicator> media_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  if (media_indicator->IsCapturingAudio(web_contents()))
    state.Put(PageSpecificContentSettings::kMicrophoneAccessed);
  if (media_indicator->IsCapturingVideo(web_contents()))
    state.Put(PageSpecificContentSettings::kCameraAccessed);

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
      type, base::Time::Now() - grant_time, setting_info.source);
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

bool PageSpecificContentSettingsDelegate::IsBlockedOnSystemLevel(
    ContentSettingsType type) {
  DCHECK(type == ContentSettingsType::MEDIASTREAM_MIC ||
         type == ContentSettingsType::MEDIASTREAM_CAMERA);

  return system_permission_settings::IsDenied(type);
}

bool PageSpecificContentSettingsDelegate::IsFrameAllowlistedForJavaScript(
    content::RenderFrameHost* render_frame_host) {
#if BUILDFLAG(ENABLE_PDF)
  // OOPIF PDF viewer only.
  if (!chrome_pdf::features::IsOopifPdfEnabled()) {
    return false;
  }

  // There should be a `pdf::PdfViewerStreamManager` if `render_frame_host`'s
  // `content::WebContents` has a PDF.
  auto* pdf_viewer_stream_manager =
      pdf::PdfViewerStreamManager::FromRenderFrameHost(render_frame_host);
  if (!pdf_viewer_stream_manager) {
    return false;
  }

  // Allow the PDF extension frame and PDF content frame to use JavaScript.
  if (pdf_viewer_stream_manager->IsPdfExtensionHost(render_frame_host) ||
      pdf_viewer_stream_manager->IsPdfContentHost(render_frame_host)) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  return false;
}

void PageSpecificContentSettingsDelegate::PrimaryPageChanged(
    content::Page& page) {
  ClearPendingProtocolHandler();
}
