# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for files in chrome/browser/resources.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

ACTION_XML_PATH = '../../../tools/metrics/actions/actions.xml'
PRESUBMIT_VERSION = '2.0.0'


def InternalCheckUserActionUpdate(input_api, output_api, action_xml_path):
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
          with open(action_xml_path, encoding='utf-8') as actions_f:
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


def CheckUserActionUpdate(input_api, output_api):
  return InternalCheckUserActionUpdate(input_api, output_api, ACTION_XML_PATH)


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


def CheckSvgsOptimized(input_api, output_api):
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


def _ImportWebDevStyle(input_api):
  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', '..', 'tools')]
    from web_dev_style import presubmit_support
  finally:
    sys.path = old_sys_path
  return presubmit_support


def CheckWebDevStyle(input_api, output_api):
  presubmit_support = _ImportWebDevStyle(input_api)
  return presubmit_support.CheckStyle(input_api, output_api)


def CheckNoNewJs(input_api, output_api):
  EXCLUDED_PATHS = [
    'chrome/browser/resources/.eslintrc',
    'chrome/browser/resources/about_sys/',
    'chrome/browser/resources/ash/settings/.eslintrc',
    'chrome/browser/resources/bluetooth_internals/',
    'chrome/browser/resources/chromeos/',
    'chrome/browser/resources/device_log/',
    'chrome/browser/resources/explore_sites_internals/',
    'chrome/browser/resources/family_link_user_internals/',
    'chrome/browser/resources/feed_internals/',
    'chrome/browser/resources/gaia_auth_host/',
    'chrome/browser/resources/hangout_services/',
    'chrome/browser/resources/image_editor/',
    'chrome/browser/resources/identity_scope_approval_dialog/',
    'chrome/browser/resources/internals/lens/',
    'chrome/browser/resources/internals/notifications/',
    'chrome/browser/resources/internals/query_tiles/',
    'chrome/browser/resources/inspect/',
    'chrome/browser/resources/nearby_internals/',
    'chrome/browser/resources/nearby_share/',
    'chrome/browser/resources/net_internals/',
    'chrome/browser/resources/network_speech_synthesis/',
    'chrome/browser/resources/new_tab_page_incognito_guest/',
    'chrome/browser/resources/new_tab_page/untrusted/',
    'chrome/browser/resources/offline_pages/',
    'chrome/browser/resources/omnibox/',
    'chrome/browser/resources/reading_mode_gdocs_helper/',
    'chrome/browser/resources/settings/',
    'chrome/browser/resources/tools/',
  ]

  normalized_excluded_paths = []
  for path in EXCLUDED_PATHS:
    normalized_excluded_paths.append(input_api.os_path.normpath(path))

  def excluded_path(f):
    for path in normalized_excluded_paths:
      if f.LocalPath().startswith(path) or '.eslintrc.js' in f.LocalPath():
        return True
    return False

  presubmit_support = _ImportWebDevStyle(input_api)
  return presubmit_support.DisallowNewJsFiles(input_api, output_api,
                                              lambda f: not excluded_path(f))

def CheckPatchFormatted(input_api, output_api):
  results = input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                         check_js=True)
  return results
