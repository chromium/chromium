// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_capturer_wrapper.h"

#include "base/check.h"

DesktopCapturerWrapper::DesktopCapturerWrapper(
    std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer)
    : desktop_capturer_(std::move(desktop_capturer)) {
  CHECK(desktop_capturer_);
}

DesktopCapturerWrapper::~DesktopCapturerWrapper() = default;

void DesktopCapturerWrapper::Start(Consumer* consumer) {
  desktop_capturer_->Start(consumer);
}

ThumbnailCapturer::FrameDeliveryMethod
DesktopCapturerWrapper::GetFrameDeliveryMethod() const {
  return FrameDeliveryMethod::kOnRequest;
}

webrtc::DelegatedSourceListController*
DesktopCapturerWrapper::GetDelegatedSourceListController() {
  return desktop_capturer_->GetDelegatedSourceListController();
}

void DesktopCapturerWrapper::CaptureFrame() {
  desktop_capturer_->CaptureFrame();
}

bool DesktopCapturerWrapper::GetSourceList(SourceList* sources) {
  return desktop_capturer_->GetSourceList(sources);
}

bool DesktopCapturerWrapper::SelectSource(SourceId id) {
  return desktop_capturer_->SelectSource(id);
}
