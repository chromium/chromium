#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A wrapper script for //third_party/perfetto/diff_test_trace_processor.py.

import argparse
import json
import subprocess
import sys
import os
import time

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--trace-descriptor', type=str, required=True)
  parser.add_argument('--test-extensions-descriptor', type=str, required=True)
  parser.add_argument('--metrics-descriptor', type=str,
                      nargs='+', required=True)
  parser.add_argument(
    '--chrome-track-event-descriptor', type=str, required=True)
  parser.add_argument(
    '--winscope-extensions-descriptor', type=str, required=True)
  parser.add_argument(
    '--gpu-extensions-descriptor', type=str, required=True)
  parser.add_argument('--gpu-interned-data-extensions', type=str, default=None)
  parser.add_argument(
      '--summary-descriptor', type=str, required=True)
  parser.add_argument(
      '--chrome-stdlib', type=str, required=True)
  parser.add_argument('--test-dir', type=str, required=True)
  parser.add_argument(
      '--trace-processor-shell', type=str, required=True)
  parser.add_argument("--name-filter", default="", type=str, required=False)
  parser.add_argument("--script", type=str, required=True)
  parser.add_argument('--isolated-script-test-output', type=str, required=False)
  args, _ = parser.parse_known_args()

  cmd = [
    "vpython3", args.script,
    "--trace-descriptor", args.trace_descriptor,
    "--test-extensions", args.test_extensions_descriptor,
    "--metrics-descriptor",
  ]
  cmd.extend(args.metrics_descriptor)
  cmd.extend([
    "--chrome-track-event-descriptor", args.chrome_track_event_descriptor,
    "--winscope-extensions", args.winscope_extensions_descriptor,
    "--gpu-extensions", args.gpu_extensions_descriptor,
    "--gpu-interned-data-extensions", args.gpu_interned_data_extensions,
    "--summary-descriptor", args.summary_descriptor,
    "--override-sql-package", os.path.abspath(args.chrome_stdlib),
    "--test-dir", args.test_dir,
    "--name-filter",
    args.name_filter,
    args.trace_processor_shell,
  ])

  test_start_time = time.time()
  completed_process = subprocess.run(cmd, capture_output=True)

  sys.stderr.buffer.write(completed_process.stderr)
  sys.stdout.buffer.write(completed_process.stdout)

  if args.isolated_script_test_output:
    failure_type = 'PASS' if completed_process.returncode == 0 else 'FAIL'
    results_json = {
        'version': 3,
        'interrupted': False,
        'num_failures_by_type': {
            failure_type: 1
        },
        'path_delimiter': '/',
        'seconds_since_epoch': int(test_start_time),
        'tests': {
            'perfetto_diff_tests': {
                'expected': 'PASS',
                'actual': failure_type,
                'time': time.time() - test_start_time,
            },
        },
    }
    with open(args.isolated_script_test_output, 'w') as fp:
      json.dump(results_json, fp)

  return completed_process.returncode

if __name__ == '__main__':
  sys.exit(main())
