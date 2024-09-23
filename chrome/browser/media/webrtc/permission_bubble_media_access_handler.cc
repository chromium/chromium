// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/webrtc/media_stream_devices_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include <vector>

#include "chrome/browser/media/webrtc/screen_capture_permission_handler_android.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/common/content_features.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"
#include "chrome/browser/permissions/system/system_media_capture_permissions_mac.h"
#endif

using content::BrowserThread;

using MediaResponseCallback =
    base::OnceCallback<void(const blink::mojom::StreamDevicesSet& devices,
                            blink::mojom::MediaStreamRequestResult result,
                            std::unique_ptr<content::MediaStreamUI> ui)>;

#if BUILDFLAG(IS_MAC)
using system_permission_settings::SystemPermission;
#endif

namespace {

void UpdatePageSpecificContentSettings(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    ContentSetting audio_setting,
    ContentSetting video_setting) {
  if (!web_contents)
    return;

  content::RenderFrameHost* const render_frame_host =
      content::RenderFrameHost::FromID(request.render_process_id,
                                       request.render_frame_id);
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          render_frame_host);
  if (!content_settings)
    return;

  content_settings::PageSpecificContentSettings::MicrophoneCameraState
      microphone_camera_state;

  if (audio_setting != CONTENT_SETTING_DEFAULT) {
    microphone_camera_state.Put(
        content_settings::PageSpecificContentSettings::kMicrophoneAccessed);
    if (audio_setting != CONTENT_SETTING_ALLOW) {
      microphone_camera_state.Put(
          content_settings::PageSpecificContentSettings::kMicrophoneBlocked);
    }
  }

  if (video_setting != CONTENT_SETTING_DEFAULT) {
    microphone_camera_state.Put(
        content_settings::PageSpecificContentSettings::kCameraAccessed);
    if (video_setting != CONTENT_SETTING_ALLOW) {
      microphone_camera_state.Put(
          content_settings::PageSpecificContentSettings::kCameraBlocked);
    }
  }

  // We should always use `GetLastCommittedURL` if web_contents represent NTP.
  // Otherwise, the Microphone permission request on NTP will be gated for
  // incorrect origin.
  GURL embedding_origin;
  if (permissions::PermissionsClient::Get()->DoURLsMatchNewTabPage(
          request.security_origin,
          web_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL())) {
    embedding_origin =
        web_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  } else {
    embedding_origin = permissions::PermissionUtil::GetLastCommittedOriginAsURL(
        render_frame_host->GetMainFrame());
  }

  content_settings->OnMediaStreamPermissionSet(
      permissions::PermissionUtil::GetCanonicalOrigin(
          ContentSettingsType::MEDIASTREAM_CAMERA, request.security_origin,
          embedding_origin),
      microphone_camera_state);
}

}  // namespace

struct PermissionBubbleMediaAccessHandler::PendingAccessRequest {
  PendingAccessRequest(const content::MediaStreamRequest& request,
                       MediaResponseCallback callback)
      : request(request), callback(std::move(callback)) {}

  // TODO(gbillock): make the MediaStreamDevicesController owned by
  // this object when we're using bubbles.
  content::MediaStreamRequest request;
  MediaResponseCallback callback;
};

PermissionBubbleMediaAccessHandler::PermissionBubbleMediaAccessHandler()
    : web_contents_collection_(this) {}

PermissionBubbleMediaAccessHandler::~PermissionBubbleMediaAccessHandler() =
    default;

bool PermissionBubbleMediaAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
#if BUILDFLAG(IS_ANDROID)
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
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  blink::PermissionType permission_type =
      type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE
          ? blink::PermissionType::AUDIO_CAPTURE
          : blink::PermissionType::VIDEO_CAPTURE;

  // TODO(crbug.com/40223767): Remove `security_origin`.
  if (render_frame_host->GetLastCommittedOrigin() != security_origin) {
    return false;
  }
  // It is OK to ignore `security_origin` because it will be calculated from
  // `render_frame_host` and we always ignore `requesting_origin` for
  // `AUDIO_CAPTURE` and `VIDEO_CAPTURE`.
  // `render_frame_host->GetMainFrame()->GetLastCommittedOrigin()` will be used
  // instead.
  return render_frame_host->GetBrowserContext()
             ->GetPermissionController()
             ->GetPermissionStatusForCurrentDocument(permission_type,
                                                     render_frame_host) ==
         blink::mojom::PermissionStatus::GRANTED;
}

void PermissionBubbleMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(IS_ANDROID)
  if (blink::IsScreenCaptureMediaType(request.video_type) &&
      !base::FeatureList::IsEnabled(features::kUserMediaScreenCapturing)) {
    // If screen capturing isn't enabled on Android, we'll use "invalid state"
    // as result, same as on desktop.
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, nullptr);
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Ensure we are observing the deletion of |web_contents|.
  web_contents_collection_.StartObserving(web_contents);

  RequestsMap& requests_map = pending_requests_[web_contents];
  requests_map.emplace(next_request_id_++,
                       PendingAccessRequest(request, std::move(callback)));

  // If this is the only request then show the infobar.
  if (requests_map.size() == 1)
    ProcessQueuedAccessRequest(web_contents);
}

