# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for //agents.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

# Extra paths that should be added to PYTHONPATH when running pylint, i.e.
# dependencies on other Chromium Python code.
PYLINT_EXTRA_PATHS_COMPONENTS = [
    ('build', 'util'),
]


def _GetChromiumSrcPath(input_api):
    """Returns the path to the Chromium src directory."""
    return input_api.os_path.realpath(
        input_api.os_path.join(input_api.PresubmitLocalPath(), '..'))


def CheckPylint(input_api, output_api):
    """Runs pylint on all directory content and subdirectories."""
    chromium_src_path = _GetChromiumSrcPath(input_api)
    pylint_extra_paths = [
        input_api.os_path.join(chromium_src_path, *component)
        for component in PYLINT_EXTRA_PATHS_COMPONENTS
    ]
    return input_api.RunTests(
        input_api.canned_checks.GetPylint(
            input_api,
            output_api,
            extra_paths_list=pylint_extra_paths,
            version='3.2',
            disabled_warnings=[
                # Often produces non-actionable warnings.
                'duplicate-code',
            ],
            files_to_skip=[r'.*/cipd/.*'],
        ))


def CheckPromptfooTestCases(input_api, output_api):
    """Checks that promptfoo.yaml files are valid."""
    promptfoo_files = [
        f.AbsoluteLocalPath()
        for f in input_api.AffectedFiles(include_deletes=False)
        if f.LocalPath().endswith('.promptfoo.yaml')
    ]

    if not promptfoo_files:
        return []

    linter_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                         'testing',
                                         'lint_promptfoo_testcases.py')
    # The linter uses third-party libraries, so it must be run with vpython3.
    cmd = ['vpython3', linter_path] + promptfoo_files
    try:
        p = input_api.subprocess.Popen(cmd,
                                       stdout=input_api.subprocess.PIPE,
                                       stderr=input_api.subprocess.PIPE)
        stdout, stderr = p.communicate()
    except OSError as e:
        return [
            output_api.PresubmitError(f'Failed to run promptfoo linter: {e}.\n'
                                      'Is vpython3 in your PATH?')
        ]

    if p.returncode != 0:
        # The linter script prints errors to stderr.
        message = (f'Promptfoo linter ({linter_path}) failed with exit code '
                   f'{p.returncode}.')
        long_text = (f'STDOUT:\n{stdout.decode("utf-8", "replace")}\n'
                     f'STDERR:\n{stderr.decode("utf-8", "replace")}\n')
        return [output_api.PresubmitError(message, long_text=long_text)]

    return []
