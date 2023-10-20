#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Discarded events that do not get to GPU are invisible for UMA metric and
# therefore should be excluded in trace-based metric. This tests ensures that's
# the case.

from os import sys

import synth_common

from synth_common import ms_to_ns
trace = synth_common.create_trace()

from chrome_scroll_helper import ChromeScrollHelper

helper = ChromeScrollHelper(trace, start_id=1234, start_gesture_id=5678)

helper.begin(from_ms=0, dur_ms=10)
helper.update(from_ms=15, dur_ms=10)
# The next update should be recognized as janky
helper.update(from_ms=30, dur_ms=30)
helper.end(from_ms=70, dur_ms=10)

helper.begin(from_ms=100, dur_ms=10)
helper.update(from_ms=115, dur_ms=10)
# The next update doesn't get to GPU, therefore would not be a part of jank
# calculation
helper.update(from_ms=130, dur_ms=30, gets_to_gpu=False)
helper.end(from_ms=170, dur_ms=10)

sys.stdout.buffer.write(trace.trace.SerializeToString())