void PermissionBubbleMediaAccessHandler::ProcessQueuedAccessRequest(
    MayBeDangling<content::WebContents> web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The queue is iterated through using a chain of PostTasks, and between these
  // executions, `web_contents` might have been destroyed. In that case, the
  // observer will have removed it from the map. Because all the accesses to the
  // pending_requests_ map are made from the UI thread, we do not need to use a
  // lock and simply verify that the WebContents is still there.
  auto it = pending_requests_.find(web_contents);
  if (it == pending_requests_.end() || it->second.empty()) {
    // Don't do anything if the tab was closed.
    return;
  }

  DCHECK(!it->second.empty());

  const int64_t request_id = it->second.begin()->first;
  const content::MediaStreamRequest& request =
      it->second.begin()->second.request;
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40160723): This should be split into
  // DisplayMediaAccessHandler and DesktopCaptureAccessHandler.
  if (blink::IsScreenCaptureMediaType(request.video_type)) {
    screen_capture::GetScreenCapturePermissionAndroid(
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
    const blink::mojom::StreamDevicesSet& stream_devices_set,
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

  // At most one stream is expected as this function is not used with the
  // getAllScreensMedia API (only used with getUserMedia).
  DCHECK_LE(stream_devices_set.stream_devices.size(), 1u);
  blink::mojom::StreamDevices devices;
  if (!stream_devices_set.stream_devices.empty()) {
    devices = *stream_devices_set.stream_devices[0];
  }

  std::unique_ptr<content::MediaStreamUI> ui;
  if (!stream_devices_set.stream_devices.empty() &&
      (stream_devices_set.stream_devices[0]->audio_device.has_value() ||
       stream_devices_set.stream_devices[0]->video_device.has_value())) {
    ui = MediaCaptureDevicesDispatcher::GetInstance()
             ->GetMediaStreamCaptureIndicator()
             ->RegisterMediaStream(web_contents, devices);
  }
  OnAccessRequestResponse(web_contents, request_id, stream_devices_set, result,
                          std::move(ui));
}

void PermissionBubbleMediaAccessHandler::OnAccessRequestResponseForBinding(
    MayBeDangling<content::WebContents> web_contents,
    int64_t request_id,
    blink::mojom::StreamDevicesSetPtr stream_devices_set,
    blink::mojom::MediaStreamRequestResult result,
    std::unique_ptr<content::MediaStreamUI> ui) {
  DCHECK(stream_devices_set);
  DCHECK(ui);
  OnAccessRequestResponse(web_contents, request_id, *stream_devices_set, result,
                          std::move(ui));
}

void PermissionBubbleMediaAccessHandler::OnAccessRequestResponse(
    content::WebContents* web_contents,
    int64_t request_id,
    const blink::mojom::StreamDevicesSet& stream_devices_set,
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

#if BUILDFLAG(IS_MAC)
  // If the request was approved, ask for system permissions if needed, and run
  // this function again when done.
  if (result == blink::mojom::MediaStreamRequestResult::OK) {
    const content::MediaStreamRequest& request = request_it->second.request;
    if (request.audio_type ==
        blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
      const SystemPermission system_audio_permission =
          system_permission_settings::CheckSystemAudioCapturePermission();
      UMA_HISTOGRAM_ENUMERATION(
          "Media.Audio.Capture.Mac.MicSystemPermission.UserMedia",
          system_audio_permission);
      if (system_audio_permission == SystemPermission::kNotDetermined) {
        // Using WeakPtr since callback can come at any time and we might be
        // destroyed.
        system_permission_settings::RequestSystemAudioCapturePermission(
            base::BindOnce(&PermissionBubbleMediaAccessHandler::
                               OnAccessRequestResponseForBinding,
                           weak_factory_.GetWeakPtr(),
                           base::UnsafeDangling(web_contents), request_id,
                           stream_devices_set.Clone(), result, std::move(ui)));
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
          system_permission_settings::CheckSystemVideoCapturePermission();
      UMA_HISTOGRAM_ENUMERATION(
          "Media.Video.Capture.Mac.CameraSystemPermission.UserMedia",
          system_video_permission);
      if (system_video_permission == SystemPermission::kNotDetermined) {
        // Using WeakPtr since callback can come at any time and we might be
        // destroyed.
        system_permission_settings::RequestSystemVideoCapturePermission(
            base::BindOnce(&PermissionBubbleMediaAccessHandler::
                               OnAccessRequestResponseForBinding,
                           weak_factory_.GetWeakPtr(),
                           base::UnsafeDangling(web_contents), request_id,
                           stream_devices_set.Clone(), result, std::move(ui)));
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
#endif  // BUILDFLAG(IS_MAC)

  MediaResponseCallback callback = std::move(request_it->second.callback);
  requests_map.erase(request_it);

  if (!requests_map.empty()) {
    // Post a task to process next queued request. It has to be done
    // asynchronously to make sure that calling infobar is not destroyed until
    // after this function returns.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PermissionBubbleMediaAccessHandler::ProcessQueuedAccessRequest,
            base::Unretained(this), base::UnsafeDangling(web_contents)));
  }

  std::move(callback).Run(stream_devices_set, final_result, std::move(ui));
}

void PermissionBubbleMediaAccessHandler::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  pending_requests_.erase(web_contents);
}
