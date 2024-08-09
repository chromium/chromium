// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/media_access_handler.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_prefs/user_prefs.h"
#include "components/webrtc/media_stream_devices_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_capture_devices.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/common/content_features.h"
#else
#include "chrome/browser/media/webrtc/display_media_access_handler.h"
#endif  //  BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/media/chromeos_login_and_lock_media_access_handler.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/controlled_frame/controlled_frame_media_access_handler.h"
#include "chrome/browser/media/extension_media_access_handler.h"
#include "chrome/browser/media/webrtc/desktop_capture_access_handler.h"
#include "chrome/browser/media/webrtc/tab_capture_access_handler.h"
#include "extensions/browser/extension_registry.h"  // nogncheck
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

using blink::MediaStreamDevices;
using content::BrowserThread;
using content::MediaCaptureDevices;

namespace {

content::WebContents* WebContentsFromIds(int render_process_id,
                                         int render_frame_id) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_process_id, render_frame_id));
  return web_contents;
}

}  // namespace

MediaCaptureDevicesDispatcher* MediaCaptureDevicesDispatcher::GetInstance() {
  return base::Singleton<MediaCaptureDevicesDispatcher>::get();
}

MediaCaptureDevicesDispatcher::MediaCaptureDevicesDispatcher()
    : is_device_enumeration_disabled_(false),
      media_stream_capture_indicator_(new MediaStreamCaptureIndicator()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if !BUILDFLAG(IS_ANDROID)
  media_access_handlers_.push_back(
      std::make_unique<DisplayMediaAccessHandler>());
#endif  //  BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  media_access_handlers_.push_back(
      std::make_unique<ChromeOSLoginAndLockMediaAccessHandler>());
#endif
  media_access_handlers_.push_back(
      std::make_unique<ExtensionMediaAccessHandler>());
  media_access_handlers_.push_back(
      std::make_unique<DesktopCaptureAccessHandler>());
  media_access_handlers_.push_back(std::make_unique<TabCaptureAccessHandler>());
  media_access_handlers_.push_back(
      std::make_unique<controlled_frame::ControlledFrameMediaAccessHandler>());
#endif
  media_access_handlers_.push_back(
      std::make_unique<PermissionBubbleMediaAccessHandler>());
}

MediaCaptureDevicesDispatcher::~MediaCaptureDevicesDispatcher() {}

void MediaCaptureDevicesDispatcher::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kDefaultAudioCaptureDeviceDeprecated,
                               std::string());
  registry->RegisterStringPref(prefs::kDefaultVideoCaptureDeviceDeprecated,
                               std::string());
}

void MediaCaptureDevicesDispatcher::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
  }
}

void MediaCaptureDevicesDispatcher::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

void MediaCaptureDevicesDispatcher::ProcessMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(IS_ANDROID)
  // Kill switch for getDisplayMedia() on browser side to prevent renderer from
  // bypassing blink side checks.
  if (request.video_type ==
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE &&
      !base::FeatureList::IsEnabled(features::kUserMediaScreenCapturing)) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED, nullptr);
    return;
  }
#endif

  for (const auto& handler : media_access_handlers_) {
    if (handler->SupportsStreamType(web_contents, request.video_type,
                                    extension) ||
        handler->SupportsStreamType(web_contents, request.audio_type,
                                    extension)) {
      handler->HandleRequest(web_contents, request, std::move(callback),
                             extension);
      return;
    }
  }
  std::move(callback).Run(blink::mojom::StreamDevicesSet(),
                          blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
                          nullptr);
}

bool MediaCaptureDevicesDispatcher::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CheckMediaAccessPermission(render_frame_host, security_origin, type,
                                    nullptr);
}

bool MediaCaptureDevicesDispatcher::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& handler : media_access_handlers_) {
    if (handler->SupportsStreamType(
            content::WebContents::FromRenderFrameHost(render_frame_host), type,
            extension)) {
      return handler->CheckMediaAccessPermission(
          render_frame_host, security_origin, type, extension);
    }
  }
  return false;
}

void MediaCaptureDevicesDispatcher::DisableDeviceEnumerationForTesting() {
  is_device_enumeration_disabled_ = true;
}

const MediaStreamDevices&
MediaCaptureDevicesDispatcher::GetAudioCaptureDevices() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_device_enumeration_disabled_ || !test_audio_devices_.empty())
    return test_audio_devices_;

  return webrtc::MediaStreamDeviceEnumeratorImpl::GetAudioCaptureDevices();
}

const MediaStreamDevices&
MediaCaptureDevicesDispatcher::GetVideoCaptureDevices() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_device_enumeration_disabled_ || !test_video_devices_.empty())
    return test_video_devices_;

  return webrtc::MediaStreamDeviceEnumeratorImpl::GetVideoCaptureDevices();
}

const std::optional<blink::MediaStreamDevice>
MediaCaptureDevicesDispatcher::GetPreferredAudioDeviceForBrowserContext(
    content::BrowserContext* context,
    const std::vector<std::string>& eligible_device_ids) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto audio_devices = GetAudioCaptureDevices();
  if (!eligible_device_ids.empty()) {
    audio_devices =
        webrtc::FilterMediaDevices(audio_devices, eligible_device_ids);
  }
  media_prefs::PreferenceRankAudioDeviceInfos(
      *user_prefs::UserPrefs::Get(context), audio_devices);

  if (audio_devices.empty()) {
    return std::nullopt;
  }
  return audio_devices.front();
}

