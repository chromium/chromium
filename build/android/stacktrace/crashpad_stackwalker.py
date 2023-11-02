#!/usr/bin/env vpython3
#
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Fetches Crashpad dumps from a given device, walks and symbolizes the stacks.
# All the non-trivial operations are performed by generate_breakpad_symbols.py,
# dump_syms, minidump_dump and minidump_stackwalk.

import argparse
import logging
import os
import posixpath
import re
import sys
import shutil
import subprocess
import tempfile

_BUILD_ANDROID_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..'))
sys.path.append(_BUILD_ANDROID_PATH)
import devil_chromium
from devil.android import device_utils
from devil.utils import timeout_retry


def _CreateSymbolsDir(build_path, dynamic_library_names):
  generator = os.path.normpath(
      os.path.join(_BUILD_ANDROID_PATH, '..', '..', 'components', 'crash',
                   'content', 'tools', 'generate_breakpad_symbols.py'))
  syms_dir = os.path.join(build_path, 'crashpad_syms')
  shutil.rmtree(syms_dir, ignore_errors=True)
  os.mkdir(syms_dir)
  for lib in dynamic_library_names:
    unstripped_library_path = os.path.join(build_path, 'lib.unstripped', lib)
    if not os.path.exists(unstripped_library_path):
      continue
    logging.info('Generating symbols for: %s', unstripped_library_path)
    cmd = [
        generator,
        '--symbols-dir',
        syms_dir,
        '--build-dir',
        build_path,
        '--binary',
        unstripped_library_path,
        '--platform',
        'android',
    ]
    return_code = subprocess.call(cmd)
    if return_code != 0:
      logging.error('Could not extract symbols, command failed: %s',
                    ' '.join(cmd))
  return syms_dir


def _ChooseLatestCrashpadDump(device, crashpad_dump_path):
  if not device.PathExists(crashpad_dump_path):
    logging.warning('Crashpad dump directory does not exist: %s',
                    crashpad_dump_path)
    return None
  latest = None
  latest_timestamp = 0
  for crashpad_file in device.ListDirectory(crashpad_dump_path):
    if crashpad_file.endswith('.dmp'):
      stat = device.StatPath(posixpath.join(crashpad_dump_path, crashpad_file))
      current_timestamp = stat['st_mtime']
      if current_timestamp > latest_timestamp:
        latest_timestamp = current_timestamp
        latest = crashpad_file
  return latest


def _ExtractLibraryNamesFromDump(build_path, dump_path):
  default_library_name = 'libmonochrome.so'
  dumper_path = os.path.join(build_path, 'minidump_dump')
  if not os.access(dumper_path, os.X_OK):
    logging.warning(
        'Cannot extract library name from dump because %s is not found, '
        'default to: %s', dumper_path, default_library_name)
    return [default_library_name]
  p = subprocess.Popen([dumper_path, dump_path],
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
  stdout, stderr = p.communicate()
  if p.returncode != 0:
    # Dumper errors often do not affect stack walkability, just a warning.
    logging.warning('Reading minidump failed with output:\n%s', stderr)

  library_names = []
  module_library_line_re = re.compile(r'[(]code_file[)]\s+= '
                                      r'"(?P<library_name>lib[^. ]+.so)"')
  in_module = False
  for line in stdout.splitlines():
    line = line.lstrip().rstrip('\n')
    if line == 'MDRawModule':
      in_module = True
      continue
    if line == '':
      in_module = False
      continue
    if in_module:
      m = module_library_line_re.match(line)
      if m:
        library_names.append(m.group('library_name'))
  if not library_names:
    logging.warning(
        'Could not find any library name in the dump, '
        'default to: %s', default_library_name)
    return [default_library_name]
  return library_names


def main():
  logging.basicConfig(level=logging.INFO)
  parser = argparse.ArgumentParser(
      description='Fetches Crashpad dumps from a given device, '
      'walks and symbolizes the stacks.')
  parser.add_argument('--device', required=True, help='Device serial number')
  parser.add_argument('--adb-path', help='Path to the "adb" command')
  parser.add_argument(
      '--build-path',
      required=True,
      help='Build output directory, equivalent to CHROMIUM_OUTPUT_DIR')
  parser.add_argument(
      '--chrome-cache-path',
      required=True,
      help='Directory on the device where Chrome stores cached files,'
      ' crashpad stores dumps in a subdirectory of it')
  args = parser.parse_args()

  stackwalk_path = os.path.join(args.build_path, 'minidump_stackwalk')
  if not os.path.exists(stackwalk_path):
    logging.error('Missing minidump_stackwalk executable')
    return 1

  devil_chromium.Initialize(output_directory=args.build_path,
                            adb_path=args.adb_path)
  device = device_utils.DeviceUtils(args.device)

  device_crashpad_path = posixpath.join(args.chrome_cache_path, 'Crashpad',
                                        'pending')

  def CrashpadDumpExists():
    return _ChooseLatestCrashpadDump(device, device_crashpad_path)

  crashpad_file = timeout_retry.WaitFor(
      CrashpadDumpExists, wait_period=1, max_tries=9)
  if not crashpad_file:
    logging.error('Could not locate a crashpad dump')
    return 1

  dump_dir = tempfile.mkdtemp()
  symbols_dir = None
  try:
    device.PullFile(
        device_path=posixpath.join(device_crashpad_path, crashpad_file),
        host_path=dump_dir)
    dump_full_path = os.path.join(dump_dir, crashpad_file)
    library_names = _ExtractLibraryNamesFromDump(args.build_path,
                                                 dump_full_path)
    symbols_dir = _CreateSymbolsDir(args.build_path, library_names)
    stackwalk_cmd = [stackwalk_path, dump_full_path, symbols_dir]
    subprocess.call(stackwalk_cmd)
  finally:
    shutil.rmtree(dump_dir, ignore_errors=True)
    if symbols_dir:
      shutil.rmtree(symbols_dir, ignore_errors=True)
  return 0


if __name__ == '__main__':
  sys.exit(main())
