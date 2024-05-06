// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_PRESENT_FRAMES_TRACER_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_PRESENT_FRAMES_TRACER_H_

#include <deque>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/exo/surface_observer.h"
#include "ui/gfx/presentation_feedback.h"

namespace exo {
class Surface;
}  // namespace exo

namespace arc {

// Traces commits and, more importantly, frame presentation events, which
// enables perceived FPS measurement.
// Stores timestamps in tick counts (microseconds) accumulated over the course
// of a trace. Backed by deques to give O(1) non-amortized insertion time.
class PresentFramesTracer {
 public:
  PresentFramesTracer();
  ~PresentFramesTracer();

  // Prevent accidental copying.
  PresentFramesTracer(const PresentFramesTracer&) = delete;
  PresentFramesTracer& operator=(const PresentFramesTracer&) = delete;

  // Record a commit as having occurred at the given time.
  void AddCommit(base::TimeTicks commit_ts);

  // Record a present as having occurred at the given time. Ideally you would
  // use ListenForPresent, but for detached displays the present event may never
  // occur, but we still want to consider it as if it happened for performance
  // metrics and testing.
  void AddPresent(base::TimeTicks present_ts);

  // Record the next frame presented event on the given surface. This can be
  // called from within SurfaceObserver::OnCommit.
  void ListenForPresent(exo::Surface* surface);

  const std::deque<int64_t>& commits() const { return commits_; }
  const std::deque<int64_t>& presents() const { return presents_; }

 private:
  void RecordPresentedFrame(const gfx::PresentationFeedback& frame);

  std::deque<int64_t> commits_, presents_;
  base::WeakPtrFactory<PresentFramesTracer> weak_ptr_factory_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_PRESENT_FRAMES_TRACER_H_
