#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility for reading / writing command-line flag files on device(s)."""


import argparse
import logging
import sys

import devil_chromium

from devil.android import device_errors
from devil.android import device_utils
from devil.android import flag_changer
from devil.android.tools import script_common
from devil.utils import cmd_helper
from devil.utils import logging_common


def CheckBuildTypeSupportsFlags(device, command_line_flags_file):
  is_webview = command_line_flags_file == 'webview-command-line'
  if device.IsUserBuild() and is_webview:
    raise device_errors.CommandFailedError(
        'WebView only respects flags on a userdebug or eng device, yours '
        'is a user build.', device)
  if device.IsUserBuild():
    logging.warning(
        'Your device (%s) is a user build; Chrome may or may not pick up '
        'your commandline flags. Check your '
        '"command_line_on_non_rooted_enabled" preference, or switch '
        'devices.', device)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.usage = '''%(prog)s --name FILENAME [--device SERIAL] [flags...]

No flags: Prints existing command-line file.
Empty string: Deletes command-line file.
Otherwise: Writes command-line file.

'''
  parser.add_argument('--name', required=True,
                      help='Name of file where to store flags on the device.')
  parser.add_argument('-e', '--executable', dest='executable', default='chrome',
                      help='(deprecated) No longer used.')
  script_common.AddEnvironmentArguments(parser)
  script_common.AddDeviceArguments(parser)
  logging_common.AddLoggingArguments(parser)

  args, remote_args = parser.parse_known_args()
  devil_chromium.Initialize(adb_path=args.adb_path)
  logging_common.InitializeLogging(args)

  devices = device_utils.DeviceUtils.HealthyDevices(device_arg=args.devices,
                                                    default_retries=0)
  all_devices = device_utils.DeviceUtils.parallel(devices)

  if not remote_args:
    # No args == do not update, just print flags.
    remote_args = None
    action = ''
  elif len(remote_args) == 1 and not remote_args[0]:
    # Single empty string arg == delete flags
    remote_args = []
    action = 'Deleted command line file. '
  else:
    if remote_args[0] == '--':
      remote_args.pop(0)
    action = 'Wrote command line file. '

  def update_flags(device):
    CheckBuildTypeSupportsFlags(device, args.name)
    changer = flag_changer.FlagChanger(device, args.name)
    if remote_args is not None:
      flags = changer.ReplaceFlags(remote_args)
    else:
      flags = changer.GetCurrentFlags()
    return (device, device.build_description, flags)

  updated_values = all_devices.pMap(update_flags).pGet(None)

  print('%sCurrent flags (in %s):' % (action, args.name))
  for d, desc, flags in updated_values:
    if flags:
      # Shell-quote flags for easy copy/paste as new args on the terminal.
      quoted_flags = ' '.join(cmd_helper.SingleQuote(f) for f in sorted(flags))
    else:
      quoted_flags = '( empty )'
    print('  %s (%s): %s' % (d, desc, quoted_flags))

  return 0


if __name__ == '__main__':
  sys.exit(main())
