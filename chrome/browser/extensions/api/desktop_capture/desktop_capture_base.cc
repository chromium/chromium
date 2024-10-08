// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_base.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory_impl.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/native_desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/branded_strings.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/manifest.h"
#include "extensions/common/switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

using content::DesktopMediaID;
using extensions::api::desktop_capture::ChooseDesktopMedia::Results::Options;

namespace extensions {

namespace {

const char kTargetNotActiveError[] = "The specified target is not active.";
const char kInvalidSourceNameError[] = "Invalid source type specified.";

DesktopMediaPickerFactory* g_picker_factory = nullptr;

}  // namespace

// static
void DesktopCaptureChooseDesktopMediaFunctionBase::SetPickerFactoryForTests(
    DesktopMediaPickerFactory* factory) {
  g_picker_factory = factory;
}

const char
    DesktopCaptureChooseDesktopMediaFunctionBase::kTargetNotFoundError[] =
        "The specified target is not found.";

DesktopCaptureChooseDesktopMediaFunctionBase::
    DesktopCaptureChooseDesktopMediaFunctionBase() = default;

DesktopCaptureChooseDesktopMediaFunctionBase::
    ~DesktopCaptureChooseDesktopMediaFunctionBase() {
  DesktopCaptureRequestsRegistry::GetInstance()->RemoveRequest(
      source_process_id(), request_id_);
}

void DesktopCaptureChooseDesktopMediaFunctionBase::Cancel() {
  // Keep reference to |this| to ensure the object doesn't get destroyed before
  // we return.
  scoped_refptr<DesktopCaptureChooseDesktopMediaFunctionBase> self(this);

  // If this picker dialog is open, this will close it.
  picker_controller_.reset();

  Respond(ArgumentList(Create(std::string(), Options())));
}

ExtensionFunction::ResponseAction
DesktopCaptureChooseDesktopMediaFunctionBase::Execute(
    const std::vector<api::desktop_capture::DesktopCaptureSourceType>& sources,
    bool exclude_system_audio,
    bool exclude_self_browser_surface,
    bool suppress_local_audio_playback_intended,
    content::RenderFrameHost* render_frame_host,
    const GURL& origin,
    const std::u16string& target_name) {
  DCHECK(!picker_controller_);

  if (!render_frame_host->IsActive())
    return RespondNow(Error(kTargetNotActiveError));

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return RespondNow(Error(kTargetNotFoundError));

  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();
  // In case of coming from background extension page, |parent_window| will
  // be null. We are going to make the picker modal to the current browser
  // window.
  if (!parent_window) {
    Browser* target_browser = chrome::FindLastActiveWithProfile(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()));

    if (target_browser)
      parent_window = target_browser->window()->GetNativeWindow();
  }

  bool request_audio = false;
  std::vector<DesktopMediaList::Type> media_types;
  for (auto source_type : sources) {
    switch (source_type) {
      case api::desktop_capture::DesktopCaptureSourceType::kNone: {
        return RespondNow(Error(kInvalidSourceNameError));
      }
      case api::desktop_capture::DesktopCaptureSourceType::kScreen: {
        media_types.push_back(DesktopMediaList::Type::kScreen);
        break;
      }
      case api::desktop_capture::DesktopCaptureSourceType::kWindow: {
        media_types.push_back(DesktopMediaList::Type::kWindow);
        break;
      }
      case api::desktop_capture::DesktopCaptureSourceType::kTab: {
        media_types.push_back(DesktopMediaList::Type::kWebContents);
        break;
      }
      case api::desktop_capture::DesktopCaptureSourceType::kAudio: {
        request_audio = true;
        break;
      }
    }
  }

  AllowedScreenCaptureLevel capture_level =
      capture_policy::GetAllowedCaptureLevel(origin, web_contents);

  capture_policy::FilterMediaList(media_types, capture_level);

  DesktopMediaList::WebContentsFilter includable_web_contents_filter =
      capture_policy::GetIncludableWebContentsFilter(origin, capture_level);
  if (exclude_self_browser_surface) {
    includable_web_contents_filter = DesktopMediaList::ExcludeWebContents(
        std::move(includable_web_contents_filter), web_contents);
  }

