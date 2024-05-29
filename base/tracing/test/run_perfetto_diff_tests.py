#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A wrapper script for //third_party/perfetto/diff_test_trace_processor.py.

import argparse
import subprocess
import sys
import os
import time
import json

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--trace-descriptor', type=str, required=True)
  parser.add_argument('--test-extensions-descriptor', type=str, required=True)
  parser.add_argument('--metrics-descriptor', type=str, required=True)
  parser.add_argument(
    '--all-chrome-metrics-descriptor', type=str, required=True)
  parser.add_argument(
    '--chrome-track-event-descriptor', type=str, required=True)
  parser.add_argument(
    '--winscope-extensions-descriptor', type=str, required=True)
  parser.add_argument(
      '--chrome-stdlib', type=str, required=True)
  parser.add_argument('--test-dir', type=str, required=True)
  parser.add_argument(
      '--trace-processor-shell', type=str, required=True)
  parser.add_argument("--name-filter", default="", type=str, required=False)
  parser.add_argument("--script", type=str, required=True)
  args, _ = parser.parse_known_args()

  cmd = [
    "vpython3", args.script,
    "--trace-descriptor", args.trace_descriptor,
    "--test-extensions", args.test_extensions_descriptor,
    "--metrics-descriptor", args.metrics_descriptor,
                            args.all_chrome_metrics_descriptor,
    "--chrome-track-event-descriptor", args.chrome_track_event_descriptor,
    "--winscope-extensions", args.winscope_extensions_descriptor,
    "--override-sql-module", os.path.abspath(args.chrome_stdlib),
    "--test-dir", args.test_dir,
    "--name-filter",
    args.name_filter,
    args.trace_processor_shell,
  ]

  test_start_time = time.time()
  completed_process = subprocess.run(cmd, capture_output=True)

  sys.stderr.buffer.write(completed_process.stderr)
  sys.stdout.buffer.write(completed_process.stdout)
  return completed_process.returncode

if __name__ == '__main__':
  sys.exit(main())
