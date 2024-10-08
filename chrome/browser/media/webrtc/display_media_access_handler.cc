// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/display_media_access_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/desktop_capture_devices_util.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory_impl.h"
#include "chrome/browser/media/webrtc/native_desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif  // BUILDFLAG(IS_MAC)

// If enabled, a capture request on the opener tab of a Picture in Picture
// window will show up in the PiP window instead if the PiP window is active.
// Otherwise, it will show up in the opener because that's where the capture
// request originated.
BASE_FEATURE(kDisplayCaptureUiInPipIfActive,
             "DisplayCaptureUiInPipIfActive",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

constexpr UrlIdentity::TypeSet allowed_types = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kIsolatedWebApp,
    UrlIdentity::Type::kFile, UrlIdentity::Type::kChromeExtension};

constexpr UrlIdentity::FormatOptions options = {
    .default_options = {
        UrlIdentity::DefaultFormatOptions::kOmitCryptographicScheme}};

// Helper function to get the title of the calling application.
std::u16string GetApplicationTitle(content::WebContents* web_contents) {
  DCHECK(web_contents);
  GURL content_origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin().GetURL();
  UrlIdentity url_identity = UrlIdentity::CreateFromUrl(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      content_origin, allowed_types, options);
  return url_identity.name;
}

}  // namespace

DisplayMediaAccessHandler::DisplayMediaAccessHandler()
    : picker_factory_(new DesktopMediaPickerFactoryImpl()),
      web_contents_collection_(this) {}

DisplayMediaAccessHandler::DisplayMediaAccessHandler(
    std::unique_ptr<DesktopMediaPickerFactory> picker_factory,
    bool display_notification)
    : display_notification_(display_notification),
      picker_factory_(std::move(picker_factory)),
      web_contents_collection_(this) {}

DisplayMediaAccessHandler::~DisplayMediaAccessHandler() = default;

bool DisplayMediaAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType stream_type,
    const extensions::Extension* extension) {
  return stream_type == blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE ||
         stream_type == blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE ||
         stream_type ==
             blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB ||
         stream_type ==
             blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET;
  // This class handles MEDIA_DISPLAY_AUDIO_CAPTURE as well, but only if it is
  // accompanied by MEDIA_DISPLAY_VIDEO_CAPTURE request as per spec.
  // https://w3c.github.io/mediacapture-screen-share/#mediadevices-additions
  // 5.1 MediaDevices Additions
  // "The user agent MUST reject audio-only requests."
}

bool DisplayMediaAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return false;
}

void DisplayMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  if (capture_policy::GetAllowedCaptureLevel(request.security_origin,
                                             web_contents) ==
      AllowedScreenCaptureLevel::kDisallowed) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        /*ui=*/nullptr);
    return;
  }

  // SafeBrowsing Delayed Warnings experiment can delay some SafeBrowsing
  // warnings until user interaction. If the current page has a delayed warning,
  // it'll have a user interaction observer attached. Show the warning
  // immediately in that case.
  safe_browsing::SafeBrowsingUserInteractionObserver* observer =
      safe_browsing::SafeBrowsingUserInteractionObserver::FromWebContents(
          web_contents);
  if (observer) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        /*ui=*/nullptr);
    observer->OnDesktopCaptureRequest();
    return;
  }

#if BUILDFLAG(IS_MAC)
  // Do not allow picker UI to be shown on a page that isn't in the foreground
  // in Mac, because the UI implementation in Mac pops a window over any content
  // which might be confusing for the users. See https://crbug.com/1407733 for
  // details.
  // TODO(emircan): Remove this once Mac UI doesn't use a window.
  if (web_contents->GetVisibility() != content::Visibility::VISIBLE &&
      request.request_type != blink::MEDIA_DEVICE_UPDATE) {
    LOG(ERROR) << "Do not allow getDisplayMedia() on a backgrounded page.";
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, /*ui=*/nullptr);
    return;
  }
