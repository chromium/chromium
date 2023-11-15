// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/present_frames_tracer.h"

#include "components/exo/surface.h"

namespace arc {

PresentFramesTracer::PresentFramesTracer() : weak_ptr_factory_(this) {}

PresentFramesTracer::~PresentFramesTracer() = default;

void PresentFramesTracer::AddCommit(base::TimeTicks commit_ts) {
  commits_.emplace_back((commit_ts - base::TimeTicks()).InMicroseconds());
}

void PresentFramesTracer::ListenForPresent(exo::Surface* surface) {
  surface->RequestPresentationCallback(
      base::BindRepeating(&PresentFramesTracer::RecordPresentedFrame,
                          weak_ptr_factory_.GetWeakPtr()));
}

void PresentFramesTracer::RecordPresentedFrame(
    const gfx::PresentationFeedback& frame) {
  if (frame.failed()) {
    VLOG(5) << "Presentation failed";
  } else if (frame.timestamp == base::TimeTicks()) {
    VLOG(5) << "Discarded frame";
  } else {
    presents_.emplace_back(
        (frame.timestamp - base::TimeTicks()).InMicroseconds());
  }
}

}  // namespace arc
