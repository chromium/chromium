// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include <vector>

#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/media/webrtc/screen_capture_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX)
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

#if defined(OS_MACOSX)
using system_media_permissions::SystemPermission;
#endif

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

PermissionBubbleMediaAccessHandler::PermissionBubbleMediaAccessHandler() {
  // PermissionBubbleMediaAccessHandler should be created on UI thread.
  // Otherwise, it will not receive
  // content::NOTIFICATION_WEB_CONTENTS_DESTROYED, and that will result in
  // possible use after free.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notifications_registrar_.Add(this,
                               content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                               content::NotificationService::AllSources());
}

PermissionBubbleMediaAccessHandler::~PermissionBubbleMediaAccessHandler() {}

bool PermissionBubbleMediaAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
#if defined(OS_ANDROID)
  return type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         type == blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;
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
  PermissionManager* permission_manager = PermissionManager::Get(profile);
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
      !base::FeatureList::IsEnabled(
          chrome::android::kUserMediaScreenCapturing)) {
    // If screen capturing isn't enabled on Android, we'll use "invalid state"
    // as result, same as on desktop.
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, nullptr);
    return;
  }
#endif  // defined(OS_ANDROID)

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

  const int request_id = it->second.begin()->first;
  const content::MediaStreamRequest& request =
      it->second.begin()->second.request;
#if defined(OS_ANDROID)
  if (blink::IsScreenCaptureMediaType(request.video_type)) {
    ScreenCaptureInfoBarDelegateAndroid::Create(
        web_contents, request,
        base::Bind(&PermissionBubbleMediaAccessHandler::OnAccessRequestResponse,
                   base::Unretained(this), web_contents, request_id));
    return;
  }
#endif

  MediaStreamDevicesController::RequestPermissions(
      request,
      base::Bind(&PermissionBubbleMediaAccessHandler::OnAccessRequestResponse,
                 base::Unretained(this), web_contents, request_id));
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

void PermissionBubbleMediaAccessHandler::OnAccessRequestResponse(
    content::WebContents* web_contents,
    int request_id,
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

#if defined(OS_MACOSX)
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
#endif  // defined(OS_MACOSX)

  RepeatingMediaResponseCallback callback =
      std::move(request_it->second.callback);
  requests_map.erase(request_it);

  if (!requests_map.empty()) {
    // Post a task to process next queued request. It has to be done
    // asynchronously to make sure that calling infobar is not destroyed until
    // after this function returns.
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &PermissionBubbleMediaAccessHandler::ProcessQueuedAccessRequest,
            base::Unretained(this), web_contents));
  }

  std::move(callback).Run(devices, final_result, std::move(ui));
}

void PermissionBubbleMediaAccessHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_DESTROYED, type);

  pending_requests_.erase(content::Source<content::WebContents>(source).ptr());
}