const std::optional<blink::MediaStreamDevice>
MediaCaptureDevicesDispatcher::GetPreferredVideoDeviceForBrowserContext(
    content::BrowserContext* context,
    const std::vector<std::string>& eligible_device_ids) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto video_devices = GetVideoCaptureDevices();
  if (!eligible_device_ids.empty()) {
    video_devices =
        webrtc::FilterMediaDevices(video_devices, eligible_device_ids);
  }
  media_prefs::PreferenceRankVideoDeviceInfos(
      *user_prefs::UserPrefs::Get(context), video_devices);

  if (video_devices.empty()) {
    return std::nullopt;
  }
  return video_devices.front();
}

scoped_refptr<MediaStreamCaptureIndicator>
MediaCaptureDevicesDispatcher::GetMediaStreamCaptureIndicator() {
  return media_stream_capture_indicator_;
}

void MediaCaptureDevicesDispatcher::OnAudioCaptureDevicesChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaCaptureDevicesDispatcher::NotifyAudioDevicesChangedOnUIThread,
          base::Unretained(this)));
}

void MediaCaptureDevicesDispatcher::OnVideoCaptureDevicesChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaCaptureDevicesDispatcher::NotifyVideoDevicesChangedOnUIThread,
          base::Unretained(this)));
}

void MediaCaptureDevicesDispatcher::OnMediaRequestStateChanged(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    const GURL& security_origin,
    blink::mojom::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaCaptureDevicesDispatcher::UpdateMediaRequestStateOnUIThread,
          base::Unretained(this), render_process_id, render_frame_id,
          page_request_id, stream_type, state));
}

void MediaCaptureDevicesDispatcher::OnCreatingAudioStream(int render_process_id,
                                                          int render_frame_id) {
  // TODO(crbug.com/41385872): Figure out how to simplify threading here.
  // Currently, this will either always be called on the UI thread, or always
  // on the IO thread, depending on how far along the work to migrate to the
  // audio service has progressed. The rest of the methods of the
  // content::MediaObserver are always called on the IO thread.
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    OnCreatingAudioStreamOnUIThread(render_process_id, render_frame_id);
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaCaptureDevicesDispatcher::OnCreatingAudioStreamOnUIThread,
          base::Unretained(this), render_process_id, render_frame_id));
}

void MediaCaptureDevicesDispatcher::NotifyAudioDevicesChangedOnUIThread() {
  MediaStreamDevices devices = GetAudioCaptureDevices();
  for (auto& observer : observers_)
    observer.OnUpdateAudioDevices(devices);
}

void MediaCaptureDevicesDispatcher::NotifyVideoDevicesChangedOnUIThread() {
  MediaStreamDevices devices = GetVideoCaptureDevices();
  for (auto& observer : observers_)
    observer.OnUpdateVideoDevices(devices);
}

void MediaCaptureDevicesDispatcher::UpdateMediaRequestStateOnUIThread(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::mojom::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& handler : media_access_handlers_) {
    if (handler->SupportsStreamType(
            WebContentsFromIds(render_process_id, render_frame_id), stream_type,
            nullptr)) {
      handler->UpdateMediaRequestState(render_process_id, render_frame_id,
                                       page_request_id, stream_type, state);
      break;
    }
  }

  for (auto& observer : observers_) {
    observer.OnRequestUpdate(render_process_id, render_frame_id, stream_type,
                             state);
  }
}

void MediaCaptureDevicesDispatcher::OnCreatingAudioStreamOnUIThread(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto& observer : observers_)
    observer.OnCreatingAudioStream(render_process_id, render_frame_id);
}

bool MediaCaptureDevicesDispatcher::IsInsecureCapturingInProgress(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& handler : media_access_handlers_) {
    if (handler->IsInsecureCapturingInProgress(render_process_id,
                                               render_frame_id))
      return true;
  }
  return false;
}

void MediaCaptureDevicesDispatcher::SetTestAudioCaptureDevices(
    const MediaStreamDevices& devices) {
  test_audio_devices_ = devices;
}

void MediaCaptureDevicesDispatcher::SetTestVideoCaptureDevices(
    const MediaStreamDevices& devices) {
  test_video_devices_ = devices;
}

void MediaCaptureDevicesDispatcher::OnSetCapturingLinkSecured(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::mojom::MediaStreamType stream_type,
    bool is_secure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!blink::IsVideoScreenCaptureMediaType(stream_type))
    return;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaCaptureDevicesDispatcher::UpdateVideoScreenCaptureStatus,
          base::Unretained(this), render_process_id, render_frame_id,
          page_request_id, stream_type, is_secure));
}

void MediaCaptureDevicesDispatcher::UpdateVideoScreenCaptureStatus(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::mojom::MediaStreamType stream_type,
    bool is_secure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(blink::IsVideoScreenCaptureMediaType(stream_type));

  for (const auto& handler : media_access_handlers_) {
    if (handler->SupportsStreamType(
            WebContentsFromIds(render_process_id, render_frame_id), stream_type,
            nullptr)) {
      handler->UpdateVideoScreenCaptureStatus(
          render_process_id, render_frame_id, page_request_id, is_secure);
      break;
    }
  }
}
