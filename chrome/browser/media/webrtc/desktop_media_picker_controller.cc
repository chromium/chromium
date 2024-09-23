// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory_impl.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/native_desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/branded_strings.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "desktop_media_picker.h"
#include "extensions/common/manifest.h"
#include "extensions/common/switches.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"

DesktopMediaPickerController::DesktopMediaPickerController(
    DesktopMediaPickerFactory* picker_factory)
    : picker_factory_(picker_factory
                          ? picker_factory
                          : DesktopMediaPickerFactoryImpl::GetInstance()) {}

DesktopMediaPickerController::~DesktopMediaPickerController() = default;

void DesktopMediaPickerController::Show(
    const Params& params,
    const std::vector<DesktopMediaList::Type>& sources,
    DesktopMediaList::WebContentsFilter includable_web_contents_filter,
    DoneCallback done_callback) {
  DCHECK(!base::Contains(sources, DesktopMediaList::Type::kNone));
  DCHECK(!done_callback_);

  done_callback_ = std::move(done_callback);
  params_ = params;

  Observe(params.web_contents);

  // Keep same order as the input |sources| and avoid duplicates.
  source_lists_ = picker_factory_->CreateMediaList(
      sources, params.web_contents, std::move(includable_web_contents_filter));
  if (source_lists_.empty()) {
    OnPickerDialogResults("At least one source type must be specified.", {});
    return;
  }

  if (params.select_only_screen && sources.size() == 1 &&
      sources[0] == DesktopMediaList::Type::kScreen) {
    // Try to bypass the picker dialog if possible.
    DCHECK(source_lists_.size() == 1);
    auto* source_list = source_lists_[0].get();
    source_list->Update(
        base::BindOnce(&DesktopMediaPickerController::OnInitialMediaListFound,
                       base::Unretained(this)));
  } else {
    ShowPickerDialog();
  }
}

void DesktopMediaPickerController::WebContentsDestroyed() {
  OnPickerDialogResults(std::string(), content::DesktopMediaID());
}

// static
bool DesktopMediaPickerController::IsSystemAudioCaptureSupported(
    Params::RequestSource request_source) {
  if (!media::IsSystemLoopbackCaptureSupported()) {
    return false;
  }
#if BUILDFLAG(IS_MAC)
 return request_source == Params::RequestSource::kCast ||
     base::FeatureList::IsEnabled(media::kMacLoopbackAudioForScreenShare);
#elif BUILDFLAG(IS_LINUX)
  if (request_source == Params::RequestSource::kCast) {
    return base::FeatureList::IsEnabled(media::kPulseaudioLoopbackForCast);
  } else {
    return base::FeatureList::IsEnabled(
        media::kPulseaudioLoopbackForScreenShare);
  }
#else
  return true;
#endif  // BUILDFLAG(IS_MAC)
}

void DesktopMediaPickerController::OnInitialMediaListFound() {
  DCHECK(params_.select_only_screen);
  DCHECK(source_lists_.size() == 1);
  auto* source_list = source_lists_[0].get();
  if (source_list->GetSourceCount() == 1) {
    // With only one possible source, the picker dialog is being bypassed. Apply
    // the default value of the "audio checkbox" here for desktop screen share.
    content::DesktopMediaID media_id = source_list->GetSource(0).id;
    DCHECK_EQ(media_id.type, content::DesktopMediaID::TYPE_SCREEN);
    media_id.audio_share =
        params_.request_audio &&
        IsSystemAudioCaptureSupported(params_.request_source);
    OnPickerDialogResults({}, media_id);
    return;
  }

  ShowPickerDialog();
}

void DesktopMediaPickerController::ShowPickerDialog() {
  picker_ = picker_factory_->CreatePicker(nullptr);
  if (!picker_) {
    OnPickerDialogResults(
        "Desktop Capture API is not yet implemented for this platform.", {});
    return;
  }

  picker_->Show(
      params_, std::move(source_lists_),
      base::BindOnce(&DesktopMediaPickerController::OnPickerDialogResults,
                     // A weak pointer is used here, because although
                     // |picker_| can't outlive this object, it can
                     // schedule this callback to be invoked
                     // asynchronously after it has potentially been
                     // destroyed.
                     weak_factory_.GetWeakPtr(), std::string()));
}

void DesktopMediaPickerController::OnPickerDialogResults(
    const std::string& err,
    content::DesktopMediaID source) {
  if (done_callback_)
    std::move(done_callback_).Run(err, source);
}
