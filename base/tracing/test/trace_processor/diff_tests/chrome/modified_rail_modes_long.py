# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from os import sys

import synth_common
from synth_common import s_to_ns

trace = synth_common.create_trace()

trace.add_chrome_metadata(os_name="Android")

track1 = 1234
gpu_track = 7890

trace.add_process_track_descriptor(track1, pid=0)
trace.add_process_track_descriptor(gpu_track, pid=4)

trace.add_rail_mode_slice(
    ts=0,
    dur=s_to_ns(1),
    track=track1,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_RESPONSE)
trace.add_rail_mode_slice(
    ts=s_to_ns(1),
    dur=-1,
    track=track1,
    mode=trace.prototypes.ChromeRAILMode.RAIL_MODE_IDLE)

# Generate an extra trace event long after events on the renderer process have
# ceased to ensure that the RAIL mode is extended to the end of the process
# rather than the end of the trace itself.
trace.add_track_event_slice("VSync", ts=s_to_ns(25), dur=10, track=gpu_track)

sys.stdout.buffer.write(trace.trace.SerializeToString())
