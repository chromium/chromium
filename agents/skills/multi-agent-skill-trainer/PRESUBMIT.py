# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for multi-agent-skill-trainer."""

import sys


def CheckChangeOnUpload(input_api, output_api):
    repo_root = input_api.change.RepositoryRoot()
    if repo_root not in sys.path:
        sys.path.insert(0, repo_root)
    # pylint: disable=import-outside-toplevel
    from agents.presubmit_support import CheckSkillPresubmit

    # pylint: enable=import-outside-toplevel
    return CheckSkillPresubmit(input_api, output_api, check_personas=True)


def CheckChangeOnCommit(input_api, output_api):
    repo_root = input_api.change.RepositoryRoot()
    if repo_root not in sys.path:
        sys.path.insert(0, repo_root)
    # pylint: disable=import-outside-toplevel
    from agents.presubmit_support import CheckSkillPresubmit

    # pylint: enable=import-outside-toplevel
    return CheckSkillPresubmit(input_api, output_api, check_personas=True)
