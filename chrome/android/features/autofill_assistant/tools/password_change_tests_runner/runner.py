#!/usr/bin/env python
#
# Copyright (c) 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The script runs password change tests present in PasswordChangeFixtureTest.

    Example Steps:
    1. Fill arguments in config file.
    2. Run command: python3 runner.py @config_example.cfg
    Note: The @ sign should be followed by the config file containing the script
          parameters. See config_example.cfg as an example.

    Please visit go/password-change-tests-runner-instructions.
"""

import argparse
import os
import subprocess
import sys

tests_list = [
    'testSingleRun',
    'testMultipleRuns',
    'testSingleRunNoCookies',
    'testInvalidCredentials',
    'testUserDeclinesGeneratedPassword',
    'testMultipleCredentials'
]


def main(args):
  parser = argparse.ArgumentParser(
      prog='runner.py',
      description="""The script is intended to run password change scripts
                                                manual integration tests.
                                                """,
      fromfile_prefix_chars='@')

  parser.add_argument(
      '--adb-path',
      dest='adb_path',
      required=True,
      help='Full path to adb command.')
  parser.add_argument(
      '--device',
      dest='device_id',
      help='Specify which device to be used. See \'adb devices\'.')
  parser.add_argument(
      '--autofill-assistant-url',
      dest='assistant_url',
      required=True,
      help='Autofill assistant url.')
  parser.add_argument(
      '--debug-socket-id',
      dest='debug_socket_id',
      default='',
      help='Debug socket id.')
  parser.add_argument(
      '--debug-bundle-id',
      dest='debug_bundle_id',
      default='',
      help='Script debug bundle id.')
  parser.add_argument(
      '--domain-url',
      dest='domain_url',
      required=True,
      help='Specify which domain URL to navigate to.')
  parser.add_argument(
      '--run-for-username',
      dest='run_for_username',
      default='',
      help='Defines which username to run the script for.')
  parser.add_argument(
      '--seed-usernames',
      dest='seed_usernames',
      default='',
      help='Usernames list for initial set of credetials.')
  parser.add_argument(
      '--seed-passwords',
      dest='seed_passwords',
      default='',
      help='Passwords list for initial set of credetials.')
  parser.add_argument(
      '--num-runs',
      dest='num_runs',
      type=int,
      default=1,
      help='Number of continuous script runs.')
  parser.add_argument(
      '--num-retries',
      dest='num_retries',
      type=int,
      default=0,
      help='Number of retries in case the test fails.')
  parser.add_argument(
      '--test',
      required=True,
      dest='test',
      choices=tests_list,
      help='Specifies which test to run.')

  # Get the arguments.
  options, extra_args = parser.parse_known_args(args)

  if extra_args:
    print('Unknown args: ' + ', '.join(extra_args) +
          '. Please check and run again.')
    return

  def get_adb_command(args):
    if options.device_id:
      return [options.adb_path, '-s', options.device_id] + args
    return [options.adb_path] + args

  # In case adb server is not started.
  with open(os.devnull, 'wb') as devnull:
    subprocess.check_call(
        get_adb_command(['start-server']), stdout=devnull, stderr=devnull)

  # Check adb device is present.
  devices = subprocess.check_output(get_adb_command(['devices'
                                                   ])).strip().splitlines()
  # The first line of `adb devices` just says "List of attached devices".
  if len(devices) == 1:
    print('adb: No emulators found')
    return

  # Check --device is provided if multiple devices are found.
  if len(devices) > 2 and not options.device_id:
    print('adb: Multiple devices found. Please provide --device.')
    return

  # Install dependencies. Apks should live in the same directory.
  apks = ['ChromePublicTest.apk', 'ChromiumNetTestSupport.apk']
  print('Installing apks')
  for apk in apks:
    if os.path.isfile(apk):
      subprocess.check_output(get_adb_command(['install', '-r', apk]))
    else:
      print('%s is not present in the folder' % apk)
      return

  packages = {
      'org.chromium.chrome.tests': [
          'ACCESS_COARSE_LOCATION', 'ACCESS_FINE_LOCATION', 'CAMERA',
          'GET_ACCOUNTS', 'READ_EXTERNAL_STORAGE', 'READ_LOGS',
          'READ_PHONE_STATE', 'RECORD_AUDIO', 'SET_ANIMATION_SCALE',
          'WRITE_EXTERNAL_STORAGE'
      ],
      'org.chromium.net.test.support': [
          'READ_EXTERNAL_STORAGE', 'READ_PHONE_STATE', 'WRITE_EXTERNAL_STORAGE'
      ]
  }

  print('Setting up permissions')
  for (package, permissions_list) in packages.items():
    for permission in permissions_list:
      subprocess.call(
          get_adb_command([
              'shell', 'pm', 'grant', package,
              'android.permission.' + permission
          ]))
  print('Setup finished')

  # Set up env variables
  # Remove previous command line file.
  subprocess.call(
      get_adb_command(
          ['shell', 'rm', '-f', '/data/local/tmp/test-cmdline-file']))

  exclude_from_arguments = ['--adb-path', '--device', '-h', '--help']

  option_objs = vars(parser)['_option_string_actions']

  # Collect all the tests related arguments.
  arguments = ' '.join([
      ('%s=%s' % (option_name, getattr(options, store_action.dest)))
      for (option_name, store_action) in option_objs.items()
      if option_name not in exclude_from_arguments
  ])

  # Push arguments into the apk command line file.
  subprocess.call(
      get_adb_command([
          'shell', 'echo', '-n', '\'_ ' + arguments + '\'', '>',
          '/data/local/tmp/test-cmdline-file'
      ]))

  print('Running test ...')
  command = get_adb_command([
      'shell', 'am', 'instrument', '-w', '-r', '-e', 'class',
      '\"org.chromium.chrome.browser.autofill_assistant.'
      'PasswordChangeFixtureTest#%s\"' % options.test,
      'org.chromium.chrome.tests/org.chromium.base.test'
      '.BaseChromiumAndroidJUnitRunner'
  ])

  subprocess.call(command)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
