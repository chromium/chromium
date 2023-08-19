# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is intended to test the handling of simple_watcher style mojo events,
# which are often missing trace events below them and so are all aggregated
# together despite them coming into different mojo interfaces.

from os import sys

import synth_common

from synth_common import ms_to_ns
trace = synth_common.create_trace()

process_track1 = 1234

trace.add_process_track_descriptor(process_track1, pid=0)

process_pid1 = 2345

thread_track1 = 1235

# Main threads have the same ID as the process
thread_tid1 = process_pid1

seq1 = 9876

thread1_counter = 60

touch_move_trace_id = 34576
trace_id1 = touch_move_trace_id + 1
trace_id2 = trace_id1 + 1
trace_id3 = trace_id2 + 1
touch_end_trace_id = trace_id3 + 1

touch_gesture_id = 87654

flow_id1 = 45678
flow_id2 = 45679
flow_id3 = 45680

trace.add_input_latency_event_slice(
    "TouchStart",
    ts=ms_to_ns(0),
    dur=ms_to_ns(1),
    track=touch_move_trace_id,
    trace_id=touch_move_trace_id,
    touch_id=touch_gesture_id)

trace.add_chrome_process_track_descriptor(process_track1, process_pid1)

trace.add_chrome_thread_with_cpu_counter(
    process_track1,
    thread_track1,
    trusted_packet_sequence_id=seq1,
    counter_track=thread1_counter,
    pid=process_pid1,
    tid=thread_tid1,
    thread_type=trace.prototypes.ThreadDescriptor.ChromeThreadType
    .CHROME_THREAD_MAIN)

# Touch move 1 - not janky
trace.add_input_latency_event_slice(
    "TouchMove",
    ts=ms_to_ns(0),
    dur=ms_to_ns(10),
    track=trace_id1,
    trace_id=trace_id1,
    touch_id=touch_gesture_id,
    is_coalesced=0)

trace.add_latency_info_flow(
    ts=ms_to_ns(0),
    dur=ms_to_ns(1),
    trusted_sequence_id=seq1,
    trace_id=trace_id1,
    flow_ids=[flow_id1])

# The slices below will block this "not janky" touch move 1.
trace.add_track_event_slice(
    "task", ts=ms_to_ns(2), dur=ms_to_ns(6), trusted_sequence_id=seq1)

trace.add_track_event_slice(
    "subtask", ts=ms_to_ns(3), dur=ms_to_ns(4), trusted_sequence_id=seq1)
# This ends the blocking slices of "not janky" touch move 1.

trace.add_latency_info_flow(
    ts=ms_to_ns(11),
    dur=ms_to_ns(1),
    trusted_sequence_id=seq1,
    trace_id=trace_id1,
    terminating_flow_ids=[flow_id1])

# Touch move 2 - janky
trace.add_input_latency_event_slice(
    "TouchMove",
    ts=ms_to_ns(16),
    dur=ms_to_ns(33),
    track=trace_id2,
    trace_id=trace_id2,
    touch_id=touch_gesture_id,
    is_coalesced=0)

trace.add_latency_info_flow(
    ts=ms_to_ns(16),
    dur=ms_to_ns(1),
    trusted_sequence_id=seq1,
    trace_id=trace_id2,
    flow_ids=[flow_id2])

# The slices below will block this "janky" touch move 2.
trace.add_track_event_slice(
    "task", ts=ms_to_ns(18), dur=ms_to_ns(29), trusted_sequence_id=seq1)

trace.add_track_event_slice(
    "subtask", ts=ms_to_ns(19), dur=ms_to_ns(27), trusted_sequence_id=seq1)
# This ends the blocking slices of "janky" touch move 2.

trace.add_latency_info_flow(
    ts=ms_to_ns(50),
    dur=ms_to_ns(1),
    trusted_sequence_id=seq1,
    trace_id=trace_id2,
    terminating_flow_ids=[flow_id2])

# Touch move 3 - janky
trace.add_input_latency_event_slice(
    "TouchMove",
    ts=ms_to_ns(55),
    dur=ms_to_ns(33),
    track=trace_id3,
    trace_id=trace_id3,
    touch_id=touch_gesture_id,
    is_coalesced=0)

trace.add_latency_info_flow(
    ts=ms_to_ns(55),
    dur=ms_to_ns(1),
    trusted_sequence_id=seq1,
    trace_id=trace_id3,
    flow_ids=[flow_id3])

# The slices below will block this "janky" touch move 3.
trace.add_track_event_slice(
    "task", ts=ms_to_ns(57), dur=ms_to_ns(29), trusted_sequence_id=seq1)

packet = trace.add_track_event_slice(
    "subtask", ts=ms_to_ns(58), dur=ms_to_ns(27), trusted_sequence_id=seq1)
# This ends the blocking slices of "janky" touch move 3.

trace.add_latency_info_flow(
    ts=ms_to_ns(87),
    dur=ms_to_ns(1),
    trusted_sequence_id=seq1,
    trace_id=trace_id3,
    step=trace.prototypes.ChromeLatencyInfo.Step.STEP_SEND_INPUT_EVENT_UI,
    terminating_flow_ids=[flow_id3])

trace.add_latency_info_flow(
    ts=ms_to_ns(89),
    dur=ms_to_ns(1),
    trusted_sequence_id=seq1,
    trace_id=trace_id3,
    terminating_flow_ids=[flow_id3])

trace.add_input_latency_event_slice(
    "TouchEnd",
    ts=ms_to_ns(90),
    dur=ms_to_ns(2),
    track=touch_end_trace_id,
    trace_id=touch_end_trace_id,
    touch_id=touch_gesture_id)

sys.stdout.buffer.write(trace.trace.SerializeToString())
