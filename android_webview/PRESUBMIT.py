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
  results.extend(_CheckAwFeatureOverridesComments(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckNoContextBindServiceAdded(input_api, output_api))
  results.extend(_CheckTargetSdkVersionNotDeleted(input_api, output_api))
  results.extend(_CheckAwFeatureOverridesComments(input_api, output_api))
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


def _CheckAwFeatureOverridesComments(input_api, output_api):
  """Checks that any added aw_feature_overrides.DisableFeature calls in
  AwFieldTrials::RegisterFeatureOverrides have a corresponding
  TEMPORARY_DISABLE or PERMANENT_DISABLE comment.
  """
  errors = []

  # We only care about android_webview/browser/aw_field_trials.cc
  def _FilterFile(affected_file):
    return input_api.FilterSourceFile(
        affected_file,
        files_to_skip=input_api.DEFAULT_FILES_TO_SKIP,
        files_to_check=[r'android_webview/browser/aw_field_trials\.cc$'])

  disable_feature_pattern = input_api.re.compile(
      r'aw_feature_overrides\.DisableFeature\(')

  valid_comment_pattern = input_api.re.compile(
      r'^\s*//\s*(?:DISABLED_TEMPORARY|DISABLED_INCOMPATIBLE|'
      r'DISABLED_NEEDS_API|DISABLED_OTHER):\s*(.*)')

  comment_pattern = input_api.re.compile(r'^\s*//')

  for f in input_api.AffectedSourceFiles(_FilterFile):
    new_contents = f.NewContents()
    for line_num, line in f.ChangedContents():
      if disable_feature_pattern.search(line):
        # line_num is 1-indexed.
        idx = line_num - 2
        found_valid_comment = False
        comment_lines = []
        explanation = None

        # Search the preceding lines of text until we find a comment that
        # matches the expected pattern, or break out of the search when we find
        # a non-comment line.
        state = 'SEARCHING_COMMENT'

        def check_valid_comment(current_line):
          nonlocal found_valid_comment, explanation
          m = valid_comment_pattern.match(current_line)
          if not m:
            return False
          found_valid_comment = True
          explanation = m.group(1)
          return True

        while idx >= 0:
          current_line = new_contents[idx]
          stripped_line = current_line.strip()

          if state == 'SEARCHING_COMMENT':
            if not stripped_line:
              # An empty line before we found any comment means no comment
              # for this group/call.
              break
            elif 'aw_feature_overrides.DisableFeature' in current_line:
              # Skip other DisableFeature calls in the same group, but don't
              # update the 'comment_lines'.
              idx -= 1
              continue
            elif comment_pattern.match(current_line):
              state = 'READING_COMMENT'
              if check_valid_comment(current_line):
                break
            else:
              # Hit some other code before any comment
              break
          elif state == 'READING_COMMENT':
            if comment_pattern.match(current_line):
              if check_valid_comment(current_line):
                break
            else:
              # Hit non-comment line after starting to read comments.
              # This ends the comment block.
              break
          # If we didn't find a comment yet, check the previous line.
          comment_lines.append((idx + 1, current_line))
          idx -= 1

        if not found_valid_comment:
          if comment_lines:
            preceding_comment = ' '.join(
                [f'Line {l}: {c.strip()}' for l, c in comment_lines])
          else:
            preceding_comment = 'None'
          errors.append(
              f"{f.LocalPath()}:{line_num}: {line.strip()}\n"
              f"  (Preceding comment was: {preceding_comment})"
          )
        if found_valid_comment and not explanation:
          errors.append(
              f"{f.LocalPath()}:{line_num}: {line.strip()}\n"
              f"  (You must provide an explanation why the feature is "
              f"disabled)"
          )


  results = []
  if errors:
    message = (
        "New calls to aw_feature_overrides.DisableFeature() must be "
        "preceded by a comment explaining why the feature is disabled in "
        "WebView.\n\n"
        "The comment must match one of the following formats:\n"
        "  // DISABLED_TEMPORARY: <explanation>\n"
        "  // DISABLED_INCOMPATIBLE: <explanation>\n"
        "  // DISABLED_NEEDS_API: <explanation>\n"
        "  // DISABLED_OTHER: <explanation>\n\n"
        "Guidelines:\n"
        "  - DISABLED_TEMPORARY: This is temporarily off-by-default, but "
        "we plan to rollout gradually via Finch in the short term "
        "future. Please write a link to a crbug tracking the "
        "feature implementation/rollout (no other explanation necessary).\n"
        "  - DISABLED_INCOMPATIBLE: Fundamentally incompatible with WebView's "
        "architecture (ex. it modifies process-level state). "
        "Please briefly explain why this is incompatible.\n"
        "  - DISABLED_NEEDS_API: Might make sense for WebView, but it would "
        "require app-facing APIs. Please briefly explain what sort of API "
        "would be necessary (ex. app-facing setting to turn it off, callback "
        "to notify the app, etc.). If API implementation is already "
        "underway or planned for the near future, then use "
        "DISABLED_TEMPORARY instead.\n"
        "  - DISABLED_OTHER: Any other reason to disable this by default. "
        "Write an explanation for why this is disabled.\n\n"
        "Affected lines:"
    )
    results.append(output_api.PresubmitError(message, errors))

  return results