#endif  // BUILDFLAG(IS_MAC)

  if (request.request_type == blink::MEDIA_DEVICE_UPDATE) {
    CHECK(!request.requested_video_device_ids.empty());
    CHECK(!request.requested_video_device_ids.front().empty());
    ProcessChangeSourceRequest(web_contents, request, std::move(callback));
    return;
  }

  if (request.video_type ==
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB ||
      request.video_type ==
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE ||
      request.video_type ==
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET) {
    // Repeat the permission test from the render process.
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
        request.render_process_id, request.render_frame_id);
    if (!rfh) {
      std::move(callback).Run(
          blink::mojom::StreamDevicesSet(),
          blink::mojom::MediaStreamRequestResult::INVALID_STATE,
          /*ui=*/nullptr);
      return;
    }

    // If the display-capture permissions-policy disallows capture, the render
    // process was not supposed to send this message.
    if (!rfh->IsFeatureEnabled(
            blink::mojom::PermissionsPolicyFeature::kDisplayCapture)) {
      bad_message::ReceivedBadMessage(
          rfh->GetProcess(), bad_message::BadMessageReason::
                                 RFH_DISPLAY_CAPTURE_PERMISSION_MISSING);
      std::move(callback).Run(
          blink::mojom::StreamDevicesSet(),
          blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
          /*ui=*/nullptr);
      return;
    }

    // Renderer process should already check for transient user activation
    // before sending IPC, but just to be sure double check here as well. This
    // is not treated as a BadMessage because it is possible for the transient
    // user activation to expire between the renderer side check and this check.
    if (!rfh->HasTransientUserActivation() &&
        capture_policy::IsTransientActivationRequiredForGetDisplayMedia(
            web_contents)) {
      std::move(callback).Run(
          blink::mojom::StreamDevicesSet(),
          blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
          /*ui=*/nullptr);
      return;
    }
  }

  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          web_contents->GetBrowserContext());
  CHECK(content_settings);
  const GURL& origin_url = web_contents->GetLastCommittedURL();
  ContentSetting content_setting_value = content_settings->GetContentSetting(
      origin_url, origin_url, ContentSettingsType::DISPLAY_MEDIA_SYSTEM_AUDIO);
  if (content_setting_value == ContentSetting::CONTENT_SETTING_BLOCK) {
    // Except for the case when DISPLAY_MEDIA_SYSTEM_AUDIO is allowed, all
    // request should contain video stream.
    if (request.video_type == blink::mojom::MediaStreamType::NO_SERVICE) {
      std::move(callback).Run(
          blink::mojom::StreamDevicesSet(),
          blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
          /*ui=*/nullptr);
      return;
    }
    ShowMediaSelectionDialog(web_contents, request, std::move(callback));
  } else if (content_setting_value == ContentSetting::CONTENT_SETTING_ALLOW) {
    if (request.video_type != blink::mojom::MediaStreamType::NO_SERVICE) {
      ShowMediaSelectionDialog(web_contents, request, std::move(callback));
      return;
    }
    // To bypass the media selection dialog, the system audio must be included.
    if (request.exclude_system_audio) {
      std::move(callback).Run(
          blink::mojom::StreamDevicesSet(),
          blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
          /*ui=*/nullptr);
      return;
    }
    BypassMediaSelectionDialog(web_contents, request, std::move(callback));
  } else {
    NOTREACHED();
  }
}

void DisplayMediaAccessHandler::UpdateMediaRequestState(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::mojom::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state != content::MEDIA_REQUEST_STATE_DONE &&
      state != content::MEDIA_REQUEST_STATE_CLOSING) {
    return;
  }

  if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
    DeletePendingAccessRequest(render_process_id, render_frame_id,
                               page_request_id);
  }
  CaptureAccessHandlerBase::UpdateMediaRequestState(
      render_process_id, render_frame_id, page_request_id, stream_type, state);

  // This method only gets called with the above checked states when all
  // requests are to be canceled. Therefore, we don't need to process the
  // next queued request.
}

void DisplayMediaAccessHandler::ShowMediaSelectionDialog(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  std::unique_ptr<DesktopMediaPicker> picker =
      picker_factory_->CreatePicker(&request);
  if (!picker) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, /*ui=*/nullptr);
    return;
  }

  // Ensure we are observing the deletion of |web_contents|.
  web_contents_collection_.StartObserving(web_contents);

  RequestsQueue& queue = pending_requests_[web_contents];

  queue.push_back(std::make_unique<PendingAccessRequest>(
      std::move(picker), request, std::move(callback),
      GetApplicationTitle(web_contents), display_notification_,
      /*is_allowlisted_extension=*/false));
  // If this is the only request then pop picker UI.
  if (queue.size() == 1) {
    ProcessQueuedAccessRequest(queue, web_contents);
  }
}

