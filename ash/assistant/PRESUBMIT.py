# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""
PRESUBMIT_VERSION = '2.0.0'

_BANNED_CPP_FUNCTIONS = ()


def CheckNoBannedFunctions(input_api, output_api):
  """Make sure that banned functions are not used."""
  warnings = []

  def GetMessageForFunction(input_api, affected_file, line_num, line, func_name,
                            message):
    result = []
    if input_api.re.search(r"^ *//", line):  # Ignore comments.
      return result
    if line.endswith(" nocheck"):  # Ignore lines with nocheck comments.
      return result

    if func_name in line:
      result.append('    %s:%d:' % (affected_file.LocalPath(), line_num))
      for message_line in message:
        result.append('      %s' % message_line)

    return result


  file_filter = lambda f: f.LocalPath().endswith(('.cc', '.mm', '.h'))
  for f in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in f.ChangedContents():
      for func_name, message in _BANNED_CPP_FUNCTIONS:
        problems = GetMessageForFunction(input_api, f, line_num, line,
                                         func_name, message)
        if problems:
          warnings.extend(problems)

  result = []
  if (warnings):
    result.append(output_api.PresubmitPromptWarning(
        'Banned functions were used.\n' + '\n'.join(warnings)))
  return result


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results += CheckNoBannedFunctions(input_api, output_api)
  return results
