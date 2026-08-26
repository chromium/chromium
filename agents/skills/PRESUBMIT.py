# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for //agents/skills.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def _EnsureRepoRootInSysPath(input_api):
    repo_root = input_api.change.RepositoryRoot()
    if repo_root not in input_api.sys.path:
        input_api.sys.path.insert(0, repo_root)


def CheckPythonTests(input_api, output_api):
    _EnsureRepoRootInSysPath(input_api)
    from agents import presubmit_support  # pylint: disable=import-outside-toplevel

    return input_api.RunTests(
        input_api.canned_checks.GetUnitTestsRecursively(
            input_api,
            output_api,
            input_api.PresubmitLocalPath(),
            files_to_check=[r'.+_(?:unit)?test\.py$'],
            files_to_skip=[],
            env=presubmit_support.get_agents_env(input_api),
        )
    )


def CheckSkills(input_api, output_api):
    _EnsureRepoRootInSysPath(input_api)
    from agents.presubmit_support import CheckSkillPresubmit  # pylint: disable=import-outside-toplevel

    return CheckSkillPresubmit(input_api, output_api, check_personas=True)
