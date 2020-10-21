# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for files in chrome/browser/resources.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

ACTION_XML_PATH = '../../../tools/metrics/actions/actions.xml'


def CheckUserActionUpdate(input_api, output_api, action_xml_path):
  """Checks if any new user action has been added."""
  if any('actions.xml' == input_api.os_path.basename(f) for f in
         input_api.change.LocalPaths()):
    # If actions.xml is already included in the changelist, the PRESUBMIT
    # for actions.xml will do a more complete presubmit check.
    return []

  file_filter = lambda f: f.LocalPath().endswith('.html')
  action_re = r'(^|\s+)metric\s*=\s*"([^ ]*)"'
  current_actions = None
  for f in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in f.ChangedContents():
      match = input_api.re.search(action_re, line)
      if match:
        # Loads contents in tools/metrics/actions/actions.xml to memory. It's
        # loaded only once.
        if not current_actions:
          with open(action_xml_path) as actions_f:
            current_actions = actions_f.read()

        metric_name = match.group(2)
        is_boolean = IsBoolean(f.NewContents(), metric_name, input_api)

        # Search for the matched user action name in |current_actions|.
        if not IsActionPresent(current_actions, metric_name, is_boolean):
          return [output_api.PresubmitPromptWarning(
            'File %s line %d: %s is missing in '
            'tools/metrics/actions/actions.xml. Please run '
            'tools/metrics/actions/extract_actions.py to update.'
            % (f.LocalPath(), line_num, metric_name), [])]
  return []


def IsActionPresent(current_actions, metric_name, is_boolean):
  """Checks if metric_name is defined in the actions file.

  Checks whether there's matching entries in an actions.xml file for the given
  |metric_name|, depending on whether it is a boolean action.

  Args:
    current_actions: The content of the actions.xml file.
    metric_name: The name for which the check should be done.
    is_boolean: Whether the action comes from a boolean control.
  """
  if not is_boolean:
    action = 'name="{0}"'.format(metric_name)
    return action in current_actions

  action_disabled = 'name="{0}_Disable"'.format(metric_name)
  action_enabled = 'name="{0}_Enable"'.format(metric_name)

  return (action_disabled in current_actions and
      action_enabled in current_actions)


def IsBoolean(new_content_lines, metric_name, input_api):
  """Check whether action defined in the changed code is boolean or not.

  Checks whether the action comes from boolean control based on the HTML
  elements attributes.

  Args:
    new_content_lines: List of changed lines.
    metric_name: The  name for which the check should be done.
  """
  new_content = '\n'.join(new_content_lines)

  html_element_re = r'<(.*?)(^|\s+)metric\s*=\s*"%s"(.*?)>' % (metric_name)
  type_re = (r'datatype\s*=\s*"boolean"|type\s*=\s*"checkbox"|'
      'type\s*=\s*"radio".*?value\s*=\s*("true"|"false")')

  match = input_api.re.search(html_element_re, new_content, input_api.re.DOTALL)
  return (match and
      any(input_api.re.search(type_re, match.group(i)) for i in (1, 3)))


def CheckHtml(input_api, output_api):
  return input_api.canned_checks.CheckLongLines(
      input_api, output_api, 80, lambda x: x.LocalPath().endswith('.html'))


def RunOptimizeWebUiTests(input_api, output_api):
  presubmit_path = input_api.PresubmitLocalPath()
  sources = ['optimize_webui_test.py', 'unpack_pak_test.py']
  tests = [input_api.os_path.join(presubmit_path, s) for s in sources]
  return input_api.canned_checks.RunUnitTests(input_api, output_api, tests)


def _CheckSvgsOptimized(input_api, output_api):
  results = []
  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', '..', 'tools')]
    from resources import svgo_presubmit
    results += svgo_presubmit.CheckOptimized(input_api, output_api)
  finally:
    sys.path = old_sys_path
  return results


def _CheckWebDevStyle(input_api, output_api):
  results = []

  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', '..', 'tools')]
    from web_dev_style import presubmit_support
    results += presubmit_support.CheckStyle(input_api, output_api)
  finally:
    sys.path = old_sys_path

  return results


def _CheckChangeOnUploadOrCommit(input_api, output_api):
  results = CheckUserActionUpdate(input_api, output_api, ACTION_XML_PATH)
  affected = input_api.AffectedFiles()
  if any(f for f in affected if f.LocalPath().endswith('.html')):
    results += CheckHtml(input_api, output_api)

  webui_sources = set(['optimize_webui.py', 'unpack_pak.py'])
  affected_files = [input_api.os_path.basename(f.LocalPath()) for f in affected]
  if webui_sources.intersection(set(affected_files)):
    results += RunOptimizeWebUiTests(input_api, output_api)
  results += _CheckSvgsOptimized(input_api, output_api)
  results += _CheckWebDevStyle(input_api, output_api)
  results += input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                         check_js=True)
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)