void DisplayMediaAccessHandler::BypassMediaSelectionDialog(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  CHECK_EQ(web_contents->GetLastCommittedURL().scheme(),
           content::kChromeUIScheme);

  content::DesktopMediaID media_id(content::DesktopMediaID::TYPE_SCREEN,
                                   content::DesktopMediaID::kNullId,
                                   /*audio_share=*/true);
  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& stream_devices =
      *stream_devices_set.stream_devices[0];
  std::unique_ptr<content::MediaStreamUI> ui = GetDevicesForDesktopCapture(
      request, web_contents, media_id, media_id.audio_share,
      request.disable_local_echo, request.suppress_local_audio_playback,
      /*display_notification=*/false, GetApplicationTitle(web_contents),
      request.captured_surface_control_active, stream_devices);
  std::move(callback).Run(stream_devices_set,
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));
}

void DisplayMediaAccessHandler::ProcessChangeSourceRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  // Ensure we are observing the deletion of |web_contents|.
  web_contents_collection_.StartObserving(web_contents);

  RequestsQueue& queue = pending_requests_[web_contents];
  queue.push_back(std::make_unique<PendingAccessRequest>(
      /*picker=*/nullptr, request, std::move(callback),
      GetApplicationTitle(web_contents), display_notification_,
      /*is_allowlisted_extension=*/false));
  // If this is the only request then pop it. Otherwise, there is already a task
  // scheduled to pop the next request.
  if (queue.size() == 1)
    ProcessQueuedAccessRequest(queue, web_contents);
}

void DisplayMediaAccessHandler::ProcessQueuedAccessRequest(
    const RequestsQueue& queue,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  const PendingAccessRequest& pending_request = *queue.front();
  UpdateTrusted(pending_request.request, false /* is_trusted */);

  const GURL& request_origin = pending_request.request.security_origin;
  AllowedScreenCaptureLevel capture_level =
      capture_policy::GetAllowedCaptureLevel(request_origin, web_contents);

  // If Capture is not allowed, then reject.
  if (capture_level == AllowedScreenCaptureLevel::kDisallowed) {
    RejectRequest(web_contents,
                  blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED);
    return;
  }

  if (pending_request.request.request_type == blink::MEDIA_DEVICE_UPDATE) {
    ProcessQueuedChangeSourceRequest(pending_request.request, web_contents);
  } else {
    ProcessQueuedPickerRequest(pending_request, web_contents, capture_level,
                               request_origin);
  }
}

