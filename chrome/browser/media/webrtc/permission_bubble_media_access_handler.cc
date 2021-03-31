// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/webrtc/media_stream_devices_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include <vector>

#include "chrome/browser/media/webrtc/screen_capture_infobar_delegate_android.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "content/public/common/content_features.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_MAC)
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"
#endif

using content::BrowserThread;

using RepeatingMediaResponseCallback =
    base::RepeatingCallback<void(const blink::MediaStreamDevices& devices,
                                 blink::mojom::MediaStreamRequestResult result,
                                 std::unique_ptr<content::MediaStreamUI> ui)>;

#if defined(OS_MAC)
using system_media_permissions::SystemPermission;
#endif

namespace {

void UpdatePageSpecificContentSettings(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    ContentSetting audio_setting,
    ContentSetting video_setting) {
  if (!web_contents)
    return;

  // TODO(https://crbug.com/1103176): We should extract the frame from |request|
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents->GetMainFrame());
  if (!content_settings)
    return;

  content_settings::PageSpecificContentSettings::MicrophoneCameraState
      microphone_camera_state = content_settings::PageSpecificContentSettings::
          MICROPHONE_CAMERA_NOT_ACCESSED;
  std::string selected_audio_device;
  std::string selected_video_device;
  std::string requested_audio_device = request.requested_audio_device_id;
  std::string requested_video_device = request.requested_video_device_id;

  // TODO(raymes): Why do we use the defaults here for the selected devices?
  // Shouldn't we just use the devices that were actually selected?
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (audio_setting != CONTENT_SETTING_DEFAULT) {
    selected_audio_device =
        requested_audio_device.empty()
            ? profile->GetPrefs()->GetString(prefs::kDefaultAudioCaptureDevice)
            : requested_audio_device;
    microphone_camera_state |=
        content_settings::PageSpecificContentSettings::MICROPHONE_ACCESSED |
        (audio_setting == CONTENT_SETTING_ALLOW
             ? 0
             : content_settings::PageSpecificContentSettings::
                   MICROPHONE_BLOCKED);
  }

  if (video_setting != CONTENT_SETTING_DEFAULT) {
    selected_video_device =
        requested_video_device.empty()
            ? profile->GetPrefs()->GetString(prefs::kDefaultVideoCaptureDevice)
            : requested_video_device;
    microphone_camera_state |=
        content_settings::PageSpecificContentSettings::CAMERA_ACCESSED |
        (video_setting == CONTENT_SETTING_ALLOW
             ? 0
             : content_settings::PageSpecificContentSettings::CAMERA_BLOCKED);
  }

  content_settings->OnMediaStreamPermissionSet(
      PermissionManagerFactory::GetForProfile(profile)->GetCanonicalOrigin(
          ContentSettingsType::MEDIASTREAM_CAMERA, request.security_origin,
          web_contents->GetLastCommittedURL()),
      microphone_camera_state, selected_audio_device, selected_video_device,
      requested_audio_device, requested_video_device);
}

}  // namespace

struct PermissionBubbleMediaAccessHandler::PendingAccessRequest {
  PendingAccessRequest(const content::MediaStreamRequest& request,
                       RepeatingMediaResponseCallback callback)
      : request(request), callback(callback) {}
  ~PendingAccessRequest() {}

  // TODO(gbillock): make the MediaStreamDevicesController owned by
  // this object when we're using bubbles.
  content::MediaStreamRequest request;
  RepeatingMediaResponseCallback callback;
};

PermissionBubbleMediaAccessHandler::PermissionBubbleMediaAccessHandler()
    : web_contents_collection_(this) {}

PermissionBubbleMediaAccessHandler::~PermissionBubbleMediaAccessHandler() =
    default;

bool PermissionBubbleMediaAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
#if defined(OS_ANDROID)
  return type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         type == blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB;
#else
  return type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
#endif
}

bool PermissionBubbleMediaAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  ContentSettingsType content_settings_type =
      type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE
          ? ContentSettingsType::MEDIASTREAM_MIC
          : ContentSettingsType::MEDIASTREAM_CAMERA;

  DCHECK(!security_origin.is_empty());
  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();
  permissions::PermissionManager* permission_manager =
      PermissionManagerFactory::GetForProfile(profile);
  return permission_manager
             ->GetPermissionStatusForFrame(content_settings_type,
                                           render_frame_host, security_origin)
             .content_setting == CONTENT_SETTING_ALLOW;
}

void PermissionBubbleMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_ANDROID)
  if (blink::IsScreenCaptureMediaType(request.video_type) &&
      !base::FeatureList::IsEnabled(features::kUserMediaScreenCapturing)) {
    // If screen capturing isn't enabled on Android, we'll use "invalid state"
    // as result, same as on desktop.
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, nullptr);
    return;
  }
#endif  // defined(OS_ANDROID)

  // Ensure we are observing the deletion of |web_contents|.
  web_contents_collection_.StartObserving(web_contents);

  RequestsMap& requests_map = pending_requests_[web_contents];
  requests_map.emplace(
      next_request_id_++,
      PendingAccessRequest(
          request, base::AdaptCallbackForRepeating(std::move(callback))));

  // If this is the only request then show the infobar.
  if (requests_map.size() == 1)
    ProcessQueuedAccessRequest(web_contents);
}

void PermissionBubbleMediaAccessHandler::ProcessQueuedAccessRequest(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = pending_requests_.find(web_contents);

  if (it == pending_requests_.end() || it->second.empty()) {
    // Don't do anything if the tab was closed.
    return;
  }

  DCHECK(!it->second.empty());

  const int64_t request_id = it->second.begin()->first;
  const content::MediaStreamRequest& request =
      it->second.begin()->second.request;
#if defined(OS_ANDROID)
  if (blink::IsScreenCaptureMediaType(request.video_type)) {
    ScreenCaptureInfoBarDelegateAndroid::Create(
        web_contents, request,
        base::BindOnce(
            &PermissionBubbleMediaAccessHandler::OnAccessRequestResponse,
            base::Unretained(this), web_contents, request_id));
    return;
  }
#endif

  webrtc::MediaStreamDevicesController::RequestPermissions(
      request, MediaCaptureDevicesDispatcher::GetInstance(),
      base::BindOnce(
          &PermissionBubbleMediaAccessHandler::OnMediaStreamRequestResponse,
          base::Unretained(this), web_contents, request_id, request));
}

void PermissionBubbleMediaAccessHandler::UpdateMediaRequestState(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::mojom::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (state != content::MEDIA_REQUEST_STATE_CLOSING)
    return;

  bool found = false;
  for (auto requests_it = pending_requests_.begin();
       requests_it != pending_requests_.end(); ++requests_it) {
    RequestsMap& requests_map = requests_it->second;
    for (RequestsMap::iterator it = requests_map.begin();
         it != requests_map.end(); ++it) {
      if (it->second.request.render_process_id == render_process_id &&
          it->second.request.render_frame_id == render_frame_id &&
          it->second.request.page_request_id == page_request_id) {
        requests_map.erase(it);
        found = true;
        break;
      }
    }
    if (found)
      break;
  }
}

// static
void PermissionBubbleMediaAccessHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kVideoCaptureAllowed, true);
  prefs->RegisterBooleanPref(prefs::kAudioCaptureAllowed, true);
  prefs->RegisterListPref(prefs::kVideoCaptureAllowedUrls);
  prefs->RegisterListPref(prefs::kAudioCaptureAllowedUrls);
}

void PermissionBubbleMediaAccessHandler::OnMediaStreamRequestResponse(
    content::WebContents* web_contents,
    int64_t request_id,
    content::MediaStreamRequest request,
    const blink::MediaStreamDevices& devices,
    blink::mojom::MediaStreamRequestResult result,
    bool blocked_by_permissions_policy,
    ContentSetting audio_setting,
    ContentSetting video_setting) {
  if (pending_requests_.find(web_contents) == pending_requests_.end()) {
    // WebContents has been destroyed. Don't need to do anything.
    return;
  }

  // If the kill switch is, or the request was blocked because of permissions
  // policy we don't update the tab context.
  if (result != blink::mojom::MediaStreamRequestResult::KILL_SWITCH_ON &&
      !blocked_by_permissions_policy) {
    UpdatePageSpecificContentSettings(web_contents, request, audio_setting,
                                      video_setting);
  }

  std::unique_ptr<content::MediaStreamUI> ui;
  if (!devices.empty()) {
    ui = MediaCaptureDevicesDispatcher::GetInstance()
             ->GetMediaStreamCaptureIndicator()
             ->RegisterMediaStream(web_contents, devices);
  }
  OnAccessRequestResponse(web_contents, request_id, devices, result,
                          std::move(ui));
}

