#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks that compiling targets in BUILD.gn file fails."""

import argparse
import json
import os
import subprocess
import re
import sys
from util import build_utils

_CHROMIUM_SRC = os.path.normpath(os.path.join(__file__, '..', '..', '..', '..'))
_NINJA_PATH = os.path.join(_CHROMIUM_SRC, 'third_party', 'depot_tools', 'ninja')

# Relative to _CHROMIUM_SRC
_GN_SRC_REL_PATH = os.path.join('third_party', 'depot_tools', 'gn')


def _raise_command_exception(args, returncode, output):
  """Raises an exception whose message describes a command failure.

    Args:
      args: shell command-line (as passed to subprocess.Popen())
      returncode: status code.
      output: command output.
    Raises:
      a new Exception.
    """
  message = 'Command failed with status {}: {}\n' \
      'Output:-----------------------------------------\n{}\n' \
      '------------------------------------------------\n'.format(
          returncode, args, output)
  raise Exception(message)


def _run_command(args, cwd=None):
  """Runs shell command. Raises exception if command fails."""
  p = subprocess.Popen(args,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT,
                       cwd=cwd)
  pout, _ = p.communicate()
  if p.returncode != 0:
    _raise_command_exception(args, p.returncode, pout)


def _run_command_get_output(args, success_output):
  """Runs shell command and returns command output."""
  p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  pout, _ = p.communicate()
  if p.returncode == 0:
    return success_output

  # For Python3 only:
  if isinstance(pout, bytes) and sys.version_info >= (3, ):
    pout = pout.decode('utf-8')
  return pout


def _copy_and_append_gn_args(src_args_path, dest_args_path, extra_args):
  """Copies args.gn.

    Args:
      src_args_path: args.gn file to copy.
      dest_args_path: Copy file destination.
      extra_args: Text to append to args.gn after copy.
    """
  with open(src_args_path) as f_in, open(dest_args_path, 'w') as f_out:
    f_out.write(f_in.read())
    f_out.write('\n')
    f_out.write('\n'.join(extra_args))


def _find_lines_after_prefix(text, prefix, num_lines):
  """Searches |text| for a line which starts with |prefix|.

  Args:
    text: String to search in.
    prefix: Prefix to search for.
    num_lines: Number of lines, starting with line with prefix, to return.
  Returns:
    Matched lines. Returns None otherwise.
  """
  lines = text.split('\n')
  for i, line in enumerate(lines):
    if line.startswith(prefix):
      return lines[i:i + num_lines]
  return None


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--gn-args-path',
                      required=True,
                      help='Path to args.gn file.')
  parser.add_argument('--test-configs-path',
                      required=True,
                      help='Path to file with test configurations')
  parser.add_argument('--out-dir',
                      required=True,
                      help='Path to output directory to use for compilation.')
  parser.add_argument('--stamp', help='Path to touch.')
  options = parser.parse_args()

  with open(options.test_configs_path) as f:
    test_configs = json.loads(f.read())

  if not os.path.exists(options.out_dir):
    os.makedirs(options.out_dir)

  out_gn_args_path = os.path.join(options.out_dir, 'args.gn')
  extra_gn_args = [
      'enable_android_nocompile_tests = true',
      'treat_warnings_as_errors = true',
      # GOMA does not work with non-standard output directories.
      'use_goma = false',
  ]
  _copy_and_append_gn_args(options.gn_args_path, out_gn_args_path,
                           extra_gn_args)

  # As all of the test targets are declared in the same BUILD.gn file, it does
  # not matter which test target is used as the root target.
  gn_args = [
      _GN_SRC_REL_PATH, '--root-target=' + test_configs[0]['target'], 'gen',
      os.path.relpath(options.out_dir, _CHROMIUM_SRC)
  ]
  _run_command(gn_args, cwd=_CHROMIUM_SRC)

  error_messages = []
  for config in test_configs:
    # Strip leading '//'
    gn_path = config['target'][2:]
    expect_regex = config['expect_regex']
    ninja_args = [_NINJA_PATH, '-C', options.out_dir, gn_path]

    # Purpose of quotes at beginning of message is to make it clear that
    # "Compile successful." is not a compiler log message.
    test_output = _run_command_get_output(ninja_args, '""\nCompile successful.')

    failure_message_lines = _find_lines_after_prefix(test_output, 'FAILED:', 5)

    found_expect_regex = False
    if failure_message_lines:
      for line in failure_message_lines:
        if re.search(expect_regex, line):
          found_expect_regex = True
          break
    if not found_expect_regex:
      error_message = '//{} failed.\nExpected compile output pattern:\n'\
          '{}\nActual compile output:\n{}'.format(
              gn_path, expect_regex, test_output)
      error_messages.append(error_message)

  if error_messages:
    raise Exception('\n'.join(error_messages))

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  main()
