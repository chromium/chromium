// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/tab_sharing_ui_android.h"

#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabSharingUIBridge_jni.h"

namespace {
// The MediaStreamUI interface specifies returning 0 if no window ID is
// applicable.
constexpr gfx::NativeViewId kNoWindowId = 0;
}  // namespace

TabSharingUIAndroid::TabSharingUIAndroid(
    content::WebContents* capturer_web_contents,
    const content::DesktopMediaID& media_id)
    : capturer_web_contents_(capturer_web_contents->GetWeakPtr()),
      media_id_(media_id) {}

TabSharingUIAndroid::~TabSharingUIAndroid() {
  StopSharing();
  if (java_bridge_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_TabSharingUIBridge_destroy(env, java_bridge_);
    java_bridge_.Reset();
  }
}

void TabSharingUIAndroid::StopSharing() {
  if (stop_callback_) {
    std::move(stop_callback_).Run();
  }
}

void TabSharingUIAndroid::ChangeSource(content::WebContents* new_source) {
  // TODO(crbug.com/480747775): implement this by saving the source_callback
  // provided OnStarted() and run it here.
}

gfx::NativeViewId TabSharingUIAndroid::OnStarted(
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
  tab_capture_indicator_ui_ = MediaCaptureDevicesDispatcher::GetInstance()
                                  ->GetMediaStreamCaptureIndicator()
                                  ->RegisterMediaStream(web_contents, devices);

  if (tab_capture_indicator_ui_) {
    tab_capture_indicator_ui_->OnStarted(
        base::DoNothing(), content::MediaStreamUI::SourceCallback(),
        std::string(), {}, content::MediaStreamUI::StateChangeCallback());
    if (capturer_web_contents_ &&
        base::FeatureList::IsEnabled(
            chrome::android::kTabSharingToolbarAndroid)) {
      JNIEnv* env = base::android::AttachCurrentThread();
      java_bridge_ = Java_TabSharingUIBridge_create(
          env, reinterpret_cast<intptr_t>(this), capturer_web_contents_.get(),
          web_contents);
    }
  } else {
    StopSharing();
  }

  return kNoWindowId;
}

DEFINE_JNI(TabSharingUIBridge)
