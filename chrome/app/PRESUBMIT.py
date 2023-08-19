# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting chrome/app/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


import os
from xml.dom import minidom

def _CheckNoProductNameInGeneratedResources(input_api, output_api):
  """Check that no PRODUCT_NAME placeholders are found in resources files.

  These kinds of strings prevent proper localization in some languages. For
  more information, see the following chromium-dev thread:
  https://groups.google.com/a/chromium.org/forum/#!msg/chromium-dev/PBs5JfR0Aoc/NOcIHII9u14J
  """

  problems = []
  filename_filter = lambda x: x.LocalPath().endswith(('.grd', '.grdp'))

  for f, line_num, line in input_api.RightHandSideLines(filename_filter):
    if ('PRODUCT_NAME' in line and 'name="IDS_PRODUCT_NAME"' not in line and
       'name="IDS_SHORT_PRODUCT_NAME"' not in line):
      problems.append('%s:%d' % (f.LocalPath(), line_num))

  if problems:
    return [output_api.PresubmitPromptWarning(
        "Don't use PRODUCT_NAME placeholders in string resources. Instead, "
        "add separate strings to google_chrome_strings.grd and "
        "chromium_strings.grd. See http://goo.gl/6614MQ for more information. "
        "Problems with this check? Contact dubroy@chromium.org.",
        items=problems)]
  return []

def _CheckFlagsMessageNotTranslated(input_api, output_api):
  """Check: all about:flags messages are marked as not requiring translation.

  This assumes that such messages are only added to generated_resources.grd and
  that all such messages have names starting with IDS_FLAGS_. The expected mark
  for not requiring translation is 'translateable="false"'.
  """

  problems = []
  filename_filter = lambda x: x.LocalPath().endswith("generated_resources.grd")

  for f, line_num, line in input_api.RightHandSideLines(filename_filter):
    if "name=\"IDS_FLAGS_" in line and not "translateable=\"false\"" in line:
      problems.append("Missing translateable=\"false\" in %s:%d"
                      % (f.LocalPath(), line_num))
      problems.append(line)

  if problems:
    return [output_api.PresubmitError(
        "If you define a flag name, description or value, mark it as not "
        "requiring translation by adding the 'translateable' attribute with "
        "value \"false\". See https://crbug.com/587272 for more context.",
        items=problems)]
  return []

  def _GetInfoStrings(file_contents):
    """Retrieves IDS_EDU_LOGIN_INFO_* messages from the file contents

    Args:
      file_contents: string

    Returns:
      A list of tuples, where each element represents a message.
      element[0] is the 'name' attribute of the message
      element[1] is the message contents
    """
    return [(message.getAttribute('name'), message.firstChild.nodeValue)
            for message in (file_contents.getElementsByTagName('grit-part')[0]
                            .getElementsByTagName('message'))
            if message.getAttribute('name').startswith('IDS_EDU_LOGIN_INFO_')]

  strings_file = next((af for af in input_api.change.AffectedFiles()
                      if af.AbsoluteLocalPath() == CHROMEOS_STRINGS_PATH), None)
  if strings_file is None:
    return []

  old_info_strings = _GetInfoStrings(
      minidom.parseString('\n'.join(strings_file.OldContents())))
  new_info_strings = _GetInfoStrings(
      minidom.parseString('\n'.join(strings_file.NewContents())))
  if set(old_info_strings) == set(new_info_strings):
    return []

  if input_api.change.issue == 0:
    # First upload, notify about string changes.
    return [
        output_api.PresubmitNotifyResult(
            UPDATE_TEXT_VERSION_MESSAGE % "v<GERRIT_CL_NUMBER>"),
        output_api.PresubmitNotifyResult(
            UPDATE_INVALIDATION_VERSION_MESSAGE % "iv<GERRIT_CL_NUMBER>"),
    ]

  new_text_version = "v" + str(input_api.change.issue)
  new_invalidation_version = "iv" + str(input_api.change.issue)

  text_version_file = next((af for af in input_api.change.AffectedFiles()
                            if af.AbsoluteLocalPath() == TEXT_VERSION_PATH),
                            None)
  result = []
  # Check if text version was updated.
  if text_version_file is None or new_text_version not in '\n'.join(
      text_version_file.NewContents()):
    result.append(
        output_api.PresubmitError(
            UPDATE_TEXT_VERSION_MESSAGE % new_text_version))
  # Check if invalidation version was updated.
  if text_version_file is None or new_invalidation_version not in '\n'.join(
      text_version_file.NewContents()):
    result.append(
        output_api.PresubmitNotifyResult(
            UPDATE_INVALIDATION_VERSION_MESSAGE % new_invalidation_version))
  return result

def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(_CheckNoProductNameInGeneratedResources(input_api, output_api))
  results.extend(_CheckFlagsMessageNotTranslated(input_api, output_api))
  return results

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
