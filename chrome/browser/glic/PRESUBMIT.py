# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for chrome/browser/glic.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def _CheckHeaderOrdering(input_api, output_api):
    include_pattern = input_api.re.compile(r'^\s*#\s*(?:include|import)\b')

    def file_filter(affected_file):
        if not input_api.FilterSourceFile(
                affected_file,
                files_to_check=[r'^chrome/browser/glic/.*\.(cc|h|mm)$'],
        ):
            return False
        return any(
            include_pattern.match(line)
            for _, line in affected_file.ChangedContents())

    affected_files = list(
        input_api.AffectedFiles(include_deletes=False,
                                file_filter=file_filter))
    file_paths = [f.UnixLocalPath() for f in affected_files]
    if not file_paths:
        return []

    sort_headers_script = input_api.os_path.join(
        input_api.PresubmitLocalPath(),
        'tools',
        'sort_headers.py',
    )

    cmd = [input_api.python3_executable, sort_headers_script, '--check-only'
           ] + file_paths
    proc = input_api.subprocess.Popen(
        cmd,
        cwd=input_api.change.RepositoryRoot(),
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE,
        text=True,
    )
    stdout, _ = proc.communicate()

    if proc.returncode != 0:
        unsorted_files = [
            line.strip() for line in stdout.splitlines() if line.strip()
        ]
        target_files = unsorted_files or file_paths
        cmd_str = ('python3 chrome/browser/glic/tools/sort_headers.py ' +
                   ' '.join(target_files))
        message = ('The C++ header includes in the following file(s) ' +
                   'are not properly sorted. Please run:\n  ' + cmd_str)
        return [
            output_api.PresubmitPromptWarning(
                message,
                items=target_files,
            )
        ]
    return []


def CheckChangeOnUpload(input_api, output_api):
    return _CheckHeaderOrdering(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CheckHeaderOrdering(input_api, output_api)
