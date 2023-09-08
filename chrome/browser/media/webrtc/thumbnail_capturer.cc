// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/thumbnail_capturer.h"

#include "base/notreached.h"

void ThumbnailCapturer::SetMaxFrameRate(uint32_t max_frame_rate) {
  NOTREACHED_NORETURN();
}

webrtc::DelegatedSourceListController*
ThumbnailCapturer::GetDelegatedSourceListController() {
  return nullptr;
}

void ThumbnailCapturer::CaptureFrame() {
  NOTREACHED_NORETURN();
}

bool ThumbnailCapturer::SelectSource(SourceId id) {
  NOTREACHED_NORETURN();
}

void ThumbnailCapturer::SelectSources(const std::vector<SourceId>& ids,
                                      gfx::Size thumbnail_size) {
  NOTREACHED_NORETURN();
}
