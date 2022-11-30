#!/usr/bin/env vpython3
#
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Takes a netlog for the WebViews in a given application.

Developer guide:
https://chromium.googlesource.com/chromium/src/+/HEAD/android_webview/docs/net-debugging.md
"""

from __future__ import print_function

import argparse
import logging
import os
import posixpath
import re
import sys
import time

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir, 'build', 'android'))
# pylint: disable=wrong-import-position,import-error
import devil_chromium
from devil.android import device_errors
from devil.android import flag_changer
from devil.android import device_utils
from devil.android.tools import script_common
from devil.utils import logging_common

WEBVIEW_COMMAND_LINE = 'webview-command-line'


def _WaitUntilCtrlC():
  try:
    while True:
      time.sleep(1)
  except KeyboardInterrupt:
    print()  # print a new line after the "^C" the user typed to the console


def CheckAppNotRunning(device, package_name, force):
  is_running = bool(device.GetApplicationPids(package_name))
  if is_running:
    msg = ('Netlog requires setting commandline flags, which only works if the '
           'application ({}) is not already running. Please kill the app and '
           'restart the script.'.format(
               package_name))
    if force:
      logging.warning(msg)
    else:
      # Extend the sentence to mention the user can skip the check.
      msg = re.sub(r'\.$', ', or pass --force to ignore this check.', msg)
      raise RuntimeError(msg)


def main():
  parser = argparse.ArgumentParser(description="""
Configures WebView to start recording a netlog. This script chooses a suitable
netlog filename for the application, and will pull the netlog off the device
when the user terminates the script (with ctrl-C). For a more complete usage
guide, open your web browser to:
https://chromium.googlesource.com/chromium/src/+/HEAD/android_webview/docs/net-debugging.md
""")
  parser.add_argument(
      '--package',
      required=True,
      type=str,
      help='Package name of the application you intend to use.')
  parser.add_argument(
      '--force',
      default=False,
      action='store_true',
      help='Suppress user checks.')

  script_common.AddEnvironmentArguments(parser)
  script_common.AddDeviceArguments(parser)
  logging_common.AddLoggingArguments(parser)

  args = parser.parse_args()
  logging_common.InitializeLogging(args)
  devil_chromium.Initialize(adb_path=args.adb_path)

  # Only use a single device, for the sake of simplicity (of implementation and
  # user experience).
  devices = device_utils.DeviceUtils.HealthyDevices(device_arg=args.devices)
  device = devices[0]
  if len(devices) > 1:
    raise device_errors.MultipleDevicesError(devices)

  if device.build_type == 'user':
    device_setup_url = ('https://chromium.googlesource.com/chromium/src/+/HEAD/'
                        'android_webview/docs/device-setup.md')
    raise RuntimeError('It appears your device is a "user" build. We only '
                       'support capturing netlog on userdebug/eng builds. See '
                       '{} to configure a development device or set up an '
                       'emulator.'.format(device_setup_url))

  package_name = args.package
  device_netlog_file_name = 'netlog.json'
  device_netlog_path = posixpath.join(
      device.GetApplicationDataDirectory(package_name), 'app_webview',
      device_netlog_file_name)

  CheckAppNotRunning(device, package_name, args.force)

  # Append to the existing flags, to allow users to experiment with other
  # features/flags enabled. The CustomCommandLineFlags will restore the original
  # flag state after the user presses 'ctrl-C'.
  changer = flag_changer.FlagChanger(device, WEBVIEW_COMMAND_LINE)
  new_flags = changer.GetCurrentFlags()
  new_flags.append('--log-net-log={}'.format(device_netlog_path))

  logging.info('Running with flags %r', new_flags)
  with flag_changer.CustomCommandLineFlags(device, WEBVIEW_COMMAND_LINE,
                                           new_flags):
    print('Netlog will start recording as soon as app starts up. Press ctrl-C '
          'to stop recording.')
    _WaitUntilCtrlC()

  host_netlog_path = 'netlog.json'
  print('Pulling netlog to "%s"' % host_netlog_path)
  # The netlog file will be under the app's uid, which the default shell doesn't
  # have permission to read (but root does). Prefer this to EnableRoot(), which
  # restarts the adb daemon.
  if device.PathExists(device_netlog_path, as_root=True):
    device.PullFile(device_netlog_path, host_netlog_path, as_root=True)
    device.RemovePath(device_netlog_path, as_root=True)
  else:
    raise RuntimeError(
        'Unable to find a netlog file in the "{}" app data directory. '
        'Did you restart and run the app?'.format(package_name))


if __name__ == '__main__':
  main()
