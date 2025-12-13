# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for agents/prompts.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckPrompts(input_api, output_api):
    """Checks that all .md files are up-to-date with their .tmpl.md sources."""
    script_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                         'process_prompts.py')
    cmd = [input_api.python_executable, script_path, '--check']
    p = input_api.subprocess.Popen(cmd,
                                   stdout=input_api.subprocess.PIPE,
                                   stderr=input_api.subprocess.PIPE,
                                   encoding='utf-8')
    stdout, stderr = p.communicate()
    if p.returncode != 0:
        error_message = (
            'Found stale prompt files. Please run '
            '`agents/prompts/process_prompts.py` to update them.\n'
            f'stdout:\n{stdout}\n'
            f'stderr:\n{stderr}\n')
        return [output_api.PresubmitError(error_message)]
    return []
