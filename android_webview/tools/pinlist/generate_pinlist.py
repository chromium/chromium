#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a pinlist for libwebviewchromium.so by analyzing its memory
residency."""

import argparse
import logging
import pathlib
import sys
import re
import time
import subprocess
import os

_SRC_PATH = pathlib.Path(__file__).resolve().parents[3]
sys.path.append(str(_SRC_PATH / 'third_party/catapult/devil'))
from devil.android import device_utils  # pylint: disable=wrong-import-position

_MAX_INITIAL_RESIDENCY_PERCENTAGE = 3.0


def _GetWebViewProviderPackage(device):
  """Returns the package name of the currently set WebView provider."""
  package_name = device.GetWebViewProvider()
  logging.info('Current WebView provider package: %s', package_name)
  return package_name


def _GetWebViewProviderApkPath(device, package_name):
  """Returns the APK path of the given WebView provider package."""
  apk_paths = device.GetApplicationPaths(package_name)
  if not apk_paths:
    raise RuntimeError(f'Could not find APK paths for {package_name}')
  logging.info('APK path for %s: %s', package_name, apk_paths[0])
  return apk_paths[0]


def _GetLibWebViewChromiumResidency(device, apk_path):
  """Runs pintool and parses the residency for libwebviewchromium.so."""
  pintool_cmd = ['pintool', 'file', apk_path, '--zip', '--gen-probe', '--dump']
  logging.info('Running pintool command: %s', ' '.join(pintool_cmd))
  output = device.RunShellCommand(pintool_cmd, as_root=True, large_output=True)

  in_webview_section = False
  webview_total_size = 0
  webview_resident_size = 0
  resident_ranges = []
  for line in output:
    if 'lib/arm64-v8a/libwebviewchromium.so' in line:
      in_webview_section = True
      match = re.search(r'size\(B\)=(\d+) resident\(B\)=(\d+)', line)
      if match:
        webview_total_size = int(match.group(1))
        webview_resident_size = int(match.group(2))
    elif in_webview_section:
      if line.startswith('zip_offset='):
        # pintool dump output for resident ranges looks like:
        # zip_offset=14303232 file_offset=0 total_bytes=106496
        resident_ranges.append({
            k: int(v)
            for k, v in (p.split('=') for p in line.split())
        })
      elif 'size(B)=' in line:
        # Reached the next file section
        break

  percentage_resident = 0.0
  if webview_total_size > 0:
    percentage_resident = (webview_resident_size / webview_total_size) * 100
    logging.info(
        'Libwebviewchromium.so: %d resident ranges, '
        'Total size: %.2f MB, '
        'Resident size: %.2f MB, '
        'Resident percentage: %.2f%%', len(resident_ranges),
        webview_total_size / (1024 * 1024),
        webview_resident_size / (1024 * 1024), percentage_resident)
  else:
    logging.info('Found %d resident ranges for libwebviewchromium.so',
                 len(resident_ranges))
  return resident_ranges, percentage_resident


def _ClearPageCache(device):
  """Clears the page cache on the device."""
  logging.info('Clearing cache and killing background apps...')
  cmd = "am kill-all && su 0 sh -c 'echo 3 > /proc/sys/vm/drop_caches'"
  device.RunShellCommand(cmd, shell=True, check_return=True)


def _InstallWebView(build_dir, build_target):
  """Installs and sets the WebView provider."""
  logging.info('Using build directory: %s', build_dir)

  install_script = build_dir / 'bin' / build_target
  if not install_script.exists():
    raise FileNotFoundError(f'Installer script not found at {install_script}. '
                            f'Did you build {build_target}?')

  logging.info('Installing %s...', build_target)
  subprocess.run([str(install_script), 'install'], check=True)
  logging.info('Setting WebView provider...')
  subprocess.run([str(install_script), 'set-webview-provider'], check=True)
  logging.info('WebView provider set successfully.')

  shell_build_target = 'system_webview_shell_apk'
  shell_install_script = build_dir / 'bin' / shell_build_target
  if not shell_install_script.exists():
    raise FileNotFoundError(
        f'Installer script not found at {shell_install_script}. '
        'Did you build system_webview_shell_apk?')

  logging.info('Installing %s...', shell_build_target)
  subprocess.run([str(shell_install_script), 'install'], check=True)
  logging.info('WebView shell installed successfully.')


def _DisableWebViewMemoryPinning(device):
  """Disables WebView memory pinning."""
  logging.info('Disabling WebView memory pinning...')
  device.RunShellCommand(['setprop', 'pinner.pin_webview_size', '0'],
                         as_root=True,
                         check_return=True)


def _DisableDiskReadAhead(device):
  """Disables disk read-ahead for the /data partition."""
  logging.info('Disabling disk read-ahead for /data partition...')
  output = device.RunShellCommand(['df', '/data'], check_return=True)
  if len(output) < 2:
    raise RuntimeError(f'Unexpected "df /data" output: {output}')

  # The second line of output contains the filesystem info.
  # e.g., /dev/block/dm-57...
  block_device_path = output[1].split()[0]
  if not block_device_path.startswith('/dev/block/'):
    raise RuntimeError(f'Unexpected block device path: {block_device_path}')

  block_device_name = os.path.basename(block_device_path)
  read_ahead_path = f'/sys/block/{block_device_name}/queue/read_ahead_kb'

  if not device.PathExists(read_ahead_path, as_root=True):
    logging.warning('Could not find %s. Skipping disable read-ahead.',
                    read_ahead_path)
    return

  logging.info(
      'Attempting to disable disk read-ahead by running: '
      'echo 1 > %s (as root)', read_ahead_path)
  cmd = f'echo 1 > {read_ahead_path}'
  device.RunShellCommand(cmd, shell=True, as_root=True, check_return=True)
  content = device.ReadFile(read_ahead_path, as_root=True).strip()
  if int(content) <= 1:
    logging.info('Disk read-ahead disabled successfully (set to %s).', content)
  else:
    logging.warning('Failed to disable disk read-ahead. Current value is %s.',
                    content)


def _GetWebViewCommandLineFlags(device):
  """Reads the current WebView command line flags from the device."""
  flags_path = '/data/local/tmp/webview-command-line'
  if device.PathExists(flags_path):
    return device.ReadFile(flags_path)
  return None


def _RestoreWebViewCommandLineFlags(device, original_content):
  """Restores the WebView command line flags to their original state."""
  logging.info('Restoring WebView command line flags...')
  flags_path = '/data/local/tmp/webview-command-line'
  if original_content is None:
    if device.PathExists(flags_path):
      device.RemovePath(flags_path)
  else:
    device.WriteFile(flags_path, original_content)
  logging.info('WebView command line flags restored.')


def _SetWebViewCommandLineFlags():
  """Sets WebView command line flags."""
  logging.info('Setting WebView command line flags...')
  cmd_script_path = _SRC_PATH / 'build/android/adb_system_webview_command_line'
  flags = ['--disable-features=WebViewPrefetchNativeLibrary']
  subprocess.run([str(cmd_script_path)] + flags, check=True)
  logging.info('WebView command line flags set successfully.')


def _InitialSetup(device):
  """Performs initial device setup."""
  _DisableWebViewMemoryPinning(device)
  _DisableDiskReadAhead(device)
  _SetWebViewCommandLineFlags()

  logging.info('Restarting Android framework...')
  # The 'stop' command will terminate the shell, so we don't check for success.
  device.RunShellCommand(['stop'], as_root=True, check_return=False)

  # Give the device a moment to process 'stop' before 'start'.
  time.sleep(2)

  # 'start' should bring the UI back up.
  device.RunShellCommand(['start'], as_root=True, check_return=False)

  # Wait for the device to be fully booted and responsive again.
  device.WaitUntilFullyBooted()
  logging.info('Framework restarted and device ready.')

  logging.info('Waiting 5 seconds for lock screen to be ready...')
  time.sleep(5)
  logging.info('Unlocking screen...')
  device.RunShellCommand(['input', 'keyevent', 'KEYCODE_MENU'],
                         check_return=True)


def _EnsureLowInitialResidency(device, webview_apk_path, max_retries=10):
  """Ensures that the initial residency of libwebviewchromium.so
  is below a threshold."""
  logging.info(
      'Ensuring initial residency of libwebviewchromium.so is < '
      '%.2f%%...', _MAX_INITIAL_RESIDENCY_PERCENTAGE)
  for i in range(max_retries):
    _ClearPageCache(device)
    _, percentage_resident = _GetLibWebViewChromiumResidency(
        device, webview_apk_path)
    if percentage_resident < _MAX_INITIAL_RESIDENCY_PERCENTAGE:
      logging.info('Initial residency is %.2f%%, which is acceptable.',
                   percentage_resident)
      return
    logging.warning('Initial residency is %.2f%%, trying again (%d/%d)...',
                    percentage_resident, i + 1, max_retries)
  raise RuntimeError(
      'Failed to achieve low initial residency after multiple retries.')


def _RunStartupWorkloadAndGetResidency(device, webview_apk_path):
  """Runs the startup workload and collects residency."""
  logging.info('Running startup workload...')
  cmd = [
      'am', 'start', '-n', 'org.chromium.webview_shell/.StartupTimeActivity',
      '-a', 'android.intent.action.VIEW', '--ei', 'target', '5'
  ]
  device.RunShellCommand(cmd, check_return=True)

  logging.info('Waiting 5 seconds for workload to complete...')
  time.sleep(5)

  logging.info('Collecting residency after startup workload...')
  return _GetLibWebViewChromiumResidency(device, webview_apk_path)


def _GeneratePinlist(device, apk_path, resident_ranges, out_dir):
  """Generates the pinlist from the resident ranges."""
  pinconfig_content = 'file lib/arm64-v8a/libwebviewchromium.so\n'
  for r in resident_ranges:
    pinconfig_content += f"offset {r['file_offset']}\n"
    pinconfig_content += f"len {r['total_bytes']}\n"

  pinconfig_host_path = out_dir / 'pinconfig.txt'
  with open(pinconfig_host_path, 'w') as f:
    f.write(pinconfig_content)
  pinconfig_device_path = '/data/local/tmp/pinconfig.txt'
  pinlist_device_path = '/data/local/tmp/pinlist.meta'
  logging.info('Pushing %s to %s', pinconfig_host_path, pinconfig_device_path)
  device.adb.Push(str(pinconfig_host_path), pinconfig_device_path)
  logging.info('Generating pinlist...')
  pintool_cmd = [
      'pintool', 'file', apk_path, '--zip', '--pinconfig',
      pinconfig_device_path, '--dump', '-o', pinlist_device_path
  ]
  logging.info('Running pintool command: %s', ' '.join(pintool_cmd))
  device.RunShellCommand(pintool_cmd, as_root=True, check_return=True)
  pinlist_host_path = out_dir / 'pinlist.meta'
  logging.info('Pulling %s to %s', pinlist_device_path, pinlist_host_path)
  device.adb.Pull(pinlist_device_path, str(pinlist_host_path))
  logging.info('Pinlist generated at %s', pinlist_host_path)

  pinlist_target_dir = _SRC_PATH / 'android_webview/pinlist'
  pinlist_target_dir.mkdir(parents=True, exist_ok=True)
  target_pinconfig_path = pinlist_target_dir / 'pinconfig.txt'
  target_pinlist_path = pinlist_target_dir / 'pinlist.meta'
  subprocess.run(
      ['cp', str(pinconfig_host_path),
       str(target_pinconfig_path)], check=True)
  logging.info('Copied %s to %s', pinconfig_host_path, target_pinconfig_path)
  subprocess.run(['cp', str(pinlist_host_path),
                  str(target_pinlist_path)],
                 check=True)
  logging.info('Copied %s to %s', pinlist_host_path, target_pinlist_path)

  logging.info('Cleaning up temporary files from device...')
  device.RemovePath(pinconfig_device_path, as_root=True)
  device.RemovePath(pinlist_device_path, as_root=True)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-v',
                      '--verbose',
                      action='count',
                      default=0,
                      help='Increase verbosity level (repeat as needed)')
  parser.add_argument('-C',
                      '--builddir',
                      type=pathlib.Path,
                      required=True,
                      help='Path to the build directory.')
  parser.add_argument(
      '--webview-build-target',
      default='system_webview_google_64_32_bundle',
      help='The WebView installer/bundle target to use. (default: %(default)s)')
  parser.add_argument('--outputdir',
                      type=pathlib.Path,
                      help='Path to store final outputs, default is builddir.')
  parser.add_argument('--isolated-script-test-output',
                      type=pathlib.Path,
                      help='Output.json file that the script can write to.')

  options = parser.parse_args()

  if options.verbose >= 2:
    level = logging.DEBUG
  elif options.verbose == 1:
    level = logging.INFO
  else:
    level = logging.WARNING
  logging.basicConfig(level=level,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  options.builddir = options.builddir.resolve()

  if options.isolated_script_test_output:
    options.isolated_script_test_output = (
        options.isolated_script_test_output.resolve())
    options.outputdir = options.isolated_script_test_output.parent
  elif options.outputdir:
    options.outputdir = options.outputdir.resolve()
  else:
    options.outputdir = options.builddir

  logging.info('Output directory: %s', options.outputdir)

  devices = device_utils.DeviceUtils.HealthyDevices()
  assert devices, 'Expected at least one connected device'
  device = devices[0]

  _InstallWebView(options.builddir, options.webview_build_target)

  original_flags = _GetWebViewCommandLineFlags(device)

  try:
    _InitialSetup(device)
    webview_provider_package = _GetWebViewProviderPackage(device)
    webview_apk_path = _GetWebViewProviderApkPath(device,
                                                  webview_provider_package)
    _EnsureLowInitialResidency(device, webview_apk_path)
    resident_ranges, _ = _RunStartupWorkloadAndGetResidency(
        device, webview_apk_path)
    _GeneratePinlist(device, webview_apk_path, resident_ranges,
                     options.outputdir)
  finally:
    _RestoreWebViewCommandLineFlags(device, original_flags)
    logging.info('Rebooting device to restore system state...')
    device.Reboot()


if __name__ == '__main__':
  main()