void DisplayMediaAccessHandler::ProcessQueuedPickerRequest(
    const PendingAccessRequest& pending_request,
    content::WebContents* web_contents,
    AllowedScreenCaptureLevel capture_level,
    const GURL& request_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  content::WebContents* ui_web_contents = web_contents;

  // If `web_contents` is the opener of a Document Picture in Picture window,
  // and if the pip window currently has the focus, then show the request in the
  // pip window instead.
  if (base::FeatureList::IsEnabled(kDisplayCaptureUiInPipIfActive)) {
    if (content::WebContents* const child_web_contents =
            web_contents->HasPictureInPictureDocument()
                ? PictureInPictureWindowManager::GetInstance()
                      ->GetChildWebContents()
                : nullptr) {
      // There should not be more than one pip window.  If `web_contents`
      // believes that it is a document pip opener, then make sure that the
      // window manager agrees with it.
      CHECK_EQ(PictureInPictureWindowManager::GetInstance()->GetWebContents(),
               web_contents);

      // The media-picker prompt will be associated with the PiP window if the
      // user's last interaction was with the PiP. (This heuristic could in the
      // future be replaced with an explicit control surface exposed to the
      // app.)
      //
      // Note that `RenderWidgetHostView::HasFocus()` does not work as expected
      // on Mac; it always returns true.  The Widget's activation state is what
      // tracks the state we care about.  It's not 100% accurate either as a
      // proxy for "the user's last interaction", but it's good enough.
      if (gfx::NativeWindow native_window =
              child_web_contents->GetTopLevelNativeWindow()) {
        if (auto* browser_view =
                BrowserView::GetBrowserViewForNativeWindow(native_window)) {
          if (browser_view->frame()->IsActive()) {
            ui_web_contents = child_web_contents;
          }
        }
      }
    }
  }

  std::vector<DesktopMediaList::Type> media_types{
      DesktopMediaList::Type::kWebContents, DesktopMediaList::Type::kWindow};
  if (!pending_request.request.exclude_monitor_type_surfaces) {
    media_types.push_back(DesktopMediaList::Type::kScreen);
  }
  if (pending_request.request.video_type ==
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB) {
    media_types.insert(media_types.begin(),
                       DesktopMediaList::Type::kCurrentTab);
  }

  capture_policy::FilterMediaList(media_types, capture_level);

  auto includable_web_contents_filter =
      capture_policy::GetIncludableWebContentsFilter(request_origin,
                                                     capture_level);
  if (pending_request.request.exclude_self_browser_surface) {
    includable_web_contents_filter = DesktopMediaList::ExcludeWebContents(
        std::move(includable_web_contents_filter), web_contents);
  }

  auto source_lists = picker_factory_->CreateMediaList(
      media_types, web_contents, includable_web_contents_filter);

  // base::Unretained(this) is safe because DisplayMediaAccessHandler is owned
  // by MediaCaptureDevicesDispatcher, which is a lazy singleton which is
  // destroyed when the browser process terminates.
  DesktopMediaPicker::DoneCallback done_callback =
      base::BindOnce(&DisplayMediaAccessHandler::OnDisplaySurfaceSelected,
                     base::Unretained(this), web_contents->GetWeakPtr());
  DesktopMediaPicker::Params picker_params(
      DesktopMediaPicker::Params::RequestSource::kGetDisplayMedia);
  picker_params.web_contents = ui_web_contents;
  gfx::NativeWindow parent_window = ui_web_contents->GetTopLevelNativeWindow();
  picker_params.context = parent_window;
  picker_params.parent = parent_window;
  picker_params.app_name = GetApplicationTitle(web_contents);
  picker_params.target_name = picker_params.app_name;
  picker_params.request_audio =
      pending_request.request.audio_type ==
      blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE;
  picker_params.exclude_system_audio =
      pending_request.request.exclude_system_audio;
  picker_params.suppress_local_audio_playback =
      pending_request.request.suppress_local_audio_playback;
  picker_params.restricted_by_policy =
      (capture_level != AllowedScreenCaptureLevel::kUnrestricted);
  picker_params.preferred_display_surface =
      pending_request.request.preferred_display_surface;
  pending_request.picker->Show(picker_params, std::move(source_lists),
                               std::move(done_callback));
}

void DisplayMediaAccessHandler::ProcessQueuedChangeSourceRequest(
    const content::MediaStreamRequest& request,
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  DCHECK(!request.requested_video_device_ids.empty());

  content::WebContentsMediaCaptureId web_contents_id;
  if (!content::WebContentsMediaCaptureId::Parse(
          request.requested_video_device_ids.front(), &web_contents_id)) {
    RejectRequest(web_contents,
                  blink::mojom::MediaStreamRequestResult::INVALID_STATE);
    return;
  }
  content::DesktopMediaID media_id(content::DesktopMediaID::TYPE_WEB_CONTENTS,
                                   content::DesktopMediaID::kNullId,
                                   web_contents_id);
  media_id.audio_share =
      request.audio_type != blink::mojom::MediaStreamType::NO_SERVICE;
  OnDisplaySurfaceSelected(web_contents->GetWeakPtr(), media_id);
}

void DisplayMediaAccessHandler::RejectRequest(
    content::WebContents* web_contents,
    blink::mojom::MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  auto it = pending_requests_.find(web_contents);
  if (it == pending_requests_.end())
    return;
  RequestsQueue& mutable_queue = it->second;
  if (mutable_queue.empty())
    return;
  PendingAccessRequest& mutable_request = *mutable_queue.front();
  std::move(mutable_request.callback)
      .Run(blink::mojom::StreamDevicesSet(), result, /*ui=*/nullptr);
  mutable_queue.pop_front();
  if (!mutable_queue.empty())
    ProcessQueuedAccessRequest(mutable_queue, web_contents);
}

