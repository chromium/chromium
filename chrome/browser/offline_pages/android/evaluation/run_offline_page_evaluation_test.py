#!/usr/bin/env python
#
# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#
# This script is used to run OfflinePageSavePageLaterEvaluationTests.
# The test will try to call SavePageLater on the list provided as the input,
# and generate results of the background offlining. Then it will pull the
# results to the output directory.
#
# Example Steps:
# 1. Build chrome_public_test_apk
# 2. Prepare a list of urls.
# 3. Run the script (use -d when you have more than one device connected.)
#   run_offline_page_evaluation_test.py --output-directory
#   ~/offline_eval_short_output/ --user-requested -use-test-scheduler
#   $CHROME_SRC/out/Default ~/offline_eval_urls.txt
# 4. Check the results in the output directory.

import argparse
import os
import re
import shutil
import subprocess
import sys
import urlparse

DEFAULT_USER_REQUEST = True
DEFAULT_USE_TEST_SCHEDULER = True
# 0 means the batch would be the whole list of urls.
DEFAULT_BATCH_SIZE = 0
DEFAULT_VERBOSE = False
DEFAULT_TEST_CMD = 'OfflinePageSavePageLaterEvaluationTest.testFailureRate'
CONFIG_FILENAME = 'test_config'
CONFIG_TEMPLATE = """\
IsUserRequested = {is_user_requested}
UseTestScheduler = {use_test_scheduler}
ScheduleBatchSize = {schedule_batch_size}
"""


def main(args):
  # Setting up the argument parser.
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--output-directory',
      dest='output_dir',
      help='Directory for output. Default is ~/offline_eval_output/')
  parser.add_argument(
      '--user-requested',
      dest='user_request',
      action='store_true',
      help='Testing as user-requested urls. Default option.')
  parser.add_argument(
      '--not-user-requested',
      dest='user_request',
      action='store_false',
      help='Testing as not user-requested urls.')
  parser.add_argument(
      '--use-test-scheduler',
      dest='use_test_scheduler',
      action='store_true',
      help='Use test scheduler to avoid real scheduling. Default option.')
  parser.add_argument(
      '--not-use-test-scheduler',
      dest='use_test_scheduler',
      action='store_false',
      help='Use GCMNetworkManager for scheduling.')
  parser.add_argument(
      '--batch-size',
      type=int,
      dest='schedule_batch_size',
      help='Number of pages to be queued after previous batch completes.')
  parser.add_argument(
      '-v',
      '--verbose',
      dest='verbose',
      action='store_true',
      help='Make test runner verbose.')
  parser.add_argument(
      '-d',
      '--device',
      type=str,
      dest='device_id',
      help='Specify which device to be used. See \'adb devices\'.')
  parser.add_argument('build_output_dir', help='Path to build directory.')
  parser.add_argument(
      'test_urls_file', help='Path to input file with urls to be tested.')
  parser.set_defaults(
      output_dir=os.path.expanduser('~/offline_eval_output'),
      user_request=DEFAULT_USER_REQUEST,
      use_test_scheduler=DEFAULT_USE_TEST_SCHEDULER,
      schedule_batch_size=DEFAULT_BATCH_SIZE,
      verbose=DEFAULT_VERBOSE)

  # Get the arguments and several paths.
  options, extra_args = parser.parse_known_args(args)

  if extra_args:
    print 'Unknown args: ' + ', '.join(
        extra_args) + '. Please check and run again.'
    return

  build_dir_path = os.path.abspath(
      os.path.join(os.getcwd(), options.build_output_dir))
  test_runner_path = os.path.join(build_dir_path,
                                  'bin/run_chrome_public_test_apk')
  config_output_path = os.path.join(options.output_dir, CONFIG_FILENAME)

  def get_adb_command(args):
    adb_path = os.path.join(
        build_dir_path,
        '../../third_party/android_sdk/public/platform-tools/adb')
    if options.device_id != None:
      return [adb_path, '-s', options.device_id] + args
    return [adb_path] + args

  # In case adb server is not started
  subprocess.call(get_adb_command(['start-server']))
  external_dir = subprocess.check_output(
      get_adb_command(['shell', 'echo', '$EXTERNAL_STORAGE'])).strip()

  # Create the output directory for results, and have a copy of test config
  # there.
  if not os.path.exists(options.output_dir):
    print 'Creating output directory for results... ' + options.output_dir
    os.makedirs(options.output_dir)
  with open(config_output_path, 'w') as config:
    config.write(
        CONFIG_TEMPLATE.format(
            is_user_requested=options.user_request,
            use_test_scheduler=options.use_test_scheduler,
            schedule_batch_size=options.schedule_batch_size))

  print 'Uploading config file and input file onto the device.'
  subprocess.call(
      get_adb_command(
          ['push', config_output_path, external_dir + '/paquete/test_config']))
  subprocess.call(
      get_adb_command([
          'push', options.test_urls_file, external_dir +
          '/paquete/offline_eval_urls.txt'
      ]))
  print 'Start running test with following configurations:'
  print CONFIG_TEMPLATE.format(
      is_user_requested=options.user_request,
      use_test_scheduler=options.use_test_scheduler,
      schedule_batch_size=options.schedule_batch_size)
  # Run test with timeout-scale as 20.0 and strict mode off.
  # This scale is only applied to timeouts which are defined as scalable ones
  # in the test framework (like the timeout used to decide if Chrome doesn't
  # start properly), on svelte devices we would hit the 'no tab selected'
  # assertion since the starting time is longer than expected by the framework.
  # So we're setting the scale to 20. It will not affect the annotation-based
  # timeouts.
  # Also turning off the strict mode so that we won't run into StrictMode
  # violations when writing to files.
  test_runner_cmd = [
      test_runner_path,
      '--timeout-scale',
      '20.0',
      '--strict-mode',
      'off',
  ]
  if options.verbose:
    test_runner_cmd += ['-v']
  if options.device_id != None:
    test_runner_cmd += ['-d', options.device_id]

  test_runner_cmd += ['-f', DEFAULT_TEST_CMD]
  subprocess.call(test_runner_cmd)

  print 'Fetching results from device...'
  archive_dir = os.path.join(options.output_dir, 'archives/')
  if os.path.exists(archive_dir):
    shutil.rmtree(archive_dir)
  subprocess.call(
      get_adb_command(['pull', external_dir + '/paquete/archives', archive_dir
                      ]))
  subprocess.call(
      get_adb_command([
          'pull', external_dir + '/paquete/offline_eval_results.txt',
          options.output_dir
      ]))
  subprocess.call(
      get_adb_command([
          'pull', external_dir + '/paquete/offline_eval_logs.txt',
          options.output_dir
      ]))
  print 'Test finished!'

  print 'Renaming archive files with host names.'
  pattern = 'Content-Location: (.*)'
  for filename in os.listdir(archive_dir):
    path = os.path.join(archive_dir, filename)
    with open(path) as f:
      content = f.read()
    result = re.search(pattern, content)
    if (result == None):
      continue
    url = result.group(1)
    url_parse = urlparse.urlparse(url)
    hostname = url_parse[1].replace('.', '_')
    url_path = re.sub('[^0-9a-zA-Z]+', '_', url_parse[2][1:])

    if (len(hostname) == 0):
      hostname = 'error_parsing_hostname'
      continue
    newname = hostname + '-' + url_path
    newpath = os.path.join(archive_dir, newname + '.mhtml')
    os.rename(path, newpath)
  print 'Renaming finished.'


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
