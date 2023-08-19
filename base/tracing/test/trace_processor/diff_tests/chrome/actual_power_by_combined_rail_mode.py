# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from os import sys

import synth_common
from synth_common import ms_to_ns

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

packet = trace.add_packet()
packet = trace.add_power_rails_desc(0, "PPVAR_VPH_PWR_RF")
packet = trace.add_power_rails_desc(1, "PPVAR_VPH_PWR_S1C")

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

trace.add_rail_mode_slice(
    ts=0,
    dur=ms_to_ns(10),
    track=rail_track1,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_RESPONSE)
trace.add_rail_mode_slice(
    ts=ms_to_ns(10),
    dur=ms_to_ns(20),
    track=rail_track1,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_LOAD)
trace.add_rail_mode_slice(
    ts=ms_to_ns(30),
    dur=-1,
    track=rail_track1,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_IDLE)

trace.add_track_event_slice(
    "task",
    0,
    ms_to_ns(10),
    trusted_sequence_id=seq2,
    cpu_start=0,
    cpu_delta=ms_to_ns(10))

trace.add_rail_mode_slice(
    ts=0,
    dur=ms_to_ns(10),
    track=rail_track2,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_ANIMATION)
trace.add_rail_mode_slice(
    ts=ms_to_ns(10),
    dur=ms_to_ns(25),
    track=rail_track2,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_IDLE)
trace.add_rail_mode_slice(
    ts=ms_to_ns(35),
    dur=ms_to_ns(10),
    track=rail_track2,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_ANIMATION)
trace.add_rail_mode_slice(
    ts=ms_to_ns(45),
    dur=ms_to_ns(10),
    track=rail_track2,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_IDLE)

packet = trace.add_packet()

# cellular
packet = trace.add_power_rails_data(0, 0, 0)
packet = trace.add_power_rails_data(10, 0, 0)
packet = trace.add_power_rails_data(20, 0, 30)
packet = trace.add_power_rails_data(30, 0, 50)
packet = trace.add_power_rails_data(40, 0, 55)
packet = trace.add_power_rails_data(50, 0, 56)
packet = trace.add_power_rails_data(55, 0, 56)

# cpu little cores
packet = trace.add_power_rails_data(0, 1, 0)
packet = trace.add_power_rails_data(10, 1, 20)
packet = trace.add_power_rails_data(20, 1, 30)
packet = trace.add_power_rails_data(30, 1, 40)
packet = trace.add_power_rails_data(40, 1, 42)
packet = trace.add_power_rails_data(50, 1, 60)
packet = trace.add_power_rails_data(55, 1, 61)

sys.stdout.buffer.write(trace.trace.SerializeToString())
