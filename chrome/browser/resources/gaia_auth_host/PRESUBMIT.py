# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for gaia_auth_host

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


import re

def _FilterFile(affected_file):
    """Returns true if the file could contain code requiring a presubmit check.
    """
    return affected_file.LocalPath().endswith(('.js'))

def _CountOccurences(matcher, contents):
    return sum(matcher.search(line) != None for line in contents)

def _CheckSamlHandlerApiCallErrors(input_api, output_api):
    """Checks that the number of "console.warn('SamlHandler.onAPICall_"
       statements stays the same.
    """
    matcher = input_api.re.compile(r'console\.warn\(\'SamlHandler\.onAPICall_')
    for f in input_api.AffectedFiles(_FilterFile):
        new_occurences = _CountOccurences(matcher, f.NewContents())
        old_occurences = _CountOccurences(matcher, f.OldContents())
        if new_occurences < old_occurences:
            return [output_api.PresubmitPromptWarning(
                'Seems that you\'re modifiying a warn log '
                '\'SamlHandler\.onAPICall_\' which is used as a signal for a '
                'tast test. Please make sure the `login.ChromeGaiaAPI` test and'
                ' this presubmit check stays functional') ]
    return []

def _CommonCheck(input_api, output_api):
    return _CheckSamlHandlerApiCallErrors(input_api, output_api)

def CheckChangeOnUpload(input_api, output_api):
    return _CommonCheck(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
    return _CommonCheck(input_api, output_api)
