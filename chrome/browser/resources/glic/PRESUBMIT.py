# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

# Runs PRESUBMIT.py in py3 mode by git cl presubmit.
USE_PYTHON3 = True

DEBUG = False

API_FILE = 'chrome/browser/resources/glic/glic_api/glic_api.ts'

TRIGGERING_FILE_PREFIXES = [
    'chrome/browser/resources/glic/glic_api/',
    'chrome/browser/resources/glic/presubmit/',
]


def CheckApiChanges(input_api, output_api, api_file, on_upload):
    skip_compatibility_check = (
        'Bypass-Glic-Api-Compatibility-Check'
        in input_api.change.GitFootersFromDescription())
    src_root = input_api.os_path.join(os.getcwd(), '../../../../')
    api_file_path = input_api.os_path.join(src_root, API_FILE)
    # If API_FILE was modified, get its old contents. Otherwise, use its current
    # contents to confirm any modified checks still pass.
    if api_file:
        old_contents = '\n'.join(api_file.OldContents())
    else:
        with open(api_file_path, 'r') as f:
            old_contents = f.read()

    cmd = [
        input_api.python_executable,
        input_api.os_path.join(
            src_root, 'chrome/browser/resources/glic/presubmit/check_api.py'),
        '--old-stdin',
        '--api-file-path=' + api_file_path,
    ]
    if skip_compatibility_check:
        cmd.append('--skip-compatibility-check')

    presubmit_results = []
    try:
        proc = input_api.subprocess.Popen(cmd,
                                          stdin=input_api.subprocess.PIPE,
                                          stdout=input_api.subprocess.PIPE,
                                          stderr=input_api.subprocess.STDOUT,
                                          text=True)
        message, _ = proc.communicate(input=old_contents)
        if proc.returncode != 0:
            if on_upload:
                presubmit_results.append(
                    output_api.PresubmitPromptWarning(message))
            else:
                presubmit_results.append(output_api.PresubmitError(message))
    except Exception as e:
        presubmit_results.append(output_api.PresubmitError(str(e)))

    return presubmit_results


def CheckApiChangesIfModified(input_api, output_api, on_upload):
    os_path = input_api.os_path
    api_file_affected = None
    need_api_check = False
    results = []
    for f in input_api.AffectedFiles():
        if any([
                os_path.normcase(f.LocalPath()).startswith(
                    os_path.normcase(prefix))
                for prefix in TRIGGERING_FILE_PREFIXES
        ]):
            need_api_check = True
        if f.LocalPath() == API_FILE:
            api_file_affected = f
            break

    if need_api_check:
        results.extend(
            CheckApiChanges(input_api, output_api, api_file_affected,
                            on_upload))
    return results


def _CommonChecks(input_api, output_api, on_upload):
    old_path = input_api.sys.path[:]
    try:
        input_api.sys.path.insert(0, "../../../..")
        from chrome.browser.resources.glic.common_checks import GlicCommonChecks
        return sum([
            CheckApiChangesIfModified(input_api, output_api, on_upload),
            GlicCommonChecks(input_api, output_api),
        ], [])
    finally:
        # Restore the original path, or other presubmits may fail.
        input_api.sys.path = old_path


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api, True)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api, False)
