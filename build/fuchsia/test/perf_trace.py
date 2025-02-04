# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Retrieves the system metrics and logs them into the monitors system. """

import json
import os
import subprocess

from numbers import Number
from typing import Optional

import monitors

from common import run_ffx_command, get_host_tool_path

# Copy to avoid cycle dependency.
TEMP_DIR = os.environ.get('TMPDIR', '/tmp')

FXT_FILE = os.path.join(TEMP_DIR, 'perf_trace.fxt')
JSON_FILE = os.path.join(TEMP_DIR, 'perf_trace.json')

METRIC_FILTERS = [
    'gfx/ContiguousPooledMemoryAllocator::Allocate/size_bytes',
    'gfx/SysmemProtectedPool/size',
    'gfx/WindowedFramePredictor::GetPrediction/Predicted frame duration(ms)',
    'gfx/WindowedFramePredictor::GetPrediction/Render time(ms)',
    'gfx/WindowedFramePredictor::GetPrediction/Update time(ms)',
    'memory_monitor/bandwidth_free/value',
    'memory_monitor/free/free$',
    'system_metrics/cpu_usage/average_cpu_percentage',
]


def start() -> None:
    """ Starts the system tracing. """
    # TODO(crbug.com/40935291): May include kernel:meta, kernel:sched, magma,
    # oemcrypto, media, kernel:syscall
    run_ffx_command(cmd=('trace', 'start', '--background', '--categories',
                         'gfx,memory_monitor,system_metrics', '--output',
                         FXT_FILE))


def stop(prefix: Optional[str] = None) -> None:
    """ Stops the system tracing and logs the metrics into the monitors system
    with an optional prefix as part of the metric names. """
    run_ffx_command(cmd=('trace', 'stop'))
    _parse_trace(prefix)


# pylint: disable=too-many-nested-blocks
def _parse_trace(prefix: Optional[str] = None) -> None:
    subprocess.run([
        get_host_tool_path('trace2json'), f'--input-file={FXT_FILE}',
        f'--output-file={JSON_FILE}'
    ],
                   check=True)
    with open(JSON_FILE, 'r') as file:
        recorders = {}
        for event in json.load(file)['traceEvents']:
            if not 'args' in event:
                # Support only the events with args now.
                continue
            cat_name = [event['cat'], event['name']]
            if prefix:
                cat_name.insert(0, prefix)
            args = event['args']
            # Support only the events with str or numeric args now.
            for arg in args:
                if isinstance(args[arg], str):
                    cat_name.append(arg)
                    cat_name.append(args[arg])
            for arg in args:
                # Allows all number types.
                if isinstance(args[arg], Number):
                    name = cat_name.copy()
                    name.append(arg)
                    for f in METRIC_FILTERS:
                        if f in '/'.join(name) + '$':
                            if tuple(name) not in recorders:
                                recorders[tuple(name)] = monitors.average(
                                    *name)
                            recorders[tuple(name)].record(args[arg])


# For tests only. To run this test, create a perf_trace.fxt in the /tmp/, run
# this script and inspect /tmp/test_script_metrics.jsonpb.
#
# If nothing needs to be customized, the commands look like,
# $ ffx trace start --background \
#                   --categories 'gfx,memory_monitor,system_metrics' \
#                   --output /tmp/perf_trace.fxt
# -- do something on the fuchsia device --
# $ ffx trace stop
# $ vpython3 build/fuchsia/test/perf_trace.py
#
# Note, reuse the perf_trace.fxt is OK, i.e. running perf_trace.py multiple
# times works.
if __name__ == '__main__':
    _parse_trace()
    monitors.dump(TEMP_DIR)
