# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

HISTOGRAMS_ALLOWLIST_PATH = (
    'android_webview/java/res/raw/histograms_allowlist.txt')
HISTOGRAMS_ALLOWLIST_FILENAME = HISTOGRAMS_ALLOWLIST_PATH.split('/')[-1]

def get_histograms_allowlist_content(src_path):
    histograms_allowlist_path = os.path.join(
        src_path, *HISTOGRAMS_ALLOWLIST_PATH.split('/'))
    with open(histograms_allowlist_path) as file:
        return [line.rstrip() for line in file]


def CheckWebViewHistogramsAllowlist(src_path, output_api):
    """Checks that histograms_allowlist.txt contains valid histograms.
    src_path should point to chromium/src
    """
    histograms_path = os.path.join(src_path, 'tools', 'metrics', 'histograms')
    sys.path.append(histograms_path)
    import print_histogram_names

    all_histograms = print_histogram_names.get_names(
        print_histogram_names.histogram_xml_files())

    histograms_allowlist = get_histograms_allowlist_content(src_path)

    errors = []
    for histogram in histograms_allowlist:
        if histogram not in all_histograms:
            errors.append(
                f'{HISTOGRAMS_ALLOWLIST_PATH} contains unknown histogram '
                f'<{histogram}>')

    if not errors:
        return []

    results = [
        output_api.PresubmitError(
            f'All histograms in {HISTOGRAMS_ALLOWLIST_PATH} must be valid.',
            errors,
        )
    ]

    return results