  DesktopMediaPickerController::DoneCallback callback = base::BindOnce(
      &DesktopCaptureChooseDesktopMediaFunctionBase::OnPickerDialogResults,
      this, origin, render_frame_host->GetGlobalId());
  DesktopMediaPickerController::Params picker_params(
      DesktopMediaPickerController::Params::RequestSource::kExtension);
  picker_params.web_contents = web_contents;
  picker_params.context = parent_window;
  picker_params.parent = parent_window;
  picker_params.app_name = base::UTF8ToUTF16(GetCallerDisplayName());
  picker_params.target_name = target_name;
  picker_params.request_audio = request_audio;
  picker_params.exclude_system_audio = exclude_system_audio;
  picker_params.suppress_local_audio_playback =
      suppress_local_audio_playback_intended;
  picker_controller_ =
      std::make_unique<DesktopMediaPickerController>(g_picker_factory);
  picker_params.restricted_by_policy =
      (capture_level != AllowedScreenCaptureLevel::kUnrestricted);
  picker_controller_->Show(picker_params, std::move(media_types),
                           includable_web_contents_filter, std::move(callback));
  return RespondLater();
}

std::string DesktopCaptureChooseDesktopMediaFunctionBase::GetCallerDisplayName()
    const {
  if (extension()->location() == mojom::ManifestLocation::kComponent ||
      extension()->location() == mojom::ManifestLocation::kExternalComponent) {
    return l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME);
  } else {
    return extension()->name();
  }
}

void DesktopCaptureChooseDesktopMediaFunctionBase::OnPickerDialogResults(
    const GURL& origin,
    const content::GlobalRenderFrameHostId& render_frame_host_id,
    const std::string& err,
    DesktopMediaID source) {
  picker_controller_.reset();

  if (!err.empty()) {
    Respond(Error(err));
    return;
  }

  if (source.is_null()) {
    DLOG(ERROR) << "Sending empty results.";
    Respond(ArgumentList(Create(std::string(), Options())));
    return;
  }

  std::string result;
  if (source.type != DesktopMediaID::TYPE_NONE) {
    result = content::DesktopStreamsRegistry::GetInstance()->RegisterStream(
        render_frame_host_id.child_id, render_frame_host_id.frame_routing_id,
        url::Origin::Create(origin), source,
        content::kRegistryStreamTypeDesktop);
  }

  Options options;
  options.can_request_audio_track = source.audio_share;
  Respond(ArgumentList(Create(result, options)));
}

DesktopCaptureRequestsRegistry::RequestId::RequestId(int process_id,
                                                     int request_id)
    : process_id(process_id), request_id(request_id) {}

bool DesktopCaptureRequestsRegistry::RequestId::operator<(
    const RequestId& other) const {
  return std::tie(process_id, request_id) <
         std::tie(other.process_id, other.request_id);
}

DesktopCaptureCancelChooseDesktopMediaFunctionBase::
    DesktopCaptureCancelChooseDesktopMediaFunctionBase() {}

DesktopCaptureCancelChooseDesktopMediaFunctionBase::
    ~DesktopCaptureCancelChooseDesktopMediaFunctionBase() {}

ExtensionFunction::ResponseAction
DesktopCaptureCancelChooseDesktopMediaFunctionBase::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_int());
  int request_id = args()[0].GetInt();

  DesktopCaptureRequestsRegistry::GetInstance()->CancelRequest(
      source_process_id(), request_id);
  return RespondNow(NoArguments());
}

DesktopCaptureRequestsRegistry::DesktopCaptureRequestsRegistry() {}
DesktopCaptureRequestsRegistry::~DesktopCaptureRequestsRegistry() {}

// static
DesktopCaptureRequestsRegistry* DesktopCaptureRequestsRegistry::GetInstance() {
  return base::Singleton<DesktopCaptureRequestsRegistry>::get();
}

void DesktopCaptureRequestsRegistry::AddRequest(
    int process_id,
    int request_id,
    DesktopCaptureChooseDesktopMediaFunctionBase* handler) {
  requests_.insert(
      RequestsMap::value_type(RequestId(process_id, request_id), handler));
}

void DesktopCaptureRequestsRegistry::RemoveRequest(int process_id,
                                                   int request_id) {
  requests_.erase(RequestId(process_id, request_id));
}

void DesktopCaptureRequestsRegistry::CancelRequest(int process_id,
                                                   int request_id) {
  auto it = requests_.find(RequestId(process_id, request_id));
  if (it != requests_.end())
    it->second->Cancel();
}

}  // namespace extensions
