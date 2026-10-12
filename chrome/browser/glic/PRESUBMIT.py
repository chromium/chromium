# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for chrome/browser/glic.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def _CheckHeaderOrdering(input_api, output_api):
  include_pattern = input_api.re.compile(r'^\s*#\s*(?:include|import)\b')

  def file_filter(affected_file):
    if not input_api.FilterSourceFile(
      affected_file,
      files_to_check=[r'^chrome/browser/glic/.*\.(cc|h|mm)$'],
    ):
      return False
    return any(
      include_pattern.match(line) for _, line in affected_file.ChangedContents()
    )

  affected_files = list(
    input_api.AffectedFiles(include_deletes=False, file_filter=file_filter)
  )
  file_paths = [f.UnixLocalPath() for f in affected_files]
  if not file_paths:
    return []

  sort_headers_script = input_api.os_path.join(
    input_api.PresubmitLocalPath(),
    'tools',
    'sort_headers.py',
  )

  cmd = [
    input_api.python3_executable,
    sort_headers_script,
    '--check-only',
  ] + file_paths
  proc = input_api.subprocess.Popen(
    cmd,
    cwd=input_api.change.RepositoryRoot(),
    stdout=input_api.subprocess.PIPE,
    stderr=input_api.subprocess.PIPE,
    text=True,
  )
  stdout, _ = proc.communicate()

  if proc.returncode != 0:
    unsorted_files = [
      line.strip() for line in stdout.splitlines() if line.strip()
    ]
    target_files = unsorted_files or file_paths
    cmd_str = 'python3 chrome/browser/glic/tools/sort_headers.py ' + ' '.join(
      target_files
    )
    message = (
      'The C++ header includes in the following file(s) '
      + 'are not properly sorted. Please run:\n  '
      + cmd_str
    )
    return [
      output_api.PresubmitError(
        message,
        items=target_files,
      )
    ]
  return []


def _CheckMemberOrdering(input_api, output_api):
  def file_filter(affected_file):
    return input_api.FilterSourceFile(
      affected_file,
      files_to_check=[r'^chrome/browser/glic/.*\.(cc|h|mm)$'],
    )

  affected_files = list(
    input_api.AffectedFiles(include_deletes=False, file_filter=file_filter)
  )
  if not affected_files:
    return []

  file_paths = [f.UnixLocalPath() for f in affected_files]

  check_script = input_api.os_path.join(
    input_api.PresubmitLocalPath(),
    'tools',
    'check_member_order.py',
  )

  cmd = [
    input_api.python3_executable,
    check_script,
  ] + file_paths
  proc = input_api.subprocess.Popen(
    cmd,
    cwd=input_api.change.RepositoryRoot(),
    stdout=input_api.subprocess.PIPE,
    stderr=input_api.subprocess.PIPE,
    text=True,
  )
  stdout, stderr = proc.communicate()

  if proc.returncode != 0:
    changed_lines_by_file = {}
    for f in affected_files:
      unix_path = f.UnixLocalPath()
      changed_lines_by_file[unix_path] = {
        line_num for line_num, _ in f.ChangedContents()
      }

    violation_pattern = input_api.re.compile(
      r'^\s*(?P<file>[^:]+):(?P<func_line>\d+): '
      r'(?:error: )?In class \'(?P<class>[^\']+)\', '
      r'function \'(?P<func>[^\']+)\' is declared after member field '
      r'\'(?P<field>[^\']+)\' \(line (?P<field_line>\d+)\)'
    )

    filtered_violations = []
    affected_violating_files = set()

    for line in stderr.splitlines():
      m = violation_pattern.match(line)
      if m:
        f_path = m.group('file').replace('\\', '/')
        func_line = int(m.group('func_line'))
        field_line = int(m.group('field_line'))
        changed_lines = changed_lines_by_file.get(f_path)
        if changed_lines is None or (
          not changed_lines
          or func_line in changed_lines
          or field_line in changed_lines
        ):
          filtered_violations.append(line)
          affected_violating_files.add(f_path)

    if filtered_violations:
      target_files = sorted(list(affected_violating_files)) or file_paths
      details = '\n' + '\n'.join(filtered_violations)
      message = (
        'Class member fields should be declared after member functions.'
        + details
      )
      return [
        output_api.PresubmitError(
          message,
          items=target_files,
        )
      ]
  return []


def _CheckGlicApiTestRegistration(input_api, output_api):
  old_path = input_api.sys.path[:]
  try:
    tools_path = input_api.os_path.join(
      input_api.change.RepositoryRoot(), 'chrome', 'browser', 'glic', 'tools'
    )
    input_api.sys.path.insert(0, tools_path)
    import check_glic_api_test_registration

    return check_glic_api_test_registration.CheckGlicApiTestRegistration(
      input_api, output_api
    )
  finally:
    input_api.sys.path = old_path


def _GlicCommonChecks(input_api, output_api):
  old_path = input_api.sys.path[:]
  try:
    input_api.sys.path.insert(0, input_api.change.RepositoryRoot())
    from chrome.browser.resources.glic.common_checks import GlicCommonChecks

    return GlicCommonChecks(input_api, output_api)
  finally:
    input_api.sys.path = old_path


def CheckChange(input_api, output_api):
  return (
    _CheckHeaderOrdering(input_api, output_api)
    + _CheckMemberOrdering(input_api, output_api)
    + _CheckGlicApiTestRegistration(input_api, output_api)
    + _GlicCommonChecks(input_api, output_api)
  )


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
