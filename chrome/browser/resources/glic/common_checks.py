# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

_common_checks_ran = False


def ResetCommonChecksForTesting():
    global _common_checks_ran
    _common_checks_ran = False


def _AffectedFiles(input_api, include_deletes=True):
    # GlicCommonChecks runs once, from whichever glic PRESUBMIT.py runs first.
    # input_api.AffectedFiles() would only list files under that PRESUBMIT.py's
    # directory, so list every file in the CL instead.
    return input_api.change.AffectedFiles(include_deletes=include_deletes)


def GlicCommonChecks(input_api, output_api):
    global _common_checks_ran
    if _common_checks_ran:
        return []
    _common_checks_ran = True

    return (
        _CheckGlicGeneratedApi(input_api, output_api)
        + _CheckRuntimeFeatureChecksIfModified(input_api, output_api)
        + _CheckGlicFeaturesDefaultState(input_api, output_api)
        + _CheckGlicInvocationSource(input_api, output_api)
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
        for f in _AffectedFiles(input_api)
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

    for f in _AffectedFiles(input_api):
        path = f.LocalPath().replace('\\', '/')
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

    for f in _AffectedFiles(input_api, include_deletes=False):
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

            # Flag bare base::FEATURE_ENABLED_BY_DEFAULT without explicit
            # platform BUILDFLAG conditionals or a platform-scope comment.
            if (
                not raw_enabled_pattern.search(feature_body)
                or 'BUILDFLAG(' in feature_body
            ):
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

            if _HasDefaultEnabledPlatformsComment(lines, start_line):
                continue

            errors.append(
                f'{local_path}:{start_line}: Feature "{feature_name}" uses '
                'base::FEATURE_ENABLED_BY_DEFAULT without specifying '
                'platform scope.'
            )

    if not errors:
        return []
    # Don't point at the FEATURE_ENABLED_BY_DEFAULT_* macros in features.h:
    # //PRESUBMIT.py (CheckBaseFeatureMacro) rejects them for new features
    # because tools/metrics/common/cpp_feature_parser.py can't resolve them.
    # See b/582871743.
    message = (
        'Glic feature(s) use bare base::FEATURE_ENABLED_BY_DEFAULT without\n'
        'specifying intended platforms. Confirm the target platforms in a\n'
        'comment directly above the feature:\n'
        f'  {_DEFAULT_ENABLED_PLATFORMS_PREFIX} <platforms and reason>\n'
        '  (e.g. "all (killswitch for ...)", "Android only (file is only '
        'built on Android)", or "desktop only (not built on Android)")\n'
        'Or, if the default differs across platforms in a cross-platform\n'
        'file, use an #if BUILDFLAG(...) / #else block with literal\n'
        'base::FEATURE_ENABLED_BY_DEFAULT /\n'
        'base::FEATURE_DISABLED_BY_DEFAULT values.'
    )
    return [output_api.PresubmitError(message, items=errors)]


_DEFAULT_ENABLED_PLATFORMS_PREFIX = '// Default-enabled platforms:'


def _HasDefaultEnabledPlatformsComment(lines, start_line):
    """Whether the comment block directly above line `start_line` (1-based)
    has a non-empty "// Default-enabled platforms: <...>" line."""
    index = start_line - 2
    while index >= 0:
        line = lines[index].strip()
        if not line.startswith('//'):
            return False
        if line.startswith(_DEFAULT_ENABLED_PLATFORMS_PREFIX) and line[
            len(_DEFAULT_ENABLED_PLATFORMS_PREFIX):
        ].strip():
            return True
        index -= 1
    return False


def _CheckGlicInvocationSource(input_api, output_api):
    enum_pattern = input_api.re.compile(
        r'\benum\s+InvocationSource\s*\{([\s\S]*?)\};'
    )

    for f in _AffectedFiles(input_api, include_deletes=False):
        local_path = f.LocalPath().replace('\\', '/')
        if local_path != 'chrome/common/glic_enums.mojom':
            continue

        changed_line_numbers = {line_num for line_num, _ in f.ChangedContents()}
        if not changed_line_numbers:
            continue

        contents = '\n'.join(f.NewContents())
        match = enum_pattern.search(contents)
        if not match:
            continue

        start_line = contents.count('\n', 0, match.start()) + 1
        end_line = contents.count('\n', 0, match.end()) + 1

        if not changed_line_numbers.intersection(
            range(start_line, end_line + 1)
        ):
            continue

        warning_message = (
            'You modified InvocationSource in chrome/common/glic_enums.mojom.\n'
            'The matching google3 enum and mapping must be updated too.\n'
            'See go/glic-invocation-source for the files to change.'
        )
        return [output_api.PresubmitPromptWarning(warning_message)]

    return []
