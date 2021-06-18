# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for OS Settings."""

USE_PYTHON3 = True

import re


def _CheckSemanticColors(input_api, output_api):
    problems = []
    for f in input_api.AffectedFiles():
        exts = ['html', 'css']
        if not any(f.LocalPath().endswith(ext) for ext in exts):
            continue

        paper_color_re = re.compile(r'--paper-\w+-\d+')
        google_refresh_color_re = re.compile(r'--google-\w+-refresh-\d+')

        for line_num, line in f.ChangedContents():
            # Search for paper-colors.
            if paper_color_re.search(line):
                problems.append(line.strip())

            # Search for google-refresh-colors.
            if google_refresh_color_re.search(line):
                problems.append(line.strip())

    if not problems:
        return []
    return [
        output_api.PresubmitPromptWarning(
            'Please avoid using paper-colors and google-refresh-colors on ' +
            'Chrome OS.\n' + 'Allowed colors are listed in ' +
            'ui/webui/resources/css/cros_palette.json5.\n' +
            'See https://crbug.com/1062154 or contact calamity@chromium.org ' +
            'or ortuno@chromium.org for more information.',
            items=problems)
    ]


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    results = []
    results.extend(_CheckSemanticColors(input_api, output_api))
    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
