# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

_common_checks_ran = False


def GlicCommonChecks(input_api, output_api):
    global _common_checks_ran
    if _common_checks_ran:
        return []
    _common_checks_ran = True

    return (_CheckGlicGeneratedApi(input_api, output_api) +
            _CheckRuntimeFeatureChecksIfModified(input_api, output_api))


def _CheckGlicGeneratedApi(input_api, output_api):
    monitored_files = (
        'chrome/common/actor_webui.mojom',
        'chrome/browser/glic/host/glic.mojom',
        'chrome/browser/resources/glic/glic_api_impl/generate.py',
        'chrome/browser/resources/glic/glic_api/glic_api.ts',
    )
    if not any(
        [f.LocalPath() in monitored_files for f in input_api.AffectedFiles()]):
        return []

    os_path = input_api.os_path
    src_root = os_path.join(os.path.dirname(__file__), '../../../..')
    cmd = [
        input_api.python_executable,
        input_api.os_path.join(
            src_root,
            'chrome/browser/resources/glic/glic_api_impl/generate.py'),
        '--check-only',
    ]

    try:
        input_api.subprocess.check_output(cmd,
                                          stderr=input_api.subprocess.STDOUT)
    except input_api.subprocess.CalledProcessError as e:
        message = e.output.decode('utf-8')
        return [output_api.PresubmitError(message)]
    return []


def _CheckRuntimeFeatureChecksIfModified(input_api, output_api):
    MONITORED_FILES = set((
        'chrome/browser/resources/glic/glic_api_impl/client/glic_api_client.ts',
        'chrome/browser/glic/host/glic.mojom',
        'chrome/browser/resources/glic/presubmit/check_runtime_features.py',
    ))

    os_path = input_api.os_path
    src_root = os_path.join(os.path.dirname(__file__), '../../../..')

    if not (set(input_api.UnixLocalPaths()) & MONITORED_FILES):
        return []

    cmd = [
        input_api.python_executable,
        os_path.join(
            src_root,
            'chrome/browser/resources/glic/presubmit/check_runtime_features.py'
        ),
    ]

    try:
        input_api.subprocess.check_output(cmd)
    except input_api.subprocess.CalledProcessError as e:
        message = ('glic check_runtime_features.py failed:\n' +
                   e.output.decode('utf-8'))
        return [output_api.PresubmitError(message)]
    return []
