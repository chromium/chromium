# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for OS Settings."""


import sys

def _CheckSemanticCssColors(input_api, output_api):
    original_sys_path = sys.path
    join = input_api.os_path.join
    src_root = input_api.change.RepositoryRoot()
    try:
        # Change the system path to SemanticCssChecker's directory to be
        # able to import it.
        sys.path.append(join(src_root, 'ui', 'chromeos', 'styles'))
        from semantic_css_checker import SemanticCssChecker
        return SemanticCssChecker.RunChecks(input_api, output_api)
    finally:
        sys.path = original_sys_path


def _CheckOSSettings(input_api, output_api):
    original_sys_path = sys.path
    try:
        cwd = input_api.PresubmitLocalPath()
        sys.path.append(cwd)
        from os_settings_presubmit_checker import OSSettingsPresubmitChecker
        return OSSettingsPresubmitChecker.RunChecks(input_api, output_api)
    finally:
        sys.path = original_sys_path


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    results = []
    results.extend(_CheckSemanticCssColors(input_api, output_api))
    results.extend(_CheckOSSettings(input_api, output_api))
    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
