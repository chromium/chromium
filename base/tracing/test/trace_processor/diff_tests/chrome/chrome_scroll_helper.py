#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Discarded events that do not get to GPU are invisible for UMA metric and
# therefore should be excluded in trace-based metric. This tests ensures that's
# the case.

import synth_common

from synth_common import ms_to_ns

trace = synth_common.create_trace()


class ChromeScrollHelper:

  def __init__(self, trace, start_id, start_gesture_id):
    self.trace = trace
    self.id = start_id
    self.gesture_id = start_gesture_id

  def begin(self, from_ms, dur_ms):
    self.trace.add_input_latency_event_slice(
        "GestureScrollBegin",
        ts=ms_to_ns(from_ms),
        dur=ms_to_ns(dur_ms),
        track=self.id,
        trace_id=self.id,
        gesture_scroll_id=self.gesture_id,
    )
    self.id += 1

  def update(self, from_ms, dur_ms, gets_to_gpu=True):
    self.trace.add_input_latency_event_slice(
        "GestureScrollUpdate",
        ts=ms_to_ns(from_ms),
        dur=ms_to_ns(dur_ms),
        track=self.id,
        trace_id=self.id,
        gesture_scroll_id=self.gesture_id,
        gets_to_gpu=gets_to_gpu,
        is_coalesced=False,
    )
    self.id += 1

  def end(self, from_ms, dur_ms):
    self.trace.add_input_latency_event_slice(
        "GestureScrollEnd",
        ts=ms_to_ns(from_ms),
        dur=ms_to_ns(dur_ms),
        track=self.id,
        trace_id=self.id,
        gesture_scroll_id=self.gesture_id)
    self.id += 1
    self.gesture_id += 1