void DisplayMediaAccessHandler::AcceptRequest(
    content::WebContents* web_contents,
    const content::DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  auto it = pending_requests_.find(web_contents);
  if (it == pending_requests_.end())
    return;
  RequestsQueue& queue = it->second;
  if (queue.empty()) {
    // UpdateMediaRequestState() called with MEDIA_REQUEST_STATE_CLOSING. Don't
    // need to do anything.
    return;
  }
  PendingAccessRequest& pending_request = *queue.front();

  const bool disable_local_echo =
      (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) &&
      media_id.web_contents_id.disable_local_echo;

  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& stream_devices =
      *stream_devices_set.stream_devices[0];
  std::unique_ptr<content::MediaStreamUI> ui = GetDevicesForDesktopCapture(
      pending_request.request, web_contents, media_id, media_id.audio_share,
      disable_local_echo, pending_request.request.suppress_local_audio_playback,
      display_notification_, GetApplicationTitle(web_contents),
      pending_request.request.captured_surface_control_active, stream_devices);
  UpdateTarget(pending_request.request, media_id);

  std::move(pending_request.callback)
      .Run(stream_devices_set, blink::mojom::MediaStreamRequestResult::OK,
           std::move(ui));
  queue.pop_front();

  if (!queue.empty())
    ProcessQueuedAccessRequest(queue, web_contents);
}

void DisplayMediaAccessHandler::OnDisplaySurfaceSelected(
    base::WeakPtr<content::WebContents> web_contents,
    content::DesktopMediaID media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents) {
    // WebContentsDestroyed() will evict the entry from `pending_requests_`,
    // which was associated with `web_contents` before it got null-out.
    return;
  }

  if (media_id.is_null()) {
    RejectRequest(web_contents.get(),
                  blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED);
    return;
  }

  // If the media id is tied to a tab, check that it hasn't been destroyed.
  if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS &&
      !content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(
              media_id.web_contents_id.render_process_id,
              media_id.web_contents_id.main_render_frame_id))) {
    RejectRequest(web_contents.get(),
                  blink::mojom::MediaStreamRequestResult::TAB_CAPTURE_FAILURE);
    return;
  }

#if BUILDFLAG(IS_MAC)
  // Check screen capture permissions on Mac if necessary.
  // Do not check screen capture permissions when window_id is populated. The
  // presence of the window_id indicates the window to be captured is a Chromium
  // window which will be captured internally, the macOS screen capture APIs
  // will not be used.
  if (system_media_permissions::ScreenCaptureNeedsSystemLevelPermissions() &&
      (media_id.type == content::DesktopMediaID::TYPE_SCREEN ||
       (media_id.type == content::DesktopMediaID::TYPE_WINDOW &&
        !media_id.window_id)) &&
      system_media_permissions::CheckSystemScreenCapturePermission() !=
          system_permission_settings::SystemPermission::kAllowed) {
    RejectRequest(
        web_contents.get(),
        blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED);
    return;
  }
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
  // Check Data Leak Prevention restrictions on Chrome.
  // base::Unretained(this) is safe because DisplayMediaAccessHandler is owned
  // by MediaCaptureDevicesDispatcher, which is a lazy singleton which is
  // destroyed when the browser process terminates.
  policy::DlpContentManager::Get()->CheckScreenShareRestriction(
      media_id, GetApplicationTitle(web_contents.get()),
      base::BindOnce(&DisplayMediaAccessHandler::OnDlpRestrictionChecked,
                     base::Unretained(this), web_contents, media_id));
#else   // BUILDFLAG(IS_CHROMEOS)
  AcceptRequest(web_contents.get(), media_id);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
void DisplayMediaAccessHandler::OnDlpRestrictionChecked(
    base::WeakPtr<content::WebContents> web_contents,
    const content::DesktopMediaID& media_id,
    bool is_dlp_allowed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents) {
    return;
  }

  if (!is_dlp_allowed) {
    RejectRequest(web_contents.get(),
                  blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED);
  }
  AcceptRequest(web_contents.get(), media_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void DisplayMediaAccessHandler::DeletePendingAccessRequest(
    int render_process_id,
    int render_frame_id,
    int page_request_id) {
  for (auto& queue_it : pending_requests_) {
    RequestsQueue& queue = queue_it.second;
    for (auto it = queue.begin(); it != queue.end(); ++it) {
      const PendingAccessRequest& pending_request = **it;
      if (pending_request.request.render_process_id == render_process_id &&
          pending_request.request.render_frame_id == render_frame_id &&
          pending_request.request.page_request_id == page_request_id) {
        queue.erase(it);
        return;
      }
    }
  }
}

void DisplayMediaAccessHandler::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pending_requests_.erase(web_contents);
}
