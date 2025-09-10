# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting chrome/app/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


import os
import re
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

def _CheckNoLiteralBrandNamesInGeneratedResources(input_api, output_api):
  """Disallow hardcoded 'Chrome' and 'Chromium' in generated_resources.grd.

  Authors should prefer adding branded IDs in
  //chrome/app/google_chrome_strings.grd and //chrome/app/chromium_strings.grd
  instead of hardcoding one brand in generated_resources.grd.
  """
  STRICT = False

  brand_word = re.compile(r'(?<![A-Za-z])(Chrome|Chromium)(?![A-Za-z])')
  filename_filter = \
    lambda af: af.LocalPath().endswith("generated_resources.grd")

  problems = []

  # 1) Build: per-file → set of changed line numbers (RHS only).
  changed_lines_by_file = {}
  for af, line_num, _ in input_api.RightHandSideLines(filename_filter):
    changed_lines_by_file.setdefault(af, set()).add(line_num)

  if not changed_lines_by_file:
    return []

  # 2) For each file with changes, parse message ranges once, then
  #    find only the message blocks that intersect changed lines.
  for af, changed_lines in changed_lines_by_file.items():
    new_lines = list(af.NewContents())
    if not new_lines:
      continue

    # Find all <message ...> ... </message> ranges: (start_line, end_line).
    ranges = []
    inside = False
    start = None
    for i, line in enumerate(new_lines, start=1):
      if not inside and "<message" in line:
        inside = True
        start = i
      if inside and "</message>" in line:
        ranges.append((start, i))
        inside = False
        start = None

    if not ranges:
      continue

    # Which message ranges were touched?
    touched_ranges = []
    for range in ranges:
      start, end = range
      # any changed line within [start, end] ?
      if any(start <= line_number <= end for line_number in changed_lines):
        touched_ranges.append(range)

    if not touched_ranges:
      continue

    # 3) Scan only touched message blocks; check *content* (strip tags).
    for start, end in touched_ranges:
      block_text = "\n".join(new_lines[start - 1:end])  # inclusive

      # Ignore <ex>…</ex>
      stripped = re.sub(r"<ex>.*?</ex>", "", block_text, flags=re.DOTALL)
      # Remove remaining tags
      stripped = re.sub(r"<[^>]+>", "", stripped)

      if brand_word.search(stripped):
        problems.append(f"{af.LocalPath()}:{start}: {stripped.strip()}")

  if not problems:
    return []

  hint = (
      "Avoid hardcoding 'Chrome' or 'Chromium' inside "
      "generated_resources.grd.\nAdd new branded IDs to "
      "google_chrome_strings.grd / chromium_strings.grd instead."
  )
  text = \
  "Hardcoded brand names found in generated_resources.grd:\n" + "\n".join(
    problems) + "\n\n" + hint

  if STRICT:
    return [output_api.PresubmitError(text)]

  return [output_api.PresubmitPromptWarning(text)]

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
  results.extend(
    _CheckNoLiteralBrandNamesInGeneratedResources(input_api, output_api))
  results.extend(_CheckFlagsMessageNotTranslated(input_api, output_api))
  return results

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