void PermissionBubbleMediaAccessHandler::OnAccessRequestResponse(
    content::WebContents* web_contents,
    int64_t request_id,
    const blink::MediaStreamDevices& devices,
    blink::mojom::MediaStreamRequestResult result,
    std::unique_ptr<content::MediaStreamUI> ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto request_maps_it = pending_requests_.find(web_contents);
  if (request_maps_it == pending_requests_.end()) {
    // WebContents has been destroyed. Don't need to do anything.
    return;
  }

  RequestsMap& requests_map(request_maps_it->second);
  if (requests_map.empty())
    return;

  auto request_it = requests_map.find(request_id);
  DCHECK(request_it != requests_map.end());
  if (request_it == requests_map.end())
    return;

  blink::mojom::MediaStreamRequestResult final_result = result;

#if defined(OS_MAC)
  // If the request was approved, ask for system permissions if needed, and run
  // this function again when done.
  if (result == blink::mojom::MediaStreamRequestResult::OK) {
    const content::MediaStreamRequest& request = request_it->second.request;
    if (request.audio_type ==
        blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
      const SystemPermission system_audio_permission =
          system_media_permissions::CheckSystemAudioCapturePermission();
      UMA_HISTOGRAM_ENUMERATION(
          "Media.Audio.Capture.Mac.MicSystemPermission.UserMedia",
          system_audio_permission);
      if (system_audio_permission == SystemPermission::kNotDetermined) {
        // Using WeakPtr since callback can come at any time and we might be
        // destroyed.
        system_media_permissions::RequestSystemAudioCapturePermisson(
            base::BindOnce(
                &PermissionBubbleMediaAccessHandler::OnAccessRequestResponse,
                weak_factory_.GetWeakPtr(), web_contents, request_id, devices,
                result, std::move(ui)),
            {content::BrowserThread::UI});
        return;
      } else if (system_audio_permission == SystemPermission::kRestricted ||
                 system_audio_permission == SystemPermission::kDenied) {
        content_settings::UpdateLocationBarUiForWebContents(web_contents);
        final_result =
            blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED;
        system_media_permissions::SystemAudioCapturePermissionBlocked();
      } else {
        DCHECK_EQ(system_audio_permission, SystemPermission::kAllowed);
        content_settings::UpdateLocationBarUiForWebContents(web_contents);
      }
    }
    if (request.video_type ==
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
      const SystemPermission system_video_permission =
          system_media_permissions::CheckSystemVideoCapturePermission();
      UMA_HISTOGRAM_ENUMERATION(
          "Media.Video.Capture.Mac.CameraSystemPermission.UserMedia",
          system_video_permission);
      if (system_video_permission == SystemPermission::kNotDetermined) {
        // Using WeakPtr since callback can come at any time and we might be
        // destroyed.
        system_media_permissions::RequestSystemVideoCapturePermisson(
            base::BindOnce(
                &PermissionBubbleMediaAccessHandler::OnAccessRequestResponse,
                weak_factory_.GetWeakPtr(), web_contents, request_id, devices,
                result, std::move(ui)),
            {content::BrowserThread::UI});
        return;
      } else if (system_video_permission == SystemPermission::kRestricted ||
                 system_video_permission == SystemPermission::kDenied) {
        content_settings::UpdateLocationBarUiForWebContents(web_contents);
        final_result =
            blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED;
        system_media_permissions::SystemVideoCapturePermissionBlocked();
      } else {
        DCHECK_EQ(system_video_permission, SystemPermission::kAllowed);
        content_settings::UpdateLocationBarUiForWebContents(web_contents);
      }
    }
  }
#endif  // defined(OS_MAC)

  RepeatingMediaResponseCallback callback =
      std::move(request_it->second.callback);
  requests_map.erase(request_it);

  if (!requests_map.empty()) {
    // Post a task to process next queued request. It has to be done
    // asynchronously to make sure that calling infobar is not destroyed until
    // after this function returns.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PermissionBubbleMediaAccessHandler::ProcessQueuedAccessRequest,
            base::Unretained(this), web_contents));
  }

  std::move(callback).Run(devices, final_result, std::move(ui));
}

void PermissionBubbleMediaAccessHandler::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  pending_requests_.erase(web_contents);
}
