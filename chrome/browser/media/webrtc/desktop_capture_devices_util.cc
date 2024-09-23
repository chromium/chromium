// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_capture_devices_util.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/common/content_features.h"
#include "media/audio/audio_device_description.h"
#include "media/mojo/mojom/capture_handle.mojom.h"
#include "media/mojo/mojom/display_media_information.mojom.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/base/l10n/l10n_util.h"

// If this feature is disabled, the SuppressLocalAudioPlayback constraint
// will become no-op if the user chooses to share a tab.
BASE_FEATURE(kSuppressLocalAudioPlaybackForTabAudio,
             "SuppressLocalAudioPlaybackForTabAudio",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If this feature is disabled, the SuppressLocalAudioPlayback constraint
// will become no-op if the user chooses to share a screen.
BASE_FEATURE(kSuppressLocalAudioPlaybackForSystemAudio,
             "SuppressLocalAudioPlaybackForSystemAudio",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// TODO(crbug.com/40181897): Eliminate code duplication with
// capture_handle_manager.cc.
media::mojom::CaptureHandlePtr CreateCaptureHandle(
    content::WebContents* capturer,
    const url::Origin& capturer_origin,
    const content::DesktopMediaID& captured_id) {
  if (capturer_origin.opaque()) {
    return nullptr;
  }

  content::RenderFrameHost* const captured_rfh =
      content::RenderFrameHost::FromID(
          captured_id.web_contents_id.render_process_id,
          captured_id.web_contents_id.main_render_frame_id);
  if (!captured_rfh || !captured_rfh->IsActive()) {
    return nullptr;
  }

  content::WebContents* const captured =
      content::WebContents::FromRenderFrameHost(captured_rfh);
  if (!captured) {
    return nullptr;
  }

  const auto& captured_config = captured->GetCaptureHandleConfig();
  if (!captured_config.all_origins_permitted &&
      base::ranges::none_of(
          captured_config.permitted_origins,
          [capturer_origin](const url::Origin& permitted_origin) {
            return capturer_origin.IsSameOriginWith(permitted_origin);
          })) {
    return nullptr;
  }

  // Observing CaptureHandle when either the capturing or the captured party
  // is incognito is disallowed, except for self-capture.
  if (capturer->GetPrimaryMainFrame() != captured->GetPrimaryMainFrame()) {
    if (capturer->GetBrowserContext()->IsOffTheRecord() ||
        captured->GetBrowserContext()->IsOffTheRecord()) {
      return nullptr;
    }
  }

  if (!captured_config.expose_origin &&
      captured_config.capture_handle.empty()) {
    return nullptr;
  }

  auto result = media::mojom::CaptureHandle::New();
  if (captured_config.expose_origin) {
    result->origin = captured->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  }
  result->capture_handle = captured_config.capture_handle;

  return result;
}

std::optional<int> GetZoomLevel(content::WebContents* capturer,
                                const content::DesktopMediaID& captured_id) {
  content::RenderFrameHost* const captured_rfh =
      content::RenderFrameHost::FromID(
          captured_id.web_contents_id.render_process_id,
          captured_id.web_contents_id.main_render_frame_id);
  if (!captured_rfh || !captured_rfh->IsActive()) {
    return std::nullopt;
  }

  content::WebContents* const captured_wc =
      content::WebContents::FromRenderFrameHost(captured_rfh);
  if (!captured_wc) {
    return std::nullopt;
  }

  double zoom_level = blink::ZoomLevelToZoomFactor(
      content::HostZoomMap::GetZoomLevel(captured_wc));
  return std::round(100 * zoom_level);
}

media::mojom::DisplayMediaInformationPtr
DesktopMediaIDToDisplayMediaInformation(
    content::WebContents* capturer,
    const url::Origin& capturer_origin,
    const content::DesktopMediaID& media_id) {
  media::mojom::DisplayCaptureSurfaceType display_surface =
      media::mojom::DisplayCaptureSurfaceType::MONITOR;
  bool logical_surface = true;
  media::mojom::CursorCaptureType cursor =
      media::mojom::CursorCaptureType::NEVER;
#if defined(USE_AURA)
  const bool uses_aura = media_id.window_id != content::DesktopMediaID::kNullId;
#else
  const bool uses_aura = false;
#endif  // defined(USE_AURA)

  media::mojom::CaptureHandlePtr capture_handle;
  int zoom_level = 100;
  switch (media_id.type) {
    case content::DesktopMediaID::TYPE_SCREEN:
      display_surface = media::mojom::DisplayCaptureSurfaceType::MONITOR;
      cursor = uses_aura ? media::mojom::CursorCaptureType::MOTION
                         : media::mojom::CursorCaptureType::ALWAYS;
      break;
    case content::DesktopMediaID::TYPE_WINDOW:
      display_surface = media::mojom::DisplayCaptureSurfaceType::WINDOW;
      cursor = uses_aura ? media::mojom::CursorCaptureType::MOTION
                         : media::mojom::CursorCaptureType::ALWAYS;
      break;
    case content::DesktopMediaID::TYPE_WEB_CONTENTS:
      display_surface = media::mojom::DisplayCaptureSurfaceType::BROWSER;
      cursor = media::mojom::CursorCaptureType::MOTION;
      capture_handle = CreateCaptureHandle(capturer, capturer_origin, media_id);
      if (base::FeatureList::IsEnabled(
              features::kCapturedSurfaceControlKillswitch)) {
        zoom_level = GetZoomLevel(capturer, media_id).value_or(zoom_level);
      }
      break;
    case content::DesktopMediaID::TYPE_NONE:
      break;
  }

  return media::mojom::DisplayMediaInformation::New(
      display_surface, logical_surface, cursor, std::move(capture_handle),
      zoom_level);
}

std::u16string GetNotificationText(const std::u16string& application_title,
                                   bool capture_audio,
                                   content::DesktopMediaID::Type capture_type) {
  if (capture_audio) {
    switch (capture_type) {
      case content::DesktopMediaID::TYPE_SCREEN:
        return l10n_util::GetStringFUTF16(
            IDS_MEDIA_SCREEN_CAPTURE_WITH_AUDIO_NOTIFICATION_TEXT,
            application_title);
      case content::DesktopMediaID::TYPE_WEB_CONTENTS:
        return l10n_util::GetStringFUTF16(
            IDS_MEDIA_TAB_CAPTURE_WITH_AUDIO_NOTIFICATION_TEXT,
            application_title);
      case content::DesktopMediaID::TYPE_NONE:
      case content::DesktopMediaID::TYPE_WINDOW:
        NOTREACHED_IN_MIGRATION();
    }
  } else {
    switch (capture_type) {
      case content::DesktopMediaID::TYPE_SCREEN:
        return l10n_util::GetStringFUTF16(
            IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TEXT, application_title);
      case content::DesktopMediaID::TYPE_WINDOW:
        return l10n_util::GetStringFUTF16(
            IDS_MEDIA_WINDOW_CAPTURE_NOTIFICATION_TEXT, application_title);
      case content::DesktopMediaID::TYPE_WEB_CONTENTS:
        return l10n_util::GetStringFUTF16(
            IDS_MEDIA_TAB_CAPTURE_NOTIFICATION_TEXT, application_title);
      case content::DesktopMediaID::TYPE_NONE:
        NOTREACHED_IN_MIGRATION();
    }
  }
  return std::u16string();
}

std::string DeviceNamePrefix(
    content::WebContents* web_contents,
    blink::mojom::MediaStreamType requested_stream_type,
    const content::DesktopMediaID& media_id) {
  if (!web_contents ||
      requested_stream_type !=
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB) {
    return std::string();
  }

  // Note that all of these must still be checked, as the explicit-selection
  // dialog for DISPLAY_VIDEO_CAPTURE_THIS_TAB could still return something
  // other than the current tab - be it a screen, window, or another tab.
  if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS &&
      web_contents->GetPrimaryMainFrame()->GetProcess()->GetID() ==
          media_id.web_contents_id.render_process_id &&
      web_contents->GetPrimaryMainFrame()->GetRoutingID() ==
          media_id.web_contents_id.main_render_frame_id) {
    return "current-";
  }

  return std::string();
}

std::string DeviceName(content::WebContents* web_contents,
                       blink::mojom::MediaStreamType requested_stream_type,
                       const content::DesktopMediaID& media_id) {
  const std::string prefix =
      DeviceNamePrefix(web_contents, requested_stream_type, media_id);
  if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) {
    return base::StrCat({prefix, content::kWebContentsCaptureScheme,
                         base::UnguessableToken::Create().ToString()});
  } else {
    // TODO(crbug.com/40793276): MediaStreamTrack.label leaks internal state for
    // screen/window
    return base::StrCat({prefix, media_id.ToString()});
  }
}

}  // namespace

