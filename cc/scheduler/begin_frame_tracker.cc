// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/begin_frame_tracker.h"

#include "base/trace_event/trace_event.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"

namespace cc {

BeginFrameTracker::BeginFrameTracker(const base::Location& location)
    : location_(location),
      location_string_(location.ToString()),
      current_finished_at_(base::TimeTicks() + base::Microseconds(-1)) {}

BeginFrameTracker::~BeginFrameTracker() = default;

void BeginFrameTracker::Start(const viz::BeginFrameArgs& new_args) {
  // Trace the frame time being passed between BeginFrameTrackers.
  TRACE_EVENT_WITH_FLOW1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.frames"),
                         "BeginFrameArgs",
                         new_args.frame_time.since_origin().InMicroseconds(),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "location", location_string_);

  // Trace this specific begin frame tracker Start/Finish times.
  TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN2(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.frames"),
      location_string_.c_str(),
      TRACE_ID_WITH_SCOPE(location_string_.c_str(),
                          new_args.frame_time.since_origin().InMicroseconds()),
      "new args", new_args.AsValue(), "current args", current_args_.AsValue());

  // Check the new viz::BeginFrameArgs are valid and monotonically increasing.
  DCHECK(new_args.IsValid());
  DCHECK_LE(current_args_.frame_time, new_args.frame_time);

  DCHECK(HasFinished())
      << "Tried to start a new frame before finishing an existing frame.";
  current_updated_at_ = base::TimeTicks::Now();
  current_args_ = new_args;
  current_finished_at_ = base::TimeTicks();

  // TODO(mithro): Add UMA tracking of delta between current_updated_at_ time
  // and the new_args.frame_time argument. This will give us how long after a
  // viz::BeginFrameArgs message was created before we started processing it.
}

const viz::BeginFrameArgs& BeginFrameTracker::Current() const {
  DCHECK(!HasFinished())
      << "Tried to use viz::BeginFrameArgs after marking the frame finished.";
  DCHECK(current_args_.IsValid())
      << "Tried to use viz::BeginFrameArgs before starting a frame!";
  return current_args_;
}

void BeginFrameTracker::Finish() {
  DCHECK(!HasFinished()) << "Tried to finish an already finished frame";
  current_finished_at_ = base::TimeTicks::Now();
  TRACE_EVENT_COPY_NESTABLE_ASYNC_END0(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.frames"),
      location_string_.c_str(),
      TRACE_ID_WITH_SCOPE(
          location_string_.c_str(),
          current_args_.frame_time.since_origin().InMicroseconds()));
}

const viz::BeginFrameArgs& BeginFrameTracker::Last() const {
  DCHECK(current_args_.IsValid())
      << "Tried to use last viz::BeginFrameArgs before starting a frame!";
  DCHECK(HasFinished())
      << "Tried to use last viz::BeginFrameArgs before the frame is finished.";
  return current_args_;
}

base::TimeDelta BeginFrameTracker::Interval() const {
  base::TimeDelta interval = current_args_.interval;
  // Normal interval will be ~16ms, 200Hz (5ms) screens are the fastest
  // easily available so anything less than that is likely an error.
  if (interval < base::Milliseconds(1)) {
    interval = viz::BeginFrameArgs::DefaultInterval();
  }
  return interval;
}

void BeginFrameTracker::AsProtozeroInto(
    perfetto::EventContext& ctx,
    base::TimeTicks now,
    perfetto::protos::pbzero::BeginImplFrameArgsV2* state) const {
  state->set_updated_at_us(current_updated_at_.since_origin().InMicroseconds());
  state->set_finished_at_us(
      current_finished_at_.since_origin().InMicroseconds());
  if (HasFinished()) {
    state->set_state(
        perfetto::protos::pbzero::BeginImplFrameArgsV2::BEGIN_FRAME_FINISHED);
    current_args_.AsProtozeroInto(ctx, state->set_current_args());
  } else {
    state->set_state(
        perfetto::protos::pbzero::BeginImplFrameArgsV2::BEGIN_FRAME_USING);
    current_args_.AsProtozeroInto(ctx, state->set_last_args());
  }

  base::TimeTicks frame_time = current_args_.frame_time;
  base::TimeTicks deadline = current_args_.deadline;
  base::TimeDelta interval = current_args_.interval;

  auto* timestamps = state->set_timestamps_in_us();
  timestamps->set_interval_delta(interval.InMicroseconds());
  timestamps->set_now_to_deadline_delta((deadline - now).InMicroseconds());
  timestamps->set_frame_time_to_now_delta((now - frame_time).InMicroseconds());
  timestamps->set_frame_time_to_deadline_delta(
      (deadline - frame_time).InMicroseconds());
  timestamps->set_now(now.since_origin().InMicroseconds());
  timestamps->set_frame_time(frame_time.since_origin().InMicroseconds());
  timestamps->set_deadline(deadline.since_origin().InMicroseconds());
}

const viz::BeginFrameArgs& BeginFrameTracker::DangerousMethodCurrentOrLast()
    const {
  if (!HasFinished()) {
    return Current();
  } else {
    return Last();
  }
}

}  // namespace cc
