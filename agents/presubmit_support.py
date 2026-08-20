# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared code for //agents presubmit-related code."""

import json
import os
import pathlib
import re
import urllib.parse

AGENTS_DIR = pathlib.Path(__file__).parent
CHROMIUM_SRC_DIR = AGENTS_DIR.parent


def get_agents_python_path_entries() -> list[pathlib.Path]:
    """Gets a list of paths to add to PYTHONPATH."""
    return [
        # Needed so that the code in this directory can import neighboring code
        # while maintaining compatibility with pytype. The hyphens in skill
        # names affect pytype's ability to resolve paths for relative imports.
        (
            CHROMIUM_SRC_DIR
            / 'agents'
            / 'skills'
            / 'analyzing-sql-traces'
            / 'scripts'
        ),
        (
            CHROMIUM_SRC_DIR
            / 'agents'
            / 'skills'
            / 'multi-agent-engineering-workflow'
        ),
        # Python code under //agents uses (or should use) fully qualified
        # imports relative to the Chromium src directory wherever possible.
        CHROMIUM_SRC_DIR,
    ]


# Regex to find relative links: [text](link)
MARKDOWN_LINK_RE = re.compile(
    r"""\[([^\]]+)\]\(\s*"""
    r"""(?:\<((?!https?://|mailto:)[^>]+)\>|((?!https?://|mailto:)[^\s)]+))"""
    r"""\s*(?:\s+["'].*?["'])?\s*\)"""
)


def CheckSkillMarkdownLinks(input_api, output_api):
    """Enforces valid relative links across all markdown documentation."""
    results = []
    skill_dir = input_api.PresubmitLocalPath()
    repo_root = input_api.change.RepositoryRoot()

    def file_filter(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=(r'.*\.md$',),
        )

    affected_md_files = {
        f.AbsoluteLocalPath(): f
        for f in input_api.AffectedSourceFiles(file_filter)
    }

    all_md_files = []
    for root, dirs, files in os.walk(skill_dir):
        dirs[:] = [d for d in dirs if d not in ('.temp', '__pycache__')]
        for file in files:
            if file.endswith('.md'):
                all_md_files.append(os.path.join(root, file))

    for md_file in all_md_files:
        affected_file = affected_md_files.get(md_file)
        is_modified = affected_file is not None
        content = None
        if is_modified:
            content = input_api.ReadFile(affected_file)
        else:
            try:
                with open(md_file, 'r', encoding='utf-8') as f:
                    content = f.read()
            except (OSError, ValueError):
                continue

        if not content:
            continue

        for line_num, line in enumerate(content.splitlines(), start=1):
            for match in MARKDOWN_LINK_RE.finditer(line):
                link_text = match.group(1)
                link_target = match.group(2) or match.group(3)

                parsed = urllib.parse.urlparse(link_target)
                if parsed.scheme or link_target.startswith(
                    ('path/to/', 'example/')
                ):
                    continue

                if link_target.startswith('//'):
                    repo_relative_path = parsed.netloc + parsed.path
                    unquoted_path = urllib.parse.unquote(repo_relative_path)
                    target_path = os.path.normpath(
                        os.path.join(repo_root, unquoted_path)
                    )
                elif parsed.path:
                    unquoted_path = urllib.parse.unquote(parsed.path)
                    if unquoted_path.startswith('/'):
                        target_path = os.path.normpath(
                            os.path.join(repo_root, unquoted_path[1:])
                        )
                    else:
                        target_path = os.path.normpath(
                            os.path.join(
                                os.path.dirname(md_file), unquoted_path
                            )
                        )
                else:
                    continue

                if not os.path.exists(target_path):
                    msg = (
                        f'Broken link in {os.path.relpath(md_file, skill_dir)}:'
                        f'{line_num}: [{link_text}]({link_target}) -> '
                        f'Target does not exist: {target_path}'
                    )
                    if is_modified:
                        results.append(output_api.PresubmitError(msg))
                    else:
                        results.append(output_api.PresubmitPromptWarning(msg))
    return results


def CheckSkillJsonFiles(input_api, output_api, check_personas=False):
    """Enforces syntax and basic structural checks on JSON files."""
    results = []
    skill_dir = input_api.PresubmitLocalPath()

    def file_filter(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=(r'.*\.json$',),
        )

    affected_json_files = {
        f.AbsoluteLocalPath(): f
        for f in input_api.AffectedSourceFiles(file_filter)
    }

    all_json_files = []
    for root, dirs, files in os.walk(skill_dir):
        dirs[:] = [d for d in dirs if d not in ('.temp', '__pycache__')]
        for file in files:
            if file.endswith('.json'):
                all_json_files.append(os.path.join(root, file))

    for json_path in all_json_files:
        affected_file = affected_json_files.get(json_path)
        is_modified = affected_file is not None
        content = None
        if is_modified:
            content = input_api.ReadFile(affected_file)
        else:
            try:
                with open(json_path, 'r', encoding='utf-8') as f:
                    content = f.read()
            except (OSError, ValueError):
                continue

        if not content:
            continue

        try:
            data = json.loads(content)
            if check_personas and 'personas' in pathlib.Path(json_path).parts:
                rel_path = os.path.relpath(json_path, skill_dir)
                if not isinstance(data, dict):
                    msg = f'Persona JSON {rel_path} must be a dictionary'
                    if is_modified:
                        results.append(output_api.PresubmitError(msg))
                    else:
                        results.append(output_api.PresubmitPromptWarning(msg))
                    continue

                required_keys = {'role', 'mandate', 'checklist'}
                missing = required_keys - data.keys()
                if missing:
                    msg = (
                        f"Persona JSON {rel_path} is missing required keys:"
                        f" {', '.join(sorted(missing))}"
                    )
                    if is_modified:
                        results.append(output_api.PresubmitError(msg))
                    else:
                        results.append(output_api.PresubmitPromptWarning(msg))
        except ValueError as e:
            rel_path = os.path.relpath(json_path, skill_dir)
            msg = f'Invalid JSON in {rel_path}: {e}'
            if is_modified:
                results.append(output_api.PresubmitError(msg))
            else:
                results.append(output_api.PresubmitPromptWarning(msg))
    return results


def CheckSkillPresubmit(input_api, output_api, check_personas=False):
    """Common entrance point for skill PRESUBMIT scripts."""
    results = []
    results.extend(CheckSkillMarkdownLinks(input_api, output_api))
    results.extend(
        CheckSkillJsonFiles(
            input_api, output_api, check_personas=check_personas
        )
    )
    return results


def get_agents_env(input_api):
    """Gets the common environment for running agents tests."""
    python_path = [str(p) for p in get_agents_python_path_entries()]
    existing_python_path = input_api.environ.get('PYTHONPATH', '')
    if existing_python_path:
        python_path.append(existing_python_path)
    env = dict(input_api.environ)
    env.update(
        {
            'PYTHONPATH': input_api.os_path.pathsep.join(python_path),
            'PYTHONDONTWRITEBYTECODE': '1',
        }
    )
    return env
