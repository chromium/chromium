# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import json

# Runs PRESUBMIT.py in py3 mode by git cl presubmit.
USE_PYTHON3 = True

DEBUG = False

API_FILE = 'chrome/browser/resources/glic/glic_api/glic_api.ts'
API_GEN_FILE = 'chrome/browser/resources/glic/glic_api/glic_api_generated.ts'

TRIGGERING_FILE_PREFIXES = [
    'chrome/browser/resources/glic/',
]


def _GetOldContents(input_api, local_path):
    for f in input_api.AffectedFiles():
        if f.LocalPath() == local_path:
            return '\n'.join(f.OldContents())
    src_root = input_api.os_path.join(os.getcwd(), '../../../../')
    full_path = input_api.os_path.join(src_root, local_path)
    if os.path.exists(full_path):
        with open(full_path, 'r') as f:
            return f.read()
    return ""


def CheckApiChanges(input_api, output_api, api_file, on_upload):
    skip_compatibility_check = (
        'Bypass-Glic-Api-Compatibility-Check'
        in input_api.change.GitFootersFromDescription()
    )
    src_root = input_api.os_path.join(os.getcwd(), '../../../../')
    api_file_path = input_api.os_path.join(src_root, API_FILE)

    api_dir = input_api.os_path.dirname(API_FILE)
    api_dir_path = input_api.os_path.join(src_root, api_dir)

    filenames = set()
    if os.path.exists(api_dir_path):
        for filename in os.listdir(api_dir_path):
            filenames.add(filename)

    for f in input_api.AffectedFiles():
        if f.LocalPath().startswith(api_dir + '/'):
            filenames.add(input_api.os_path.basename(f.LocalPath()))

    old_files_map = {}
    for filename in filenames:
        if filename.endswith('.ts') and not filename.endswith('.d.ts'):
            local_path = input_api.os_path.join(api_dir, filename).replace(
                '\\', '/'
            )
            old_files_map[local_path] = _GetOldContents(input_api, local_path)
    old_contents = json.dumps(old_files_map)

    cmd = [
        input_api.python_executable,
        input_api.os_path.join(
            input_api.PresubmitLocalPath(), 'presubmit', 'check_api.py'
        ),
        '--old-stdin',
        '--api-file-path=' + api_file_path,
    ]
    if skip_compatibility_check:
        cmd.append('--skip-compatibility-check')

    presubmit_results = []
    try:
        proc = input_api.subprocess.Popen(
            cmd,
            stdin=input_api.subprocess.PIPE,
            stdout=input_api.subprocess.PIPE,
            stderr=input_api.subprocess.STDOUT,
            text=True,
        )
        message, _ = proc.communicate(input=old_contents)
        if proc.returncode != 0:
            if on_upload:
                presubmit_results.append(
                    output_api.PresubmitPromptWarning(message)
                )
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
        if any(
            [
                os_path.normcase(f.LocalPath()).startswith(
                    os_path.normcase(prefix)
                )
                for prefix in TRIGGERING_FILE_PREFIXES
            ]
        ):
            need_api_check = True
        if f.LocalPath() == API_FILE:
            api_file_affected = f
            break

    if need_api_check:
        results.extend(
            CheckApiChanges(input_api, output_api, api_file_affected, on_upload)
        )
    return results


def _CheckTypeScriptImports(input_api, output_api):
    def file_filter(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=[r'^chrome/browser/resources/glic/.*\.ts$'],
            files_to_skip=[r'.*\.d\.ts$'],
        )

    affected_files = list(
        input_api.AffectedFiles(include_deletes=False, file_filter=file_filter)
    )
    file_paths = [f.UnixLocalPath() for f in affected_files]
    if not file_paths:
        return []

    format_ts_imports_script = input_api.os_path.join(
        input_api.change.RepositoryRoot(),
        'chrome',
        'browser',
        'glic',
        'tools',
        'format_ts_imports.py',
    )

    cmd = [
        input_api.python3_executable,
        format_ts_imports_script,
        '--check-only',
    ] + file_paths

    proc = input_api.subprocess.Popen(
        cmd,
        cwd=input_api.change.RepositoryRoot(),
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE,
        text=True,
    )
    _, stderr = proc.communicate()

    if proc.returncode != 0:
        unformatted_files = []
        for line in stderr.splitlines():
            line = line.strip()
            if not line or line.startswith('The following files') or line.startswith('Warning:'):
                continue
            norm_line = input_api.os_path.normpath(line)
            for f in file_paths:
                f_norm = input_api.os_path.normpath(f)
                if f_norm == norm_line or norm_line.endswith(f_norm):
                    unformatted_files.append(f)
                    break
        target_files = unformatted_files or file_paths
        cmd_str = (
            'vpython3 chrome/browser/glic/tools/format_ts_imports.py -i '
            + ' '.join(target_files)
        )
        message = (
            'TypeScript imports in the following file(s) are not properly '
            'formatted. Please run:\n  ' + cmd_str
        )
        return [
            output_api.PresubmitError(
                message,
                items=target_files,
            )
        ]
    return []


def _CommonChecks(input_api, output_api, on_upload):
    old_path = input_api.sys.path[:]
    try:
        input_api.sys.path.insert(0, "../../../..")
        from chrome.browser.resources.glic.common_checks import GlicCommonChecks

        return sum(
            [
                CheckApiChangesIfModified(input_api, output_api, on_upload),
                GlicCommonChecks(input_api, output_api),
                _CheckTypeScriptImports(input_api, output_api),
            ],
            [],
        )
    finally:
        # Restore the original path, or other presubmits may fail.
        input_api.sys.path = old_path


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api, True)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api, False)
