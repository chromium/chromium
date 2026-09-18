# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

_common_checks_ran = False


def ResetCommonChecksForTesting():
    global _common_checks_ran
    _common_checks_ran = False


def GlicCommonChecks(input_api, output_api):
    global _common_checks_ran
    if _common_checks_ran:
        return []
    _common_checks_ran = True

    return (
        _CheckGlicGeneratedApi(input_api, output_api)
        + _CheckRuntimeFeatureChecksIfModified(input_api, output_api)
        + _CheckGlicFeaturesDefaultState(input_api, output_api)
    )


def _CheckGlicGeneratedApi(input_api, output_api):
    monitored_files = (
        'chrome/common/actor_webui.mojom',
        'chrome/common/glic_enums.mojom',
        'chrome/browser/glic/host/glic.mojom',
        'chrome/browser/resources/glic/glic_api_impl/generate.py',
        'chrome/browser/resources/glic/glic_api/glic_api.ts',
        'chrome/browser/resources/glic/glic_api/glic_api_generated.ts',
    )
    repo_root = input_api.change.RepositoryRoot()
    affected_files = [
        input_api.os_path.relpath(f.AbsoluteLocalPath(), repo_root).replace(
            '\\', '/'
        )
        for f in input_api.AffectedFiles()
    ]
    if not any([path in monitored_files for path in affected_files]):
        return []

    os_path = input_api.os_path
    src_root = os_path.join(os.path.dirname(__file__), '../../../..')
    cmd = [
        input_api.python_executable,
        input_api.os_path.join(
            src_root, 'chrome/browser/resources/glic/glic_api_impl/generate.py'
        ),
        '--check-only',
    ]

    try:
        input_api.subprocess.check_output(
            cmd, stderr=input_api.subprocess.STDOUT
        )
    except input_api.subprocess.CalledProcessError as e:
        message = e.output.decode('utf-8')
        return [output_api.PresubmitError(message)]
    return []


def _CheckRuntimeFeatureChecksIfModified(input_api, output_api):
    os_path = input_api.os_path
    src_root = os_path.join(os_path.dirname(__file__), '../../../..')

    for raw_path in input_api.LocalPaths():
        path = raw_path.replace('\\', '/')
        if path.startswith('chrome/browser/resources/glic/glic_api_impl/'):
            break
        if path in (
            'chrome/browser/glic/host/glic.mojom',
            'chrome/browser/resources/glic/presubmit'
            + '/check_runtime_features.py',
        ):
            break
    else:
        return []

    cmd = [
        input_api.python_executable,
        os_path.join(
            src_root,
            'chrome/browser/resources/glic/presubmit/check_runtime_features.py',
        ),
    ]

    try:
        input_api.subprocess.check_output(cmd)
    except input_api.subprocess.CalledProcessError as e:
        message = 'glic check_runtime_features.py failed:\n' + e.output.decode(
            'utf-8'
        )
        return [output_api.PresubmitError(message)]
    return []


def _CheckGlicFeaturesDefaultState(input_api, output_api):
    # TODO: Remove the line skip checks and heuristics when all Glic features
    # are moved into glic/features instead of common features.
    feature_pattern = input_api.re.compile(
        r'\bBASE_(?:RUNTIME_MUTABLE_)?FEATURE\s*\(\s*(\w+)\s*,([\s\S]*?)\);'
    )
    raw_enabled_pattern = input_api.re.compile(
        r'\b(?:base::)?FEATURE_ENABLED_BY_DEFAULT\b'
    )
    errors = []

    for f in input_api.AffectedFiles(include_deletes=False):
        local_path = f.LocalPath().replace('\\', '/')
        if not local_path.endswith(('.cc', '.mm')):
            continue

        is_glic_dir = local_path.startswith('chrome/browser/glic/')
        is_chrome_common = local_path.startswith('chrome/common/')

        if not (is_glic_dir or is_chrome_common):
            continue

        changed_line_numbers = {line_num for line_num, _ in f.ChangedContents()}
        if not changed_line_numbers:
            continue

        lines = list(f.NewContents())
        contents = '\n'.join(lines)

        for match in feature_pattern.finditer(contents):
            feature_name = match.group(1).strip()
            feature_body = match.group(2)

            # If outside chrome/browser/glic, only inspect Glic features.
            if not is_glic_dir and 'Glic' not in feature_name:
                continue

            # Check if bare base::FEATURE_ENABLED_BY_DEFAULT is used.
            if not raw_enabled_pattern.search(feature_body):
                continue

            start_line = contents.count('\n', 0, match.start()) + 1
            end_line = contents.count('\n', 0, match.end()) + 1

            if not changed_line_numbers.intersection(
                range(start_line, end_line + 1)
            ):
                continue

            # Ignore commented out macros.
            if lines[start_line - 1].strip().startswith('//'):
                continue

            errors.append(
                f'{local_path}:{start_line}: Feature "{feature_name}" uses '
                'base::FEATURE_ENABLED_BY_DEFAULT directly.'
            )

    if not errors:
        return []

    message = (
        'Glic feature(s) must explicitly specify default enablement per\n'
        'platform instead of using base::FEATURE_ENABLED_BY_DEFAULT.\n'
        'Please use one of the following macros from '
        'chrome/browser/glic/public/features.h:\n'
        '  - FEATURE_ENABLED_BY_DEFAULT_NON_ANDROID\n'
        '  - FEATURE_ENABLED_BY_DEFAULT_ANDROID_ONLY\n'
        '  - FEATURE_ENABLED_BY_DEFAULT_ALL_PLATFORMS'
    )
    return [output_api.PresubmitError(message, items=errors)]
