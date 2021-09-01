#!/usr/bin/env vpython3
#
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys


class SkylabClientTestTest:

  # The basic shell script for client test run in Skylab. The arguments listed
  # here will be fed by autotest at the run time.
  #
  # * test-launcher-summary-output: the path for the json result. It will be
  #     assigned by autotest, who will upload it to GCS upon test completion.
  # * test-launcher-shard-index: the index for this test run.
  # * test-launcher-total-shards: the total test shards.
  # * test_args: arbitrary runtime arguments configured in test_suites.pyl,
  #     attached after '--'.
  BASIC_SHELL_SCRIPT = """
#!/bin/sh

while [[ $# -gt 0 ]]; do
    case "$1" in
        --test-launcher-summary-output)
            summary_output=$2
            shift 2
            ;;

        --test-launcher-shard-index)
            shard_index=$2
            shift 2
            ;;

        --test-launcher-total-shards)
            total_shards=$2
            shift 2
            ;;

        --)
            test_args=$2
            break
            ;;

        *)
            break
            ;;
    esac
done

if [ ! -d $(dirname $summary_output) ] ; then
    mkdir -p $(dirname $summary_output)
fi

cd `dirname $0` && cd ..
  """

  def __init__(self, args):
    self.test_exe = args.test_exe
    self.output = args.output

  @property
  def suite_name(self):
    return self.test_exe

  def build_test_script(self):
    # Build the shell script that will be used on the device to invoke the test.
    # Stored here as a list of lines.
    device_test_script_contents = self.BASIC_SHELL_SCRIPT.split('\n')

    test_invocation = ('LD_LIBRARY_PATH=./ ./%s '
                       ' --test-launcher-summary-output=$summary_output'
                       ' --test-launcher-shard-index=$shard_index'
                       ' --test-launcher-total-shards=$total_shards'
                       ' $test_args' % self.test_exe)

    device_test_script_contents.append(test_invocation)
    with open(self.output, 'w') as w:
      w.write('\n'.join(device_test_script_contents) + '\n')
      os.chmod(self.output, 0o755)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--test-exe',
      type=str,
      required=True,
      help='Path to test executable to run inside the device.')
  parser.add_argument('--verbose', '-v', action='store_true')
  parser.add_argument(
      '--output',
      required=True,
      type=str,
      help='Path to create the runner script.')

  args = parser.parse_args()

  test = SkylabClientTestTest(args)
  test.build_test_script()
  return 0


if __name__ == '__main__':
  sys.exit(main())
