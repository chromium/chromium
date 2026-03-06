# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'

def CheckStdlib(input_api, output_api):
  stdlib_dir = input_api.PresubmitLocalPath()
  chromium_src_dir = input_api.os_path.abspath(
    input_api.os_path.join(stdlib_dir, '..', '..'))
  tool = input_api.os_path.join(
    chromium_src_dir,
    'tools', 'tracing', 'check_stdlib.py')
  cmd = [ input_api.python3_executable, tool ]
  test_cmd = input_api.Command(
    name='check_stdlib',
    cmd=cmd,
    kwargs={},
    message=output_api.PresubmitError)
  return input_api.RunTests([test_cmd])

_STDLIB_PATHS = (
  r"^base/tracing/stdlib/",
  r"^base/tracing/test/",
  r"^base/tracing/protos/"
)

def CheckPerfettoTestsTag(input_api, output_api):
  """Checks that commits to the trace processor chrome stdlib or the
  Perfetto diff tests contain a PERFETTO_TESTS tag in their commit
  message."""
  def FileFilter(affected_file):
    return input_api.FilterSourceFile(affected_file,
                                      files_to_check=_STDLIB_PATHS)

  # Only consider changes to chrome stdlib or tests paths
  if not any (input_api.AffectedFiles(file_filter=FileFilter)):
    return []

  if input_api.change.PERFETTO_TESTS:
    return []

  message = (
    'Must provide PERFETTO_TESTS='
    '`autoninja -C out/Default perfetto_diff_tests && '
    'out/Default/bin/run_perfetto_diff_tests` line in CL description.'
    '\nPlease ensure the Perfetto diff tests pass before submitting.'
  )
  return [output_api.PresubmitNotifyResult(message)]

def CheckTestDataCheckedIn(input_api, output_api):
  """Checks that the test data files in base/tracing/test/data_sha256 are
  checked in.
  """
  def FileFilter(affected_file):
    return input_api.FilterSourceFile(affected_file,
                                      files_to_check=_STDLIB_PATHS)

  if not any(input_api.AffectedFiles(file_filter=FileFilter)):
    return []

  sha256_dir = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                      'test', 'data_sha256')
  unpublished_files = []
  for filename in input_api.os_listdir(sha256_dir):
    if not filename.endswith('.sha256'):
      continue
    path = input_api.os_path.join(sha256_dir, filename)
    # This checks if the file is known to git.
    # git ls-files returns the filename if it is tracked, and nothing otherwise.
    cmd = ['git', 'ls-files', '--error-unmatch', path]
    try:
      input_api.subprocess.check_output(
          cmd, stderr=input_api.subprocess.STDOUT)
    except input_api.subprocess.CalledProcessError:
      unpublished_files.append(filename)

  if unpublished_files:
    return [
        output_api.PresubmitError(
          'The following files in base/tracing/test/data_sha256/ are not '
          'checked into git. Please add them:',
          unpublished_files)
    ]
  return []
