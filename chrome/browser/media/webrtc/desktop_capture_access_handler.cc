// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_capture_access_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/desktop_capture_devices_util.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory_impl.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/native_desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "ui/base/ui_base_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif  // BUILDFLAG(IS_MAC)

using content::BrowserThread;
using extensions::mojom::ManifestLocation;

namespace {

// Helper to get title of the calling application shown in the screen capture
// notification.
std::u16string GetApplicationTitle(content::WebContents* web_contents,
                                   const extensions::Extension* extension) {
  // Use extension name as title for extensions and host/origin for drive-by
  // web.
  if (extension)
    return base::UTF8ToUTF16(extension->name());

  return url_formatter::FormatOriginForSecurityDisplay(
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

// Returns whether an on-screen notification should appear after desktop capture
// is approved for |extension|.  Component extensions do not display a
// notification.
bool ShouldDisplayNotification(const extensions::Extension* extension) {
  return !(extension &&
           (extension->location() == ManifestLocation::kComponent ||
            extension->location() == ManifestLocation::kExternalComponent));
}

// Returns true if an on-screen notification should not be displayed after
// desktop capture is taken for the |url|.
bool HasNotificationExemption(const GURL& url) {
  return url.spec() == chrome::kChromeUIFeedbackURL;
}

#if !BUILDFLAG(IS_ANDROID)
// Find browser or app window from a given |web_contents|.
gfx::NativeWindow FindParentWindowForWebContents(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (browser && browser->window())
    return browser->window()->GetNativeWindow();

  const extensions::AppWindowRegistry::AppWindowList& window_list =
      extensions::AppWindowRegistry::Get(web_contents->GetBrowserContext())
          ->app_windows();
  for (extensions::AppWindow* app_window : window_list) {
    if (app_window->web_contents() == web_contents)
      return app_window->GetNativeWindow();
  }

  return nullptr;
}
#endif

bool IsMediaTypeAllowed(AllowedScreenCaptureLevel allowed_capture_level,
                        content::DesktopMediaID::Type media_type) {
  switch (media_type) {
    case content::DesktopMediaID::TYPE_NONE:
      NOTREACHED();
    case content::DesktopMediaID::TYPE_SCREEN:
      return allowed_capture_level >= AllowedScreenCaptureLevel::kDesktop;
    case content::DesktopMediaID::TYPE_WINDOW:
      return allowed_capture_level >= AllowedScreenCaptureLevel::kWindow;
    case content::DesktopMediaID::TYPE_WEB_CONTENTS:
      // SameOrigin is more restrictive than just tabs; so as long as at least
      // SameOrigin is allowed, then TYPE_WEB_CONTENTS can be included, and the
      // origins will be filtered for the SameOrigin requirement later.
      return allowed_capture_level >= AllowedScreenCaptureLevel::kSameOrigin;
  }
}

// Checks whether audio should be captured for the given |media_id| and
// |request|.
bool ShouldCaptureAudio(const content::DesktopMediaID& media_id,
                        const content::MediaStreamRequest& request) {
  // This value is essentially from the checkbox on picker window, so it
  // corresponds to user permission.
  const bool audio_permitted = media_id.audio_share;

  // This value is essentially from whether getUserMedia requests audio stream.
  const bool audio_requested =
      request.audio_type ==
      blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE;

  // This value shows for a given capture type, whether the system or our code
  // can support audio sharing. Currently audio is only supported for screen and
  // tab/webcontents capture streams.
  const bool audio_supported =
      (media_id.type == content::DesktopMediaID::TYPE_SCREEN &&
       DesktopMediaPickerController::IsSystemAudioCaptureSupported(
           DesktopMediaPicker::Params::RequestSource::kExtension)) ||
      media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS;

  return audio_permitted && audio_requested && audio_supported;
}

// Returns whether the request is approved or not. Some extensions do not
// require user approval, because they provide their own user approval UI. For
// others, shows a message box and asks for user approval.
bool IsRequestApproved(content::WebContents* web_contents,
                       const content::MediaStreamRequest& request,
                       const extensions::Extension* extension,
                       bool is_allowlisted_extension) {
  // Component extensions and some external extensions are approved by default.
  if (extension &&
      (extension->location() == ManifestLocation::kComponent ||
       extension->location() == ManifestLocation::kExternalComponent ||
       is_allowlisted_extension)) {
    return true;
  }

  // chrome://feedback/ is allowed by default.
  // The user can still decide whether the screenshot taken is shared or not.
  if (request.security_origin.spec() == chrome::kChromeUIFeedbackURL) {
    return true;
  }

#if !BUILDFLAG(IS_ANDROID)
  gfx::NativeWindow parent_window =
      FindParentWindowForWebContents(web_contents);
#else
  gfx::NativeWindow parent_window = gfx::NativeWindow();
#endif
  const std::u16string application_name = base::UTF8ToUTF16(
      extension ? extension->name() : request.security_origin.spec());
  const std::u16string confirmation_text = l10n_util::GetStringFUTF16(
      request.audio_type == blink::mojom::MediaStreamType::NO_SERVICE
          ? IDS_MEDIA_SCREEN_CAPTURE_CONFIRMATION_TEXT
          : IDS_MEDIA_SCREEN_AND_AUDIO_CAPTURE_CONFIRMATION_TEXT,
      application_name);
  const chrome::MessageBoxResult mb_result = chrome::ShowQuestionMessageBoxSync(
      parent_window,
      l10n_util::GetStringFUTF16(IDS_MEDIA_SCREEN_CAPTURE_CONFIRMATION_TITLE,
                                 application_name),
      confirmation_text);
  return mb_result == chrome::MESSAGE_BOX_RESULT_YES;
}

}  // namespace

DesktopCaptureAccessHandler::DesktopCaptureAccessHandler()
    : picker_factory_(new DesktopMediaPickerFactoryImpl()),
      display_notification_(true),
      web_contents_collection_(this) {}

DesktopCaptureAccessHandler::DesktopCaptureAccessHandler(
    std::unique_ptr<DesktopMediaPickerFactory> picker_factory)
    : picker_factory_(std::move(picker_factory)),
      display_notification_(false),
      web_contents_collection_(this) {}

DesktopCaptureAccessHandler::~DesktopCaptureAccessHandler() = default;

void DesktopCaptureAccessHandler::ProcessScreenCaptureAccessRequest(
    content::WebContents* web_contents,
    const extensions::Extension* extension,
    std::unique_ptr<PendingAccessRequest> pending_request) {
  DCHECK_EQ(pending_request->request.video_type,
            blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE);

  UpdateExtensionTrusted(pending_request->request,
                         pending_request->is_allowlisted_extension);

  const bool screen_capture_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableUserMediaScreenCapturing) ||
      pending_request->is_allowlisted_extension ||
      IsBuiltInFeedbackUI(pending_request->request.security_origin);

  const bool origin_is_secure =
      network::IsUrlPotentiallyTrustworthy(
          pending_request->request.security_origin) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowHttpScreenCapture);

