#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A helper script to list class verification errors.

This is a wrapper around the device's oatdump executable, parsing desired output
and accommodating API-level-specific details, such as file paths.
"""



import argparse
import dataclasses  # pylint: disable=wrong-import-order
import logging
import os
import re

import devil_chromium
from devil.android import device_errors
from devil.android import device_temp_file
from devil.android import device_utils
from devil.android.ndk import abis
from devil.android.sdk import version_codes
from devil.android.tools import script_common
from devil.utils import logging_common
from py_utils import tempfile_ext

STATUSES = [
    'NotReady',
    'RetryVerificationAtRuntime',
    'Verified',
    'Initialized',
    'SuperclassValidated',
]


def DetermineDeviceToUse(devices):
  """Like DeviceUtils.HealthyDevices(), but only allow a single device.

  Args:
    devices: A (possibly empty) list of serial numbers, such as from the
        --device flag.
  Returns:
    A single device_utils.DeviceUtils instance.
  Raises:
    device_errors.NoDevicesError: Raised when no non-denylisted devices exist.
    device_errors.MultipleDevicesError: Raise when multiple devices exist, but
        |devices| does not distinguish which to use.
  """
  if not devices:
    # If the user did not specify which device, we let HealthyDevices raise
    # MultipleDevicesError.
    devices = None
  usable_devices = device_utils.DeviceUtils.HealthyDevices(device_arg=devices)
  # If the user specified more than one device, we still only want to support a
  # single device, so we explicitly raise MultipleDevicesError.
  if len(usable_devices) > 1:
    raise device_errors.MultipleDevicesError(usable_devices)
  return usable_devices[0]


class DeviceOSError(Exception):
  """Raised when a file is missing from the device, or something similar."""


class UnsupportedDeviceError(Exception):
  """Raised when the device is not supported by this script."""


def _GetFormattedArch(device):
  abi = device.product_cpu_abi
  # Some architectures don't map 1:1 with the folder names.
  return {abis.ARM_64: 'arm64', abis.ARM: 'arm'}.get(abi, abi)


def FindOdexFiles(device, package_name):
  """Gets the full paths to the dex files on the device."""
  sdk_level = device.build_version_sdk
  paths_to_apk = device.GetApplicationPaths(package_name)
  if not paths_to_apk:
    raise DeviceOSError(
        'Could not find data directory for {}. Is it installed?'.format(
            package_name))

  ret = []
  for path_to_apk in paths_to_apk:
    if version_codes.LOLLIPOP <= sdk_level <= version_codes.LOLLIPOP_MR1:
      # Of the form "com.example.foo-\d", where \d is a digit (usually 1 or 2).
      package_with_suffix = os.path.basename(os.path.dirname(path_to_apk))
      arch = _GetFormattedArch(device)
      dalvik_prefix = '/data/dalvik-cache/{arch}'.format(arch=arch)
      odex_file = '{prefix}/data@app@{package}@base.apk@classes.dex'.format(
          prefix=dalvik_prefix, package=package_with_suffix)
    elif sdk_level >= version_codes.MARSHMALLOW:
      arch = _GetFormattedArch(device)
      odex_file = '{data_dir}/oat/{arch}/base.odex'.format(
          data_dir=os.path.dirname(path_to_apk), arch=arch)
    else:
      raise UnsupportedDeviceError(
          'Unsupported API level: {}'.format(sdk_level))

    odex_file_exists = device.FileExists(odex_file)
    if odex_file_exists:
      ret.append(odex_file)
    elif sdk_level >= version_codes.PIE:
      raise DeviceOSError(
          'Unable to find odex file: you must run dex2oat on debuggable apps '
          'on >= P after installation.')
    else:
      raise DeviceOSError('Unable to find odex file ' + odex_file)
  return ret


def _AdbOatDump(device, odex_file, out_file):
  """Runs oatdump on the device."""
  # Get the path to the odex file.
  with device_temp_file.DeviceTempFile(device.adb) as device_file:
    device.RunShellCommand(
        ['oatdump', '--oat-file=' + odex_file, '--output=' + device_file.name],
        timeout=420,
        shell=True,
        check_return=True)
    device.PullFile(device_file.name, out_file, timeout=220)


@dataclasses.dataclass(order=True, frozen=True)
class JavaClass:
  """This represents a Java Class and its ART Class Verification status."""
  name: str
  verification_status: str


def _ParseMappingFile(proguard_map_file):
  """Creates a map of obfuscated names to deobfuscated names."""
  mappings = {}
  with open(proguard_map_file, 'r') as f:
    pattern = re.compile(r'^(\S+) -> (\S+):')
    for line in f:
      m = pattern.match(line)
      if m is not None:
        deobfuscated_name = m.group(1)
        obfuscated_name = m.group(2)
        mappings[obfuscated_name] = deobfuscated_name
  return mappings


def _DeobfuscateJavaClassName(dex_code_name, proguard_mappings):
  return proguard_mappings.get(dex_code_name, dex_code_name)


def FormatJavaClassName(dex_code_name, proguard_mappings):
  obfuscated_name = dex_code_name.replace('/', '.')
  if proguard_mappings is not None:
    return _DeobfuscateJavaClassName(obfuscated_name, proguard_mappings)
  return obfuscated_name


def ParseOatdump(oatdump_output, proguard_mappings):
  """Lists all Java classes in the dex along with verification status."""
  java_classes = []
  pattern = re.compile(r'\d+: L([^;]+).*\(type_idx=[^(]+\((\w+)\).*')
  for line in oatdump_output:
    m = pattern.match(line)
    if m is not None:
      name = FormatJavaClassName(m.group(1), proguard_mappings)
      # Some platform levels prefix this with "Status" while other levels do
      # not. Strip this for consistency.
      verification_status = m.group(2).replace('Status', '')
      java_classes.append(JavaClass(name, verification_status))
  return java_classes


def _PrintVerificationResults(target_status, java_classes, show_summary):
  """Prints results for user output."""
  # Sort to keep output consistent between runs.
  java_classes.sort(key=lambda c: c.name)
  d = {}
  for status in STATUSES:
    d[status] = 0

  for java_class in java_classes:
    if java_class.verification_status == target_status:
      print(java_class.name)
    if java_class.verification_status not in d:
      raise RuntimeError('Unexpected status: {0}'.format(
          java_class.verification_status))
    d[java_class.verification_status] += 1

  if show_summary:
    for status in d:
      count = d[status]
      print('Total {status} classes: {num}'.format(
          status=status, num=count))
    print('Total number of classes: {num}'.format(
        num=len(java_classes)))


def RealMain(mapping, device_arg, package, status, hide_summary, workdir):
  if mapping is None:
    logging.warning('Skipping deobfuscation because no map file was provided.')
    proguard_mappings = None
  else:
    proguard_mappings = _ParseMappingFile(mapping)
  device = DetermineDeviceToUse(device_arg)
  host_tempfile = os.path.join(workdir, 'out.dump')
  device.EnableRoot()
  odex_files = FindOdexFiles(device, package)
  java_classes = set()
  for odex_file in odex_files:
    _AdbOatDump(device, odex_file, host_tempfile)
    with open(host_tempfile, 'r') as f:
      java_classes.update(ParseOatdump(f, proguard_mappings))
  _PrintVerificationResults(status, sorted(java_classes), not hide_summary)


def main():
  parser = argparse.ArgumentParser(description="""
