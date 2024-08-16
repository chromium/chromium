// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/thumbnail_capturer.h"

#include "base/notreached.h"

void ThumbnailCapturer::SetMaxFrameRate(uint32_t max_frame_rate) {
  NOTREACHED();
}

webrtc::DelegatedSourceListController*
ThumbnailCapturer::GetDelegatedSourceListController() {
  return nullptr;
}

void ThumbnailCapturer::CaptureFrame() {
  NOTREACHED();
}

bool ThumbnailCapturer::SelectSource(SourceId id) {
  NOTREACHED();
}

void ThumbnailCapturer::SelectSources(const std::vector<SourceId>& ids,
                                      gfx::Size thumbnail_size) {
  NOTREACHED();
}
