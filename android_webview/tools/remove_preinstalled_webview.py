#!/usr/bin/env python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Removes the preinstalled WebView on a device to avoid signature mismatches.

This should only be used by developers. This script will fail on actual user
devices (and this configuration is not recommended for user devices).

The recommended development configuration for Googlers is to satisfy all of the
below:
  1. The device has a Google-provided image.
  2. The device does not have an image based on AOSP.
  3. Set `use_signing_keys = true` in GN args.

If any of the above are not satisfied (or if you're external to Google), you can
use this script to remove the system-image WebView on your device, which will
allow you to install a local WebView build without triggering signature
mismatches (which would otherwise block installing the APK).

After running this script, you should be able to build and install
system_webview_apk.
  * If your device does *not* have an AOSP-based image, you will need to set
    `system_webview_package_name = "com.google.android.webview"` in GN args.
"""

from __future__ import print_function

import argparse
import logging
import os
import sys


sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, 'build', 'android'))
import devil_chromium  # pylint: disable=import-error
from devil.android import device_utils  # pylint: disable=import-error
from devil.android.sdk import keyevent  # pylint: disable=import-error
from devil.android.sdk import version_codes  # pylint: disable=import-error
from devil.android.tools import script_common  # pylint: disable=import-error
from devil.android.tools import system_app  # pylint: disable=import-error


WEBVIEW_SYSTEM_IMAGE_PATHS = ['/system/app/webview',
                              '/system/app/WebViewGoogle',
                              '/system/app/WebViewStub']
WEBVIEW_PACKAGES = ['com.android.webview', 'com.google.android.webview']


def _UnlockDevice(device):
  device.SendKeyEvent(keyevent.KEYCODE_MENU)


def UninstallWebViewSystemImages(device):
  """Uninstalls system images for known WebView packages."""
  print('Removing system images from %s...' % device.serial)
  system_app.RemoveSystemApps(device, WEBVIEW_PACKAGES)
  _UnlockDevice(device)


def UninstallWebViewUpdates(device):
  """Uninstalls updates for WebView packages, if updates exist."""
  print('Uninstalling updates from %s...' % device.serial)
  for webview_package in WEBVIEW_PACKAGES:
    paths = device.GetApplicationPaths(webview_package)
    if not paths:
      return  # Package isn't installed, nothing to do
    if set(paths) <= set(WEBVIEW_SYSTEM_IMAGE_PATHS):
      # If we only have preinstalled paths, don't try to uninstall updates
      # (necessary, otherwise we will raise an exception on some devices).
      return
    device.Uninstall(webview_package)


def AllowStandaloneWebView(device):
  if device.build_version_sdk < version_codes.NOUGAT:
    return
  allow_standalone_webview = ['cmd', 'webviewupdate',
                              'enable-redundant-packages']
  device.RunShellCommand(allow_standalone_webview, check_return=True)


def RemovePreinstalledWebViews(device):
  device.EnableRoot()
  UninstallWebViewUpdates(device)
  UninstallWebViewSystemImages(device)
  AllowStandaloneWebView(device)


def main():
  parser = argparse.ArgumentParser(description="""
Removes the preinstalled WebView APKs to avoid signature mismatches during
development.
""")

  parser.add_argument('--verbose', '-v', default=False, action='store_true')
  parser.add_argument('--quiet', '-q', default=False, action='store_true')
  script_common.AddEnvironmentArguments(parser)
  script_common.AddDeviceArguments(parser)

  args = parser.parse_args()
  if args.verbose:
    logging.basicConfig(stream=sys.stderr, level=logging.INFO)
  elif args.quiet:
    logging.basicConfig(stream=sys.stderr, level=logging.ERROR)
  else:
    logging.basicConfig(stream=sys.stderr, level=logging.WARN)

  devil_chromium.Initialize()
  script_common.InitializeEnvironment(args)

  devices = device_utils.DeviceUtils.HealthyDevices(device_arg=args.devices)
  device_utils.DeviceUtils.parallel(devices).pMap(RemovePreinstalledWebViews)


if __name__ == '__main__':
  sys.exit(main())