  if (!screen_capture_enabled || !origin_is_secure) {
    std::move(pending_request->callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::INVALID_STATE,
             /*ui=*/nullptr);
    return;
  }

  if (!IsRequestApproved(web_contents, pending_request->request, extension,
                         pending_request->is_allowlisted_extension)) {
    std::move(pending_request->callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
             /*ui=*/nullptr);
    return;
  }

  if (!content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(
              pending_request->request.render_process_id,
              pending_request->request.render_frame_id))) {
    std::move(pending_request->callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::INVALID_STATE,
             /*ui=*/nullptr);
    return;
  }

  const bool capture_audio =
      pending_request->request.audio_type ==
          blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE &&
      DesktopMediaPickerController::IsSystemAudioCaptureSupported(
          DesktopMediaPicker::Params::RequestSource::kExtension);

#if BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const content::DesktopMediaID screen_id =
      content::DesktopMediaID::RegisterNativeWindow(
          content::DesktopMediaID::TYPE_SCREEN,
          primary_root_window_for_testing_
              ? primary_root_window_for_testing_.get()
              : ash::Shell::Get()->GetPrimaryRootWindow());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  const content::DesktopMediaID screen_id = content::DesktopMediaID(
      content::DesktopMediaID::TYPE_SCREEN, webrtc::kFullDesktopScreenId);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // base::Unretained(this) is safe because DesktopCaptureAccessHandler is owned
  // by MediaCaptureDevicesDispatcher, which is a lazy singleton which is
  // destroyed when the browser process terminates.
  policy::DlpContentManager::Get()->CheckScreenShareRestriction(
      screen_id, GetApplicationTitle(web_contents, extension),
      base::BindOnce(&DesktopCaptureAccessHandler::OnDlpRestrictionChecked,
                     base::Unretained(this), web_contents->GetWeakPtr(),
                     std::move(pending_request), screen_id, capture_audio));
  return;
#else   // BUILDFLAG(IS_CHROMEOS)
  const content::DesktopMediaID screen_id = content::DesktopMediaID(
      content::DesktopMediaID::TYPE_SCREEN, webrtc::kFullDesktopScreenId);
  AcceptRequest(web_contents, std::move(pending_request), screen_id,
                capture_audio);
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

