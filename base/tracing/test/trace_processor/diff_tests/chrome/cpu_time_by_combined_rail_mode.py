#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from os import sys

import synth_common

trace = synth_common.create_trace()

process_track1 = 1234
process_track2 = 4567

process_pid1 = 2345
process_pid2 = 5678

thread_track1 = 1235
thread_track2 = 4568

rail_track1 = 1236
rail_track2 = 4569

# Main threads have the same ID as the process
thread_tid1 = process_pid1
thread_tid2 = process_pid2

seq1 = 9876
seq2 = 9877

thread1_counter = 60
thread2_counter = 61

trace.add_chrome_process_track_descriptor(process_track1, process_pid1)
trace.add_chrome_process_track_descriptor(process_track2, process_pid2)

trace.add_chrome_thread_with_cpu_counter(
    process_track1,
    thread_track1,
    trusted_packet_sequence_id=seq1,
    counter_track=thread1_counter,
    pid=process_pid1,
    tid=thread_tid1,
    thread_type=trace.prototypes.ThreadDescriptor.ChromeThreadType
    .CHROME_THREAD_MAIN)

trace.add_chrome_thread_with_cpu_counter(
    process_track2,
    thread_track2,
    trusted_packet_sequence_id=seq2,
    counter_track=thread2_counter,
    pid=process_pid2,
    tid=thread_tid2,
    thread_type=trace.prototypes.ThreadDescriptor.ChromeThreadType
    .CHROME_THREAD_MAIN)

trace.add_track_descriptor(rail_track1, parent=process_track1)
trace.add_track_descriptor(rail_track2, parent=process_track2)

trace.add_track_event_slice(
    "task", 0, 5000, trusted_sequence_id=seq1, cpu_start=0, cpu_delta=10000)
trace.add_track_event_slice(
    "task",
    5000,
    5000,
    trusted_sequence_id=seq1,
    cpu_start=12000,
    cpu_delta=4000)

trace.add_track_event_slice(
    "task",
    10000,
    6000,
    trusted_sequence_id=seq1,
    cpu_start=18000,
    cpu_delta=2000)
trace.add_track_event_slice(
    "task",
    16000,
    4000,
    trusted_sequence_id=seq1,
    cpu_start=20000,
    cpu_delta=7000)

trace.add_track_event_slice(
    "task",
    30000,
    10000,
    trusted_sequence_id=seq1,
    cpu_start=30000,
    cpu_delta=1000)

trace.add_rail_mode_slice(
    ts=0,
    dur=10000,
    track=rail_track1,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_RESPONSE)
trace.add_rail_mode_slice(
    ts=10000,
    dur=20000,
    track=rail_track1,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_LOAD)
trace.add_rail_mode_slice(
    ts=30000,
    dur=-1,
    track=rail_track1,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_IDLE)

trace.add_track_event_slice(
    "task", 0, 10000, trusted_sequence_id=seq2, cpu_start=0, cpu_delta=10000)

trace.add_track_event_slice(
    "task",
    10000,
    15000,
    trusted_sequence_id=seq2,
    cpu_start=12000,
    cpu_delta=1000)

trace.add_track_event_slice(
    "task",
    35000,
    10000,
    trusted_sequence_id=seq2,
    cpu_start=20000,
    cpu_delta=20000)

trace.add_track_event_slice(
    "task",
    45000,
    10000,
    trusted_sequence_id=seq2,
    cpu_start=40000,
    cpu_delta=1000)

trace.add_rail_mode_slice(
    ts=0,
    dur=10000,
    track=rail_track2,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_ANIMATION)
trace.add_rail_mode_slice(
    ts=10000,
    dur=25000,
    track=rail_track2,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_IDLE)
trace.add_rail_mode_slice(
    ts=35000,
    dur=10000,
    track=rail_track2,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_ANIMATION)
trace.add_rail_mode_slice(
    ts=45000,
    dur=10000,
    track=rail_track2,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_IDLE)

sys.stdout.buffer.write(trace.trace.SerializeToString())
