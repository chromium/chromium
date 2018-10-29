// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_base.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/webrtc/desktop_media_list_ash.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory_impl.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/native_desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/chromium_strings.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/manifest.h"
#include "extensions/common/switches.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::api::desktop_capture::ChooseDesktopMedia::Results::Options;
using content::DesktopMediaID;

namespace extensions {

namespace {

const char kInvalidSourceNameError[] = "Invalid source type specified.";
const char kEmptySourcesListError[] =
    "At least one source type must be specified.";

DesktopMediaPickerFactory* g_picker_factory = nullptr;

}  // namespace

// static
void DesktopCaptureChooseDesktopMediaFunctionBase::SetPickerFactoryForTests(
    DesktopMediaPickerFactory* factory) {
  g_picker_factory = factory;
}

DesktopCaptureChooseDesktopMediaFunctionBase::
    DesktopCaptureChooseDesktopMediaFunctionBase() = default;

DesktopCaptureChooseDesktopMediaFunctionBase::
    ~DesktopCaptureChooseDesktopMediaFunctionBase() {
  // RenderFrameHost may be already destroyed.
  if (render_frame_host()) {
    DesktopCaptureRequestsRegistry::GetInstance()->RemoveRequest(
        render_frame_host()->GetProcess()->GetID(), request_id_);
  }
}

void DesktopCaptureChooseDesktopMediaFunctionBase::Cancel() {
  // Keep reference to |this| to ensure the object doesn't get destroyed before
  // we return.
  scoped_refptr<DesktopCaptureChooseDesktopMediaFunctionBase> self(this);
  if (picker_) {
    picker_.reset();
    SetResultList(Create(std::string(), Options()));
    SendResponse(true);
  }
}

bool DesktopCaptureChooseDesktopMediaFunctionBase::Execute(
    const std::vector<api::desktop_capture::DesktopCaptureSourceType>& sources,
    content::WebContents* web_contents,
    const GURL& origin,
    const base::string16 target_name) {
  // Register to be notified when the tab is closed.
  Observe(web_contents);

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
  std::vector<content::DesktopMediaID::Type> media_types;
  for (auto source_type : sources) {
    switch (source_type) {
      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_NONE: {
        error_ = kInvalidSourceNameError;
        return false;
      }
      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_SCREEN: {
        media_types.push_back(content::DesktopMediaID::TYPE_SCREEN);
        break;
      }
      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_WINDOW: {
        media_types.push_back(content::DesktopMediaID::TYPE_WINDOW);
        break;
      }
      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_TAB: {
        if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                extensions::switches::kDisableTabForDesktopShare)) {
          continue;
        }
        media_types.push_back(content::DesktopMediaID::TYPE_WEB_CONTENTS);
        break;
      }
      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_AUDIO: {
        if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
                extensions::switches::kDisableDesktopCaptureAudio)) {
          request_audio = true;
        }
        break;
      }
    }
  }

  DesktopMediaPickerFactory* picker_factory =
      g_picker_factory ? g_picker_factory
                       : DesktopMediaPickerFactoryImpl::GetInstance();
  // Keep same order as the input |sources| and avoid duplicates.
  std::vector<std::unique_ptr<DesktopMediaList>> source_lists =
      picker_factory->CreateMediaList(media_types);
  if (source_lists.empty()) {
    error_ = kEmptySourcesListError;
    return false;
  }
  picker_ = picker_factory->CreatePicker();
  if (!picker_) {
    error_ = "Desktop Capture API is not yet implemented for this platform.";
    return false;
  }

  DesktopMediaPicker::DoneCallback callback = base::Bind(
      &DesktopCaptureChooseDesktopMediaFunctionBase::OnPickerDialogResults,
      this);
  DesktopMediaPicker::Params picker_params;
  picker_params.web_contents = web_contents;
  picker_params.context = parent_window;
  picker_params.parent = parent_window;
  picker_params.app_name = base::UTF8ToUTF16(GetCallerDisplayName());
  picker_params.target_name = target_name;
  picker_params.request_audio = request_audio;
  picker_->Show(picker_params, std::move(source_lists), callback);
  origin_ = origin;
  return true;
}

std::string DesktopCaptureChooseDesktopMediaFunctionBase::GetCallerDisplayName()
    const {
  if (extension()->location() == Manifest::COMPONENT ||
      extension()->location() == Manifest::EXTERNAL_COMPONENT) {
    return l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME);
  } else {
    return extension()->name();
  }
}

void DesktopCaptureChooseDesktopMediaFunctionBase::WebContentsDestroyed() {
  Cancel();
}

void DesktopCaptureChooseDesktopMediaFunctionBase::OnPickerDialogResults(
    DesktopMediaID source) {
  std::string result;
  if (source.type != DesktopMediaID::TYPE_NONE && web_contents()) {
    // TODO(miu): Once render_frame_host() is being set, we should register the
    // exact RenderFrame requesting the stream, not the main RenderFrame.  With
    // that change, also update
    // MediaCaptureDevicesDispatcher::ProcessDesktopCaptureAccessRequest().
    // http://crbug.com/304341
    content::RenderFrameHost* const main_frame = web_contents()->GetMainFrame();
    result = content::DesktopStreamsRegistry::GetInstance()->RegisterStream(
        main_frame->GetProcess()->GetID(), main_frame->GetRoutingID(), origin_,
        source, extension()->name(), content::kRegistryStreamTypeDesktop);
  }

  Options options;
  options.can_request_audio_track = source.audio_share;
  results_ = Create(result, options);
  SendResponse(true);
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
  int request_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &request_id));

  DesktopCaptureRequestsRegistry::GetInstance()->CancelRequest(
      render_frame_host()->GetProcess()->GetID(), request_id);
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