bool DesktopCaptureAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return type == blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE ||
         type == blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE;
}

bool DesktopCaptureAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return false;
}

void DesktopCaptureAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const bool is_allowlisted_extension =
      IsExtensionAllowedForScreenCapture(extension);
  const bool should_display_notification =
      display_notification_ && ShouldDisplayNotification(extension) &&
      !HasNotificationExemption(request.security_origin);
  std::unique_ptr<PendingAccessRequest> pending_request =
      std::make_unique<PendingAccessRequest>(
          /*picker=*/nullptr, request, std::move(callback),
          GetApplicationTitle(web_contents, extension),
          should_display_notification, is_allowlisted_extension);

  if (request.video_type !=
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) {
    std::move(pending_request->callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::INVALID_STATE,
             /*ui=*/nullptr);
    return;
  }

  AllowedScreenCaptureLevel allowed_capture_level =
      capture_policy::GetAllowedCaptureLevel(request.security_origin,
                                             web_contents);

  if (allowed_capture_level == AllowedScreenCaptureLevel::kDisallowed) {
    std::move(pending_request->callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
             /*ui=*/nullptr);
    return;
  }

  if (request.request_type == blink::MEDIA_DEVICE_UPDATE) {
    ProcessChangeSourceRequest(web_contents, std::move(pending_request));
    return;
  }

  // If the device id wasn't specified then this is a screen capture request
  // (i.e. chooseDesktopMedia() API wasn't used to generate device id).
  if (request.requested_video_device_ids.empty() ||
      request.requested_video_device_ids.front().empty()) {
    if (allowed_capture_level < AllowedScreenCaptureLevel::kDesktop) {
      std::move(pending_request->callback)
          .Run(blink::mojom::StreamDevicesSet(),
               blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
               /*ui=*/nullptr);
      return;
    }
#if BUILDFLAG(IS_MAC)
    if (system_media_permissions::ScreenCaptureNeedsSystemLevelPermissions() &&
        system_media_permissions::CheckSystemScreenCapturePermission() !=
            system_permission_settings::SystemPermission::kAllowed) {
      std::move(pending_request->callback)
          .Run(blink::mojom::StreamDevicesSet(),
               blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED,
               /*ui=*/nullptr);
      return;
    }
#endif
    ProcessScreenCaptureAccessRequest(web_contents, extension,
                                      std::move(pending_request));
    return;
  }

  // Resolve DesktopMediaID for the specified device id.
  content::DesktopMediaID media_id;
  // TODO(http://crbug.com/304341): Replace "main RenderFrame" IDs with the
  // request's actual RenderFrame IDs once the desktop capture extension API
  // implementation is fixed.
  content::WebContents* const web_contents_for_stream =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(request.render_process_id,
                                           request.render_frame_id));
  content::RenderFrameHost* const main_frame =
      web_contents_for_stream ? web_contents_for_stream->GetPrimaryMainFrame()
                              : nullptr;
  if (main_frame) {
    // This function would have already returned if this vector was empty.
    CHECK(!request.requested_video_device_ids.empty());
    media_id =
        content::DesktopStreamsRegistry::GetInstance()->RequestMediaForStreamId(
            request.requested_video_device_ids.front(),
            main_frame->GetProcess()->GetID(), main_frame->GetRoutingID(),
            url::Origin::Create(request.security_origin),
            content::kRegistryStreamTypeDesktop);
  }

  // Received invalid device id.
  if (media_id.type == content::DesktopMediaID::TYPE_NONE) {
    std::move(pending_request->callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::INVALID_STATE,
             /*ui=*/nullptr);
    return;
  }

  if (!IsMediaTypeAllowed(allowed_capture_level, media_id.type)) {
    std::move(pending_request->callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
             /*ui=*/nullptr);
    return;
  }
#if BUILDFLAG(IS_MAC)
  if (media_id.type != content::DesktopMediaID::TYPE_WEB_CONTENTS &&
      system_media_permissions::CheckSystemScreenCapturePermission() !=
          system_permission_settings::SystemPermission::kAllowed) {
    std::move(pending_request->callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED,
             /*ui=*/nullptr);
    return;
  }
