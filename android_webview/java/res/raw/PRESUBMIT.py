# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit tests for android_webview/java/res/raw"""

import os
import sys


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CheckHistogramsAllowlist(input_api, output_api))
    return results


def _CheckHistogramsAllowlist(input_api, output_api):
    """Checks that histograms_allowlist.txt contains valid histograms."""
    sys.path.append(input_api.PresubmitLocalPath())
    from histograms_allowlist_check import HISTOGRAMS_ALLOWLIST_FILENAME
    from histograms_allowlist_check import CheckWebViewHistogramsAllowlist

    histograms_allowlist_filter = lambda f: f.LocalPath().endswith(
        HISTOGRAMS_ALLOWLIST_FILENAME)
    if not input_api.AffectedFiles(file_filter=histograms_allowlist_filter):
        return []

    # src_path should point to chromium/src
    src_path = os.path.join(input_api.PresubmitLocalPath(), '..', '..', '..',
                            '..')
    return CheckWebViewHistogramsAllowlist(src_path, output_api)
