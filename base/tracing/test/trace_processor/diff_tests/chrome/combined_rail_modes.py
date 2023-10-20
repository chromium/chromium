#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from os import sys

import synth_common

trace = synth_common.create_trace()

track1 = 1234
track2 = 4567

trace.add_process_track_descriptor(track1, pid=0)
trace.add_process_track_descriptor(track2, pid=2)

trace.add_rail_mode_slice(
    ts=0,
    dur=10000,
    track=track1,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_RESPONSE)
trace.add_rail_mode_slice(
    ts=10000,
    dur=20000,
    track=track1,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_LOAD)
trace.add_rail_mode_slice(
    ts=30000,
    dur=-1,
    track=track1,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_IDLE)

trace.add_rail_mode_slice(
    ts=0,
    dur=10000,
    track=track2,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_ANIMATION)
trace.add_rail_mode_slice(
    ts=10000,
    dur=25000,
    track=track2,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_IDLE)
trace.add_rail_mode_slice(
    ts=25000,
    dur=10000,
    track=track2,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_ANIMATION)
trace.add_rail_mode_slice(
    ts=35000,
    dur=10000,
    track=track2,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_IDLE)

sys.stdout.buffer.write(trace.trace.SerializeToString())
