// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/tab_sharing_indicator_android.h"

#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace {
// The MediaStreamUI interface specifies returning 0 if no window ID is
// applicable.
constexpr gfx::NativeViewId kNoWindowId = 0;
}  // namespace

TabSharingIndicatorAndroid::TabSharingIndicatorAndroid(
    const content::DesktopMediaID& media_id)
    : media_id_(media_id) {}

TabSharingIndicatorAndroid::~TabSharingIndicatorAndroid() {
  StopSharing();
}

void TabSharingIndicatorAndroid::StopSharing() {
  if (stop_callback_) {
    std::move(stop_callback_).Run();
  }
}

gfx::NativeViewId TabSharingIndicatorAndroid::OnStarted(
    base::OnceClosure stop_callback,
    content::MediaStreamUI::SourceCallback source_callback,
    const std::vector<content::DesktopMediaID>& media_ids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(media_id_.type == content::DesktopMediaID::TYPE_WEB_CONTENTS);

  DCHECK(!stop_callback_);
  stop_callback_ = std::move(stop_callback);

  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(
              media_id_.web_contents_id.render_process_id,
              media_id_.web_contents_id.main_render_frame_id));

  if (!web_contents) {
    StopSharing();
    return kNoWindowId;
  }

  // Create and register a stream to signal that the tab is being mirrored.
  const blink::MediaStreamDevice device(
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
      media_id_.ToString(), std::string());

  blink::mojom::StreamDevices devices;
  devices.video_device = device;
  tab_sharing_indicator_ui_ = MediaCaptureDevicesDispatcher::GetInstance()
                                  ->GetMediaStreamCaptureIndicator()
                                  ->RegisterMediaStream(web_contents, devices);

  if (tab_sharing_indicator_ui_) {
    tab_sharing_indicator_ui_->OnStarted(
        base::DoNothing(), content::MediaStreamUI::SourceCallback(),
        std::string(), {}, content::MediaStreamUI::StateChangeCallback());
  } else {
    StopSharing();
  }

  return kNoWindowId;
}
