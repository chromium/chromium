# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit tests for //android_webview/

Gates against using Context#bindService API before upload.
"""

def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CheckNoContextBindServiceAdded(input_api, output_api))
  results.extend(_CheckTargetSdkVersionNotDeleted(input_api, output_api))
  return results

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckNoContextBindServiceAdded(input_api, output_api))
  results.extend(_CheckTargetSdkVersionNotDeleted(input_api, output_api))
  return results

def _CheckNoContextBindServiceAdded(input_api, output_api):
  """Checks that new no files under //android_webview directly use the
  Context#bindService. This is because Android platform disallows calling
  Context#bindService() from within a BroadcastReceiver context.
  """
  errors = []
  bind_service_pattern = input_api.re.compile(
      r'.*\.bindService\(.*')

  def _FilterFile(affected_file):
    skip_files = (input_api.DEFAULT_FILES_TO_SKIP +
      (r'.*android_webview[/\\]common[/\\]services[/\\]ServiceHelper\.java',
        r'.*android_webview[/\\]support_library[/\\]boundary_interfaces[/\\].*',
        r'.*android_webview[/\\]js_sandbox[/\\].*',))
    return input_api.FilterSourceFile(
        affected_file,
        files_to_skip=skip_files,
        files_to_check=[r'.+\.java$'])

  for f in input_api.AffectedSourceFiles(_FilterFile):
    for line_num, line in f.ChangedContents():
      match = bind_service_pattern.search(line)
      if match:
        if "ServiceHelper.bindService" not in line:
          errors.append("%s:%d:%s" % (f.LocalPath(), line_num, line))

  results = []

  if errors:
    results.append(output_api.PresubmitPromptWarning("""
New code in //android_webview should not use \
android.content.Context#bindService. Instead use \
android_webview.common.services.ServiceHelper#bindService.
""", errors))

  return results

def _CheckTargetSdkVersionNotDeleted(input_api, output_api):
  """Checks that we keep any checks related to targetSdkVersion in place. It's
  OK for WebView to drop support for old OS versions (SDK_INT), however we do
  not yet want to drop support for old targetSdks (targetSdkVersion). It's
  possible for apps to specify a really old targetSdkVersion even if the app is
  installed on new OS versions.
  """
  footer_key = 'Bypass-Target-Sdk-Version-Check'
  reasons = input_api.change.GitFootersFromDescription().get(footer_key, [])
  if reasons:
    if ''.join(reasons).strip() == '':
      return [
          output_api.PresubmitError(
              f'{footer_key} is specified without a reason. Please provide a '
              f'reason in "{footer_key}: <reason>"')
      ]
    input_api.logging.info('Target SDK version check is being bypassed.')
    return []

  errors = []
  target_sdk_version_pattern = input_api.re.compile(
      r'.*[tT]argetSdkVersion.*')

  def _FilterFile(affected_file):
    return input_api.FilterSourceFile(
        affected_file,
        files_to_skip=input_api.DEFAULT_FILES_TO_SKIP,
        files_to_check=[r'.+\.java$'])

  for f in input_api.AffectedSourceFiles(_FilterFile):
    old_target_sdk_checks = []
    new_target_sdk_checks = []
    for line in f.OldContents():
      match = target_sdk_version_pattern.search(line)
      if match:
        old_target_sdk_checks.append(line.strip())
    for line in f.NewContents():
      match = target_sdk_version_pattern.search(line)
      if match:
        new_target_sdk_checks.append(line.strip())
    for line in old_target_sdk_checks:
      if line not in new_target_sdk_checks:
        errors.append("%s: %s" % (f.LocalPath(), line))

  results = []

  if errors:
    results.append(output_api.PresubmitError("""
Code in //android_webview should not ever get rid of checks to targetSdkVersion.
This code is not dead code, even when we drop support for old SDK_INT versions.
It is possible for apps to specify a really old targetSdkVersion even if the app
is installed on new OS versions, and this is something we want to support.

If you are completely deleting a check, then please undo the change or raise
this issue with //android_webview/OWNERS to get their explicit approval before
landing the change.

To bypass this check, add "Bypass-Target-Sdk-Version-Check: <reason>" to the
CL description.
""", errors))

  return results
