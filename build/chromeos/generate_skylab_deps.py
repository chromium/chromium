#!/usr/bin/env vpython3
#
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

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


def build_test_script(args):
  # Build the shell script that will be used on the device to invoke the test.
  # Stored here as a list of lines.
  device_test_script_contents = BASIC_SHELL_SCRIPT.split('\n')

  test_invocation = ('LD_LIBRARY_PATH=./ ./%s '
                     ' --test-launcher-summary-output=$summary_output'
                     ' --test-launcher-shard-index=$shard_index'
                     ' --test-launcher-total-shards=$total_shards'
                     ' $test_args' % args.test_exe)

  device_test_script_contents.append(test_invocation)
  with open(args.output, 'w') as w:
    w.write('\n'.join(device_test_script_contents) + '\n')
    os.chmod(args.output, 0o755)


def build_filter_file(args):
  with open(args.output, 'w') as w:
    if args.disabled_tests is not None:
      w.write('\n'.join('-{0}'.format(test) for test in args.disabled_tests) +
              '\n')
    if args.tests is not None:
      w.write('\n'.join(args.tests) + '\n')
    os.chmod(args.output, 0o755)


def main():
  parser = argparse.ArgumentParser()
  subparsers = parser.add_subparsers(dest='command')

  script_gen_parser = subparsers.add_parser('generate-runner')
  script_gen_parser.add_argument(
      '--test-exe',
      type=str,
      required=True,
      help='Path to test executable to run inside the device.')
  script_gen_parser.add_argument('--verbose', '-v', action='store_true')
  script_gen_parser.add_argument(
      '--output',
      required=True,
      type=str,
      help='Path to create the runner script.')
  script_gen_parser.set_defaults(func=build_test_script)

  filter_gen_parser = subparsers.add_parser('generate-filter')
  filter_gen_parser.add_argument(
      '--disabled-tests',
      type=str,
      required=False,
      action='append',
      help='Space separated test names to prevent running. This generates the \
          negative filter')
  filter_gen_parser.add_argument(
      '--tests',
      type=str,
      required=False,
      action='append',
      help='Space separated test names to be run. This generates a positive \
          filter.')
  filter_gen_parser.add_argument(
      '--output',
      required=True,
      type=str,
      help='Path to create the plain text filter file.')
  filter_gen_parser.set_defaults(func=build_filter_file)

  args = parser.parse_args()

  if (args.command == "generate-filter" and args.disabled_tests is None and
      args.tests is None):
    parser.error('disabled_tests or tests must be provided to generate-filter')
  args.func(args)

  return 0


if __name__ == '__main__':
  sys.exit(main())
