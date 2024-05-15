// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/tab_capture_access_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/same_origin_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {
// This helper class is designed to live as long as the capture, and is used
// when no other MediaStreamUI object is used. If the capture violates the
// SameOrigin EnterprisePolicy, we'll abort the capture and show a dialog that
// we have stopped it.
class SameOriginPolicyUI : public MediaStreamUI {
 public:
  // Since we own the observer, the base::Unretained for the callback is safe.
  SameOriginPolicyUI(content::WebContents* observed_contents,
                     const url::Origin& reference_origin)
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

// Returns an instance of MediaStreamUI to be passed to content layer and stores
// the list of media stream devices for tab capture in |out_devices|.
std::unique_ptr<content::MediaStreamUI> GetMediaStreamUI(
    const content::MediaStreamRequest& request,
    content::WebContents* web_contents,
    std::unique_ptr<MediaStreamUI> media_ui,
    blink::mojom::StreamDevices& out_devices) {
  if (request.audio_type ==
      blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE) {
    out_devices.audio_device = blink::MediaStreamDevice(
        blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE,
        /*id=*/std::string(),
        /*name=*/std::string());
  }

  if (request.video_type ==
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE) {
    out_devices.video_device = blink::MediaStreamDevice(
        blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
        /*id=*/std::string(),
        /*name=*/std::string());
  }

  return MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->RegisterMediaStream(web_contents, out_devices, std::move(media_ui));
}

}  // namespace

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
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return false;
}

void TabCaptureAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  extensions::TabCaptureRegistry* tab_capture_registry =
      extensions::TabCaptureRegistry::Get(profile);
  if (!tab_capture_registry) {
    NOTREACHED_IN_MIGRATION();
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, /*ui=*/nullptr);
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
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        /*ui=*/nullptr);
    return;
  }

  // |extension| may be null if the tabCapture starts with
  // tabCapture.getMediaStreamId().
  // TODO(crbug.com/40571241): Deprecate tabCaptureRegistry soon.
  const std::string extension_id = extension ? extension->id() : "";
  if (!tab_capture_registry->VerifyRequest(
          request.render_process_id, request.render_frame_id, extension_id)) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, /*ui=*/nullptr);
    return;
  }

  std::unique_ptr<MediaStreamUI> media_ui;
  if (capture_level == AllowedScreenCaptureLevel::kSameOrigin) {
    media_ui = std::make_unique<SameOriginPolicyUI>(
        target_web_contents, url::Origin::Create(request.security_origin));
  }
  const bool is_allowlisted_extension =
      IsExtensionAllowedForScreenCapture(extension);

#if BUILDFLAG(IS_CHROMEOS)
  if (request.video_type ==
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE) {
    // Use extension name as title for extensions and host/origin for drive-by
    // web.
    std::u16string application_title =
        extension
            ? base::UTF8ToUTF16(extension->name())
            : url_formatter::FormatOriginForSecurityDisplay(
                  web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
                  url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
    content::DesktopMediaID media_id(
        content::DesktopMediaID::TYPE_WEB_CONTENTS, /*id=*/0,
        content::WebContentsMediaCaptureId(request.render_process_id,
                                           request.render_frame_id));
    // base::Unretained(this) is safe because TabCaptureAccessHandler is owned
    // by MediaCaptureDevicesDispatcher, which is a lazy singleton which is
    // destroyed when the browser process terminates.
    policy::DlpContentManager::Get()->CheckScreenShareRestriction(
        media_id, application_title,
        base::BindOnce(
            &TabCaptureAccessHandler::OnDlpRestrictionChecked,
            base::Unretained(this), web_contents->GetWeakPtr(),
            std::make_unique<PendingAccessRequest>(
                /*picker=*/nullptr, request, std::move(callback),
                application_title,
                /*display_notification=*/false, is_allowlisted_extension),
            std::move(media_ui)));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  AcceptRequest(web_contents, request, std::move(callback),
                is_allowlisted_extension, std::move(media_ui));
}

void TabCaptureAccessHandler::AcceptRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    bool is_allowlisted_extension,
    std::unique_ptr<MediaStreamUI> media_ui) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  // TOOD(crbug.com/1300883): Generalize to multiple streams.
  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& stream_devices =
      *stream_devices_set.stream_devices[0];
  std::unique_ptr<content::MediaStreamUI> ui = GetMediaStreamUI(
      request, web_contents, std::move(media_ui), stream_devices);
  DCHECK(stream_devices.audio_device.has_value() ||
         stream_devices.video_device.has_value());

  UpdateExtensionTrusted(request, is_allowlisted_extension);
  std::move(callback).Run(stream_devices_set,
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));
}

#if BUILDFLAG(IS_CHROMEOS)
void TabCaptureAccessHandler::OnDlpRestrictionChecked(
    base::WeakPtr<content::WebContents> web_contents,
    std::unique_ptr<PendingAccessRequest> pending_request,
    std::unique_ptr<MediaStreamUI> media_ui,
    bool is_dlp_allowed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents) {
    return;
  }

  if (is_dlp_allowed) {
    AcceptRequest(web_contents.get(), pending_request->request,
                  std::move(pending_request->callback),
                  pending_request->is_allowlisted_extension,
                  std::move(media_ui));
  } else {
    std::move(pending_request->callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
             /*ui=*/nullptr);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)
