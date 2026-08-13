# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for multi-agent-skill-creator.

This script enforces structural integrity and formatting for the skill creator.
"""

import os
import sys


def CheckTemplateSyntax(input_api, output_api):
    results = []
    skill_dir = input_api.PresubmitLocalPath()
    templates_dir = os.path.join(skill_dir, 'templates')

    if not os.path.exists(templates_dir):
        return []

    def FileFilter(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=(r'.*\.py\.template$',),
        )

    # Precompute affected files map for O(1) lookups
    affected_template_files = {
        f.AbsoluteLocalPath(): f
        for f in input_api.AffectedSourceFiles(FileFilter)
    }

    for root, dirs, files in os.walk(templates_dir):
        dirs[:] = [d for d in dirs if d not in ('.temp', '__pycache__')]
        for file in files:
            if file.endswith('.py.template'):
                template_path = os.path.join(root, file)
                affected_file = affected_template_files.get(template_path)
                is_modified = affected_file is not None
                source = None
                if is_modified:
                    source = input_api.ReadFile(affected_file)
                else:
                    try:
                        with open(template_path, 'r', encoding='utf-8') as f:
                            source = f.read()
                    except (OSError, ValueError):
                        continue

                if not source:
                    continue

                try:
                    # Compile in memory to check syntax
                    compile(source, template_path, 'exec')
                except Exception as e:
                    results.append(
                        output_api.PresubmitError(
                            f'Template syntax error in'
                            f' {os.path.relpath(template_path, skill_dir)}: {e}'
                        )
                    )
    return results


def _CommonChecks(input_api, output_api):
    repo_root = input_api.change.RepositoryRoot()
    sys_path_added = False
    if repo_root not in sys.path:
        sys.path.insert(0, repo_root)
        sys_path_added = True
    try:
        # pylint: disable=import-outside-toplevel
        from agents.presubmit_support import CheckSkillPresubmit

        # pylint: enable=import-outside-toplevel
        results = []
        results.extend(CheckSkillPresubmit(input_api, output_api))
        results.extend(CheckTemplateSyntax(input_api, output_api))
        return results
    finally:
        if sys_path_added:
            sys.path.remove(repo_root)


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
