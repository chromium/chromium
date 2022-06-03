// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/tab_capture_access_handler.h"

#include <utility>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/same_origin_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

namespace {
// This helper class is designed to live as long as the capture, and is used
// when no other MediaStreamUI object is used. If the capture violates the
// SameOrigin EnterprisePolicy, we'll abort the capture and show a dialog that
// we have stopped it.
class SameOriginPolicyUI : public MediaStreamUI {
 public:
  // Since we own the observer, the base::Unretained for the callback is safe.
  SameOriginPolicyUI(content::WebContents* observed_contents,
                     const GURL& reference_origin)
      : observer_(
            observed_contents,
            reference_origin,
            base::BindRepeating(&SameOriginPolicyUI::OnSameOriginStateChange,
                                base::Unretained(this))) {}
  // Called when stream capture is stopped.
  ~SameOriginPolicyUI() override = default;

  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) override {
    stop_callback_ = std::move(stop_callback);
    return 0;
  }

 private:
  void OnSameOriginStateChange(content::WebContents* wc) {
    std::move(stop_callback_).Run();
    capture_policy::ShowCaptureTerminatedDialog(wc);
  }

  SameOriginObserver observer_;
  base::OnceClosure stop_callback_;
};
}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/dlp/dlp_content_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TabCaptureAccessHandler::TabCaptureAccessHandler() = default;

TabCaptureAccessHandler::~TabCaptureAccessHandler() = default;

bool TabCaptureAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return type == blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE ||
         type == blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE;
}

bool TabCaptureAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return false;
}

void TabCaptureAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  blink::MediaStreamDevices devices;
  std::unique_ptr<content::MediaStreamUI> ui;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  extensions::TabCaptureRegistry* tab_capture_registry =
      extensions::TabCaptureRegistry::Get(profile);
  if (!tab_capture_registry) {
    NOTREACHED();
    std::move(callback).Run(
        devices, blink::mojom::MediaStreamRequestResult::INVALID_STATE,
        std::move(ui));
    return;
  }

  AllowedScreenCaptureLevel capture_level =
      capture_policy::GetAllowedCaptureLevel(request.security_origin,
                                             web_contents);
  DesktopMediaList::WebContentsFilter can_show_web_contents =
      capture_policy::GetIncludableWebContentsFilter(request.security_origin,
                                                     capture_level);

  content::WebContents* target_web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(request.render_process_id,
                                           request.render_frame_id));
  if (!can_show_web_contents.Run(target_web_contents)) {
    std::move(callback).Run(
        devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::move(ui));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (request.video_type ==
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE) {
    content::DesktopMediaID media_id(
        content::DesktopMediaID::TYPE_WEB_CONTENTS, /*id=*/0,
        content::WebContentsMediaCaptureId(request.render_process_id,
                                           request.render_frame_id));
    if (policy::DlpContentManager::Get()->IsScreenCaptureRestricted(media_id)) {
      std::move(callback).Run(
          devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
          std::move(ui));
      return;
    }
  }
#endif

  // |extension| may be null if the tabCapture starts with
  // tabCapture.getMediaStreamId().
  // TODO(crbug.com/831722): Deprecate tabCaptureRegistry soon.
  const std::string extension_id = extension ? extension->id() : "";
  const bool tab_capture_allowed = tab_capture_registry->VerifyRequest(
      request.render_process_id, request.render_frame_id, extension_id);

  if (request.audio_type ==
          blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE &&
      tab_capture_allowed) {
    devices.push_back(blink::MediaStreamDevice(
        blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE, std::string(),
        std::string()));
  }

  if (request.video_type ==
          blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE &&
      tab_capture_allowed) {
    devices.push_back(blink::MediaStreamDevice(
        blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE, std::string(),
        std::string()));
  }

  if (!devices.empty()) {
    std::unique_ptr<MediaStreamUI> media_ui;
    if (capture_level == AllowedScreenCaptureLevel::kSameOrigin) {
      media_ui = std::make_unique<SameOriginPolicyUI>(target_web_contents,
                                                      request.security_origin);
    }
    ui = MediaCaptureDevicesDispatcher::GetInstance()
             ->GetMediaStreamCaptureIndicator()
             ->RegisterMediaStream(web_contents, devices, std::move(media_ui));
  }
  UpdateExtensionTrusted(request, extension);
  std::move(callback).Run(
      devices,
      devices.empty() ? blink::mojom::MediaStreamRequestResult::INVALID_STATE
                      : blink::mojom::MediaStreamRequestResult::OK,
      std::move(ui));
}