List Java classes in an APK which fail ART class verification.
""")
  parser.add_argument(
      '--package',
      '-P',
      type=str,
      default=None,
      required=True,
      help='Specify the full application package name')
  parser.add_argument(
      '--mapping',
      '-m',
      type=os.path.realpath,
      default=None,
      help='Mapping file for the desired APK to deobfuscate class names')
  parser.add_argument(
      '--hide-summary',
      default=False,
      action='store_true',
      help='Do not output the total number of classes in each Status.')
  parser.add_argument(
      '--status',
      type=str,
      default='RetryVerificationAtRuntime',
      choices=STATUSES,
      help='Which category of classes to list at the end of the script')
  parser.add_argument(
      '--workdir',
      '-w',
      type=os.path.realpath,
      default=None,
      help=('Work directory for oatdump output (default = temporary '
            'directory). If specified, this will not be cleaned up at the end '
            'of the script (useful if you want to inspect oatdump output '
            'manually)'))

  script_common.AddEnvironmentArguments(parser)
  script_common.AddDeviceArguments(parser)
  logging_common.AddLoggingArguments(parser)

  args = parser.parse_args()
  devil_chromium.Initialize(adb_path=args.adb_path)
  logging_common.InitializeLogging(args)

  if args.workdir:
    if not os.path.isdir(args.workdir):
      raise RuntimeError('Specified working directory does not exist')
    RealMain(args.mapping, args.devices, args.package, args.status,
             args.hide_summary, args.workdir)
    # Assume the user wants the workdir to persist (useful for debugging).
    logging.warning('Not cleaning up explicitly-specified workdir: %s',
                    args.workdir)
  else:
    with tempfile_ext.NamedTemporaryDirectory() as workdir:
      RealMain(args.mapping, args.devices, args.package, args.status,
               args.hide_summary, workdir)


if __name__ == '__main__':
  main()
