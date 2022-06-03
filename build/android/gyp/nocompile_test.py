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

# Regex for determining whether compile failed because 'gn gen' needs to be run.
_GN_GEN_REGEX = re.compile(r'ninja: (error|fatal):')


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


def _run_command_get_failure_output(args):
  """Runs shell command.

  Returns:
      Command output if command fails, None if command succeeds.
  """
  p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  pout, _ = p.communicate()

  if p.returncode == 0:
    return None

  # For Python3 only:
  if isinstance(pout, bytes) and sys.version_info >= (3, ):
    pout = pout.decode('utf-8')
  return '' if pout is None else pout


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


def _find_regex_in_test_failure_output(test_output, regex):
  """Searches for regex in test output.

    Args:
      test_output: test output.
      regex: regular expression to search for.
    Returns:
      Whether the regular expression was found in the part of the test output
      after the 'FAILED' message.

      If the regex does not contain '\n':
        the first 5 lines after the 'FAILED' message (including the text on the
        line after the 'FAILED' message) is searched.
      Otherwise:
        the entire test output after the 'FAILED' message is searched.
  """
  if test_output is None:
    return False

  failed_index = test_output.find('FAILED')
  if failed_index < 0:
    return False

  failure_message = test_output[failed_index:]
  if regex.find('\n') >= 0:
    return re.search(regex, failure_message)

  return _search_regex_in_list(failure_message.split('\n')[:5], regex)


def _search_regex_in_list(value, regex):
  for line in value:
    if re.search(regex, line):
      return True
  return False


def _do_build_get_failure_output(gn_path, gn_cmd, options):
  # Extract directory from test target. As all of the test targets are declared
  # in the same BUILD.gn file, it does not matter which test target is used.
  target_dir = gn_path.rsplit(':', 1)[0]

  if gn_cmd is not None:
    gn_args = [
        _GN_SRC_REL_PATH, '--root-target=' + target_dir, gn_cmd,
        os.path.relpath(options.out_dir, _CHROMIUM_SRC)
    ]
    _run_command(gn_args, cwd=_CHROMIUM_SRC)

  ninja_args = [_NINJA_PATH, '-C', options.out_dir, gn_path]
  return _run_command_get_failure_output(ninja_args)


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
    # Escape '\' in '\.' now. This avoids having to do the escaping in the test
    # specification.
    config_text = f.read().replace(r'\.', r'\\.')
    test_configs = json.loads(config_text)

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

  ran_gn_gen = False
  did_clean_build = False
  error_messages = []
  for config in test_configs:
    # Strip leading '//'
    gn_path = config['target'][2:]
    expect_regex = config['expect_regex']

    test_output = _do_build_get_failure_output(gn_path, None, options)

    # 'gn gen' takes > 1s to run. Only run 'gn gen' if it is needed for compile.
    if (test_output
        and _search_regex_in_list(test_output.split('\n'), _GN_GEN_REGEX)):
      assert not ran_gn_gen
      ran_gn_gen = True
      test_output = _do_build_get_failure_output(gn_path, 'gen', options)

    if (not _find_regex_in_test_failure_output(test_output, expect_regex)
        and not did_clean_build):
      # Ensure the failure is not due to incremental build.
      did_clean_build = True
      test_output = _do_build_get_failure_output(gn_path, 'clean', options)

    if not _find_regex_in_test_failure_output(test_output, expect_regex):
      if test_output is None:
        # Purpose of quotes at beginning of message is to make it clear that
        # "Compile successful." is not a compiler log message.
        test_output = '""\nCompile successful.'
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
