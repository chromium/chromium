# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess

PRESUBMIT_VERSION = '2.0.0'

def CheckSqlModules(input_api, output_api):
  stdlib_dir = input_api.PresubmitLocalPath()
  chromium_src_dir = input_api.os_path.join(stdlib_dir, '..', '..')
  tool = input_api.os_path.join(
    chromium_src_dir,
    'third_party', 'perfetto', 'tools', 'check_sql_modules.py')
  cmd = [
    input_api.python3_executable,
    tool,
    '--stdlib-sources',
    './stdlib/chrome'
    ]
  if subprocess.call(cmd):
    # TODO(b/283962174): Add presubmit failure when TP stdlib migration
    # is complete.
    return []
  return []