#endif

  if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS &&
      !content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(
              media_id.web_contents_id.render_process_id,
              media_id.web_contents_id.main_render_frame_id))) {
    std::move(pending_request->callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::TAB_CAPTURE_FAILURE,
             /*ui=*/nullptr);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  const std::u16string application_title = pending_request->application_title;
  // base::Unretained(this) is safe because DesktopCaptureAccessHandler is owned
  // by MediaCaptureDevicesDispatcher, which is a lazy singleton which is
  // destroyed when the browser process terminates.
  policy::DlpContentManager::Get()->CheckScreenShareRestriction(
      media_id, application_title,
      base::BindOnce(&DesktopCaptureAccessHandler::OnDlpRestrictionChecked,
                     base::Unretained(this), web_contents->GetWeakPtr(),
                     std::move(pending_request), media_id,
                     ShouldCaptureAudio(media_id, request)));
#else  // BUILDFLAG(IS_CHROMEOS)
  AcceptRequest(web_contents, std::move(pending_request), media_id,
                ShouldCaptureAudio(media_id, request));

#endif  // !BUILDFLAG(IS_CHROMEOS)
}

void DesktopCaptureAccessHandler::ProcessChangeSourceRequest(
    content::WebContents* web_contents,
    std::unique_ptr<PendingAccessRequest> pending_request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(pending_request->request.video_type,
            blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE);

  if (pending_request->request.requested_video_device_ids.empty() ||
      pending_request->request.requested_video_device_ids.front().empty()) {
    // Passing nullptr selects the default picker (DesktopMediaPickerViews).
    pending_request->picker = picker_factory_->CreatePicker(nullptr);
    if (!pending_request->picker) {
      std::move(pending_request->callback)
          .Run(blink::mojom::StreamDevicesSet(),
               blink::mojom::MediaStreamRequestResult::INVALID_STATE,
               /*ui=*/nullptr);
      return;
    }
  }

  // Ensure we are observing the deletion of |web_contents|.
  web_contents_collection_.StartObserving(web_contents);

  RequestsQueue& queue = pending_requests_[web_contents];
  queue.push_back(std::move(pending_request));
  // If this is the only request then pop picker UI.
  if (queue.size() == 1)
    ProcessQueuedAccessRequest(queue, web_contents);
}

void DesktopCaptureAccessHandler::UpdateMediaRequestState(
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

void DesktopCaptureAccessHandler::ProcessQueuedAccessRequest(
    const RequestsQueue& queue,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const PendingAccessRequest& pending_request = *queue.front();

  if (!pending_request.picker) {
    DCHECK(!pending_request.request.requested_video_device_ids.empty());
    content::WebContentsMediaCaptureId web_contents_id;
    if (content::WebContentsMediaCaptureId::Parse(
            pending_request.request.requested_video_device_ids.front(),
            &web_contents_id)) {
      content::DesktopMediaID media_id(
          content::DesktopMediaID::TYPE_WEB_CONTENTS,
          content::DesktopMediaID::kNullId, web_contents_id);
      media_id.audio_share = pending_request.request.audio_type !=
                             blink::mojom::MediaStreamType::NO_SERVICE;
      OnPickerDialogResults(web_contents->GetWeakPtr(),
                            pending_request.application_title, media_id);
      return;
    }
  }

  const GURL& request_origin = pending_request.request.security_origin;
  AllowedScreenCaptureLevel capture_level =
      capture_policy::GetAllowedCaptureLevel(request_origin, web_contents);
  auto includable_web_contents_filter =
      capture_policy::GetIncludableWebContentsFilter(request_origin,
                                                     capture_level);

  auto source_lists = picker_factory_->CreateMediaList(
      {DesktopMediaList::Type::kWebContents}, web_contents,
      std::move(includable_web_contents_filter));

  // base::Unretained(this) is safe because DesktopCaptureAccessHandler is owned
  // by MediaCaptureDevicesDispatcher, which is a lazy singleton which is
  // destroyed when the browser process terminates.
  DesktopMediaPicker::DoneCallback done_callback =
      base::BindOnce(&DesktopCaptureAccessHandler::OnPickerDialogResults,
                     base::Unretained(this), web_contents->GetWeakPtr(),
                     pending_request.application_title);
  DesktopMediaPicker::Params picker_params(
      DesktopMediaPicker::Params::RequestSource::kExtension);
  picker_params.web_contents = web_contents;
  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();
  picker_params.context = parent_window;
  picker_params.parent = parent_window;
  picker_params.app_name = pending_request.application_title;
  picker_params.target_name = picker_params.app_name;
  picker_params.request_audio = pending_request.request.audio_type !=
                                blink::mojom::MediaStreamType::NO_SERVICE;
  picker_params.exclude_system_audio =
      pending_request.request.exclude_system_audio;
  picker_params.restricted_by_policy =
      (capture_level != AllowedScreenCaptureLevel::kUnrestricted);
  pending_request.picker->Show(picker_params, std::move(source_lists),
                               std::move(done_callback));

  // Focus on the tab with the picker for easy access.
  if (auto* delegate = web_contents->GetDelegate())
    delegate->ActivateContents(web_contents);
}

void DesktopCaptureAccessHandler::OnPickerDialogResults(
    base::WeakPtr<content::WebContents> web_contents,
    const std::u16string& application_title,
    content::DesktopMediaID media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents) {
    // If `pending_requests_` contained the old value of `pending_requests_`
    // before it got nulled out, then WebContentsDestroyed() will be
    // called with the value and evict the request from `pending_requests_`.
    return;
  }

  auto it = pending_requests_.find(web_contents.get());
  if (it == pending_requests_.end())
    return;
  RequestsQueue& queue = it->second;
  if (queue.empty()) {
    // UpdateMediaRequestState() called with MEDIA_REQUEST_STATE_CLOSING. Don't
    // need to do anything.
    return;
  }

  std::unique_ptr<PendingAccessRequest> pending_request =
      std::move(queue.front());
  queue.pop_front();

  if (media_id.is_null()) {
    std::move(pending_request->callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
             /*ui=*/nullptr);
  } else {
#if BUILDFLAG(IS_CHROMEOS)
    // base::Unretained(this) is safe because DesktopCaptureAccessHandler is
    // owned by MediaCaptureDevicesDispatcher, which is a lazy singleton which
    // is destroyed when the browser process terminates.
    policy::DlpContentManager::Get()->CheckScreenShareRestriction(
        media_id, application_title,
        base::BindOnce(&DesktopCaptureAccessHandler::OnDlpRestrictionChecked,
                       base::Unretained(this), web_contents,
                       std::move(pending_request), media_id,
                       media_id.audio_share));
#else   // BUILDFLAG(IS_CHROMEOS)
    AcceptRequest(web_contents.get(), std::move(pending_request), media_id,
                  media_id.audio_share);
#endif  // !BUILDFLAG(IS_CHROMEOS)
  }
  if (!queue.empty())
    ProcessQueuedAccessRequest(queue, web_contents.get());
}