std::unique_ptr<content::MediaStreamUI> GetDevicesForDesktopCapture(
    const content::MediaStreamRequest& request,
    content::WebContents* web_contents,
    const content::DesktopMediaID& media_id,
    bool capture_audio,
    bool disable_local_echo,
    bool suppress_local_audio_playback,
    bool display_notification,
    const std::u16string& application_title,
    bool captured_surface_control_active,
    blink::mojom::StreamDevices& out_devices) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(2) << __func__ << ": media_id " << media_id.ToString()
           << ", capture_audio " << capture_audio << ", disable_local_echo "
           << disable_local_echo << ", suppress_local_audio_playback "
           << suppress_local_audio_playback << ", display_notification "
           << display_notification << ", application_title "
           << application_title;

  // Add selected desktop source to the list.
  blink::MediaStreamDevice device(
      request.video_type, media_id.ToString(),
      DeviceName(web_contents, request.video_type, media_id));
  device.display_media_info = DesktopMediaIDToDisplayMediaInformation(
      web_contents, url::Origin::Create(request.security_origin), media_id);
  if (request.video_type != blink::mojom::MediaStreamType::NO_SERVICE) {
    out_devices.video_device = device;
  }

  if (capture_audio) {
    DCHECK_NE(request.audio_type, blink::mojom::MediaStreamType::NO_SERVICE);

    if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) {
      content::WebContentsMediaCaptureId web_id = media_id.web_contents_id;
      if (!base::FeatureList::IsEnabled(
              kSuppressLocalAudioPlaybackForTabAudio)) {
        suppress_local_audio_playback = false;  // Surface-specific killswitch.
      }
      // TODO(crbug.com/40244028): Deprecate disable_local_echo, support the
      // same functionality based only on suppress_local_audio_playback.
      web_id.disable_local_echo =
          disable_local_echo || suppress_local_audio_playback;
      out_devices.audio_device = blink::MediaStreamDevice(
          request.audio_type, web_id.ToString(), "Tab audio");
    } else {
      if (!base::FeatureList::IsEnabled(
              kSuppressLocalAudioPlaybackForSystemAudio)) {
        suppress_local_audio_playback = false;  // Surface-specific killswitch.
      }
      // Use the special loopback device ID for system audio capture.
      out_devices.audio_device = blink::MediaStreamDevice(
          request.audio_type,
          (disable_local_echo || suppress_local_audio_playback
               ? media::AudioDeviceDescription::kLoopbackWithMuteDeviceId
               : media::AudioDeviceDescription::kLoopbackInputDeviceId),
          "System Audio");
    }
    out_devices.audio_device->display_media_info =
        DesktopMediaIDToDisplayMediaInformation(
            web_contents, url::Origin::Create(request.security_origin),
            media_id);
  }

  // If required, register to display the notification for stream capture.
  std::unique_ptr<MediaStreamUI> notification_ui;
  if (display_notification) {
    if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) {
      content::GlobalRenderFrameHostId capturer_id;
      if (web_contents && web_contents->GetPrimaryMainFrame()) {
        capturer_id = web_contents->GetPrimaryMainFrame()->GetGlobalId();
      }
      const bool app_preferred_current_tab =
          request.video_type ==
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB;
      notification_ui =
          TabSharingUI::Create(capturer_id, media_id, application_title,
                               /*favicons_used_for_switch_to_tab_button=*/false,
                               app_preferred_current_tab,
                               TabSharingInfoBarDelegate::TabShareType::CAPTURE,
                               captured_surface_control_active);
    } else {
      notification_ui = ScreenCaptureNotificationUI::Create(
          GetNotificationText(application_title, capture_audio, media_id.type),
          web_contents);
    }
  }

  return MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->RegisterMediaStream(web_contents, out_devices,
                            std::move(notification_ui), application_title);
}
