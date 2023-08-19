#!/usr/bin/env vpython3
#
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple tool to run simpleperf to get sampling-based perf traces.

Typical Usage:
  android_webview/tools/run_simpleperf.py \
    --report-path report.html \
    --output-directory out/Debug/
"""

import argparse
import html
import logging
import os
import re
import subprocess
import sys

sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, 'build', 'android'))
# pylint: disable=wrong-import-position,import-error
import devil_chromium
from devil.android import apk_helper
from devil.android import device_errors
from devil.android.ndk import abis
from devil.android.tools import script_common
from devil.utils import logging_common
from py_utils import tempfile_ext

_SUPPORTED_ARCH_DICT = {
    abis.ARM: 'arm',
    abis.ARM_64: 'arm64',
    abis.X86: 'x86',
    # Note: x86_64 isn't tested yet.
}


class StackAddressInterpreter:
  """A class to interpret addresses in simpleperf using stack script."""
  def __init__(self, args, tmp_dir):
    self.args = args
    self.tmp_dir = tmp_dir

  @staticmethod
  def RunStackScript(output_dir, stack_input_path):
    """Run the stack script.

    Args:
      output_dir: The directory of Chromium output.
      stack_input_path: The path to the stack input file.

    Returns:
      The output of running the stack script (stack.py).
    """
    # Note that stack script is not designed to be used in a stand-alone way.
    # Therefore, it is better off to call it as a command line.
    # TODO(changwan): consider using llvm symbolizer directly.
    cmd = ['third_party/android_platform/development/scripts/stack',
           '--output-directory', output_dir,
           stack_input_path]
    return subprocess.check_output(cmd, universal_newlines=True).splitlines()

  @staticmethod
  def _ConvertAddressToFakeTraceLine(address, lib_path):
    formatted_address = '0x' + '0' * (16 - len(address)) + address
    # Pretend that this is Chromium's stack traces output in logcat.
    # Note that the date, time, pid, tid, frame number, and frame address
    # are all fake and they are irrelevant.
    return ('11-15 00:00:00.000 11111 11111 '
            'E chromium: #00 0x0000001111111111 %s+%s') % (
                lib_path, formatted_address)

  def Interpret(self, addresses, lib_path):
    """Interpret the given addresses.

    Args:
      addresses: A collection of addresses.
      lib_path: The path to the WebView library.

    Returns:
      A list of (address, function_info) where function_info is the function
      name, plus file name and line if args.show_file_line is set.
    """
    stack_input_path = os.path.join(self.tmp_dir, 'stack_input.txt')
    with open(stack_input_path, 'w') as f:
      for address in addresses:
        f.write(StackAddressInterpreter._ConvertAddressToFakeTraceLine(
            address, lib_path) + '\n')

    stack_output = StackAddressInterpreter.RunStackScript(
        self.args.output_directory, stack_input_path)

    if self.args.debug:
      logging.debug('First 10 lines of stack output:')
      for i in range(max(10, len(stack_output))):
        logging.debug(stack_output[i])

    logging.info('We got the results from the stack script. Translating the '
                 'addresses...')

    address_function_pairs = []
    pattern = re.compile(r'  0*(?P<address>[1-9a-f][0-9a-f]+)  (?P<function>.*)'
                         r'  (?P<file_name_line>.*)')
    for line in stack_output:
      m = pattern.match(line)
      if m:
        function_info = m.group('function')
        if self.args.show_file_line:
          function_info += " | " + m.group('file_name_line')

        address_function_pairs.append((m.group('address'), function_info))

    logging.info('The translation is done.')
    return address_function_pairs


class SimplePerfRunner:
  """A runner for simpleperf and its postprocessing."""

  def __init__(self, device, args, tmp_dir, address_interpreter):
    self.device = device
    self.address_interpreter = address_interpreter
    self.args = args
    self.apk_helper = None
    self.tmp_dir = tmp_dir

  def _GetFormattedArch(self):
    arch = _SUPPORTED_ARCH_DICT.get(
        self.device.product_cpu_abi)
    if not arch:
      raise Exception('Your device arch (' +
                      self.device.product_cpu_abi + ') is not supported.')
    logging.info('Guessing arch=%s because product.cpu.abi=%s', arch,
                 self.device.product_cpu_abi)
    return arch

  def GetWebViewLibraryNameAndPath(self, package_name):
    """Get WebView library name and path on the device."""
    apk_path = self._GetWebViewApkPath(package_name)
    logging.debug('WebView APK path: %s', apk_path)
    # TODO(changwan): check if we need support for bundle.
    tmp_apk_path = os.path.join(self.tmp_dir, 'base.apk')
    self.device.adb.Pull(apk_path, tmp_apk_path)
    self.apk_helper = apk_helper.ToHelper(tmp_apk_path)
    metadata = self.apk_helper.GetAllMetadata()
    lib_name = None
    for key, value in metadata:
      if key == 'com.android.webview.WebViewLibrary':
        lib_name = value

    lib_path = os.path.join(apk_path, 'lib', self._GetFormattedArch(), lib_name)
    logging.debug("WebView's library path on the device should be: %s",
                  lib_path)
    return lib_name, lib_path

  def Run(self):
    """Run the simpleperf and do the post processing."""
    package_name = self.GetCurrentWebViewProvider()
    SimplePerfRunner.RunPackageCompile(package_name)
    perf_data_path = os.path.join(self.tmp_dir, 'perf.data')
    SimplePerfRunner.RunSimplePerf(perf_data_path, self.args)
    lines = SimplePerfRunner.GetOriginalReportHtml(
        perf_data_path,
        os.path.join(self.tmp_dir, 'unprocessed_report.html'))
    lib_name, lib_path = self.GetWebViewLibraryNameAndPath(package_name)
    addresses = SimplePerfRunner.CollectAddresses(lines, lib_name)
    logging.info("Extracted %d addresses", len(addresses))
    address_function_pairs = self.address_interpreter.Interpret(
        addresses, lib_path)

    lines = SimplePerfRunner.ReplaceAddressesWithFunctionInfos(
        lines, address_function_pairs, lib_name)

    with open(self.args.report_path, 'w') as f:
      for line in lines:
        f.write(line + '\n')

    logging.info("The final report has been generated at '%s'.",
                 self.args.report_path)

  @staticmethod
  def RunSimplePerf(perf_data_path, args):
    """Runs the simple perf commandline."""
    cmd = [
        'third_party/android_toolchain/ndk/simpleperf/app_profiler.py',
        '--perf_data_path', perf_data_path, '--skip_collect_binaries'
    ]
    if args.system_wide:
      cmd.append('--system_wide')
    else:
      cmd.extend([
          '--app', 'org.chromium.webview_shell', '--activity',
          '.TelemetryActivity'
      ])

    if args.record_options:
      cmd.extend(['--record_options', args.record_options])

    logging.info("Profile has started.")
    subprocess.check_call(cmd)
    logging.info("Profile has finished, processing the results...")

  @staticmethod
  def RunPackageCompile(package_name):
    """Compile the package (dex optimization)."""
    cmd = [
        'adb', 'shell', 'cmd', 'package', 'compile', '-m', 'speed', '-f',
        package_name
    ]
    subprocess.check_call(cmd)

  def GetCurrentWebViewProvider(self):
    return self.device.GetWebViewUpdateServiceDump()['CurrentWebViewPackage']

  def _GetWebViewApkPath(self, package_name):
    return self.device.GetApplicationPaths(package_name)[0]

  @staticmethod
  def GetOriginalReportHtml(perf_data_path, report_html_path):
    """Gets the original report.html from running simpleperf."""
    cmd = [
        'third_party/android_toolchain/ndk/simpleperf/report_html.py',
        '--record_file', perf_data_path, '--report_path', report_html_path,
        '--no_browser'
    ]
    subprocess.check_call(cmd)
    lines = []
    with open(report_html_path, 'r') as f:
      lines = f.readlines()
    return lines

  @staticmethod
  def CollectAddresses(lines, lib_name):
    """Collect address-looking texts from lines.

    Args:
      lines: A list of strings that may contain addresses.
      lib_name: The name of the WebView library.

    Returns:
      A set containing the addresses that were found in the lines.
    """
    addresses = set()
    for line in lines:
      for address in re.findall(lib_name + r'\[\+([0-9a-f]+)\]', line):
        addresses.add(address)
    return addresses

  @staticmethod
  def ReplaceAddressesWithFunctionInfos(lines, address_function_pairs,
                                        lib_name):
    """Replaces the addresses with function names.

    Args:
      lines: A list of strings that may contain addresses.
      address_function_pairs: A list of pairs of (address, function_name).
      lib_name: The name of the WebView library.

    Returns:
      A list of strings with addresses replaced by function names.
    """

    logging.info('Replacing the HTML content with new function names...')

    # Note: Using a lenient pattern matching and a hashmap (dict) is much faster
    # than using a double loop (by the order of 1,000).
    # '+address' will be replaced by function name.
    address_function_dict = {
        '+' + k: html.escape(v, quote=False)
        for k, v in address_function_pairs
    }
    # Look behind the lib_name and '[' which will not be substituted. Note that
    # '+' is used in the pattern but will be removed.
    pattern = re.compile(r'(?<=' + lib_name + r'\[)\+([a-f0-9]+)(?=\])')

    def replace_fn(match):
      address = match.group(0)
      if address in address_function_dict:
        return address_function_dict[address]
      return address

    # Line-by-line assignment to avoid creating a temp list.
    for i, line in enumerate(lines):
      lines[i] = pattern.sub(replace_fn, line)

    logging.info('Replacing is done.')
    return lines


def main(raw_args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--debug', action='store_true',
                      help='Get additional debugging mode')
  parser.add_argument(
      '--output-directory',
      help='the path to the build output directory, such as out/Debug')
  parser.add_argument('--report-path',
                      default='report.html', help='Report path')
  parser.add_argument('--adb-path',
                      help='Absolute path to the adb binary to use.')
  parser.add_argument('--record-options',
                      help=('Set recording options for app_profiler.py command.'
                            ' Example: "-e task-clock:u -f 1000 -g --duration'
                            ' 10" where -f means sampling frequency per second.'
                            ' Try `app_profiler.py record -h` for more '
                            ' information. Note that not setting this defaults'
                            ' to the default record options.'))
  parser.add_argument('--show-file-line', action='store_true',
                      help='Show file name and lines in the result.')
  parser.add_argument(
      '--system-wide',
      action='store_true',
      help=('Whether to profile system wide (without launching'
            'an app).'))

  script_common.AddDeviceArguments(parser)
  logging_common.AddLoggingArguments(parser)

  args = parser.parse_args(raw_args)
  logging_common.InitializeLogging(args)
  devil_chromium.Initialize(adb_path=args.adb_path)

  devices = script_common.GetDevices(args.devices, args.denylist_file)
  device = devices[0]

  if len(devices) > 1:
    raise device_errors.MultipleDevicesError(devices)

  with tempfile_ext.NamedTemporaryDirectory(
      prefix='tmp_simpleperf') as tmp_dir:
    runner = SimplePerfRunner(
        device, args, tmp_dir,
        StackAddressInterpreter(args, tmp_dir))
    runner.Run()


if __name__ == '__main__':
  main(sys.argv[1:])
