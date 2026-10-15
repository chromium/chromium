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
    """Checks that HistogramsAllowlist.java contains valid histograms."""
    src_path = os.path.join(input_api.PresubmitLocalPath(), '..', '..', '..',
                            '..')
    histograms_path = os.path.join(src_path, 'tools', 'metrics', 'histograms')
    sys.path.append(histograms_path)

    import histogram_paths
    from histograms_allowlist_check import WellKnownAllowlistPath
    from histograms_allowlist_check import check_histograms_allowlist
    allowlist_path = os.path.join(
        src_path, WellKnownAllowlistPath.ANDROID_WEBVIEW.relative_path())

    histograms_allowlist_filter = lambda f: f.LocalPath().endswith(
        WellKnownAllowlistPath.ANDROID_WEBVIEW.filename())
    if not input_api.AffectedFiles(file_filter=histograms_allowlist_filter):
        return []

    return check_histograms_allowlist(output_api, allowlist_path,
                                      histogram_paths.ALL_XMLS)