void DesktopCaptureAccessHandler::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pending_requests_.erase(web_contents);
}

void DesktopCaptureAccessHandler::DeletePendingAccessRequest(
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

void DesktopCaptureAccessHandler::AcceptRequest(
    content::WebContents* web_contents,
    std::unique_ptr<PendingAccessRequest> pending_request,
    const content::DesktopMediaID& media_id,
    bool capture_audio) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  // TODO(crbug.com/40216442): Generalize to multiple streams.
  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& stream_devices =
      *stream_devices_set.stream_devices[0];
  std::unique_ptr<content::MediaStreamUI> ui = GetDevicesForDesktopCapture(
      pending_request->request, web_contents, media_id, capture_audio,
      pending_request->request.disable_local_echo,
      // TODO(crbug.com/40244027): Support suppressLocalAudioPlayback for the
      // extension API as well. If this happens as a result of merging
      // DesktopCaptureAccessHandler and DisplayMediaAccessHandler, that's fine.
      /*suppress_local_audio_playback=*/false,
      pending_request->should_display_notification,
      pending_request->application_title,
      pending_request->request.captured_surface_control_active, stream_devices);
  DCHECK(stream_devices.audio_device.has_value() ||
         stream_devices.video_device.has_value());

  UpdateExtensionTrusted(pending_request->request,
                         pending_request->is_allowlisted_extension);

  std::move(pending_request->callback)
      .Run(stream_devices_set, blink::mojom::MediaStreamRequestResult::OK,
           std::move(ui));
}

#if BUILDFLAG(IS_CHROMEOS)
void DesktopCaptureAccessHandler::OnDlpRestrictionChecked(
    base::WeakPtr<content::WebContents> web_contents,
    std::unique_ptr<PendingAccessRequest> pending_request,
    const content::DesktopMediaID& media_id,
    bool capture_audio,
    bool is_dlp_allowed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents) {
    // No need to do anything since WebContents is already destroyed by the time
    // this is invoked.
    return;
  }

  if (!is_dlp_allowed) {
    std::move(pending_request->callback)
        .Run(blink::mojom::StreamDevicesSet(),
             blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
             /*ui=*/nullptr);
    return;
  }

  AcceptRequest(web_contents.get(), std::move(pending_request), media_id,
                capture_audio);
}
#endif  // BUILDFLAG(IS_CHROMEOS)
