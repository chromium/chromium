# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for multi-agent-skill-creator.

This script enforces structural integrity and formatting for the skill creator.
"""

import os
import re
import urllib.parse

# Regex to find relative links: [text](link)
# Handles standard links, angle brackets <link>, and titles "title"
MARKDOWN_LINK_RE = re.compile(
    r"""\[([^\]]+)\]\(\s*"""
    r"""(?:\<((?!https?://|mailto:)[^>]+)\>|((?!https?://|mailto:)[^\s)]+))"""
    r"""\s*(?:\s+["'].*?["'])?\s*\)""")


def CheckMarkdownLinks(input_api, output_api):
    results = []
    skill_dir = input_api.PresubmitLocalPath()
    repo_root = input_api.change.RepositoryRoot()

    def FileFilter(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=(r'.*\.md$', ),
        )

    # Precompute affected files map for O(1) lookups
    affected_md_files = {
        f.AbsoluteLocalPath(): f
        for f in input_api.AffectedSourceFiles(FileFilter)
    }

    # We check all markdown files in the directory
    all_md_files = []
    for root, dirs, files in os.walk(skill_dir):
        # Prune unwanted directories in-place
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

                # Parse link to strip queries/anchors and check scheme
                parsed = urllib.parse.urlparse(link_target)
                if parsed.scheme:
                    # External link, skip
                    continue

                if link_target.startswith('//'):
                    # Reconstruct path relative to repo root (netloc + path)
                    repo_relative_path = parsed.netloc + parsed.path
                    unquoted_path = urllib.parse.unquote(repo_relative_path)
                    target_path = os.path.normpath(
                        os.path.join(repo_root, unquoted_path))
                elif parsed.path:
                    unquoted_path = urllib.parse.unquote(parsed.path)
                    if unquoted_path.startswith('/'):
                        # Relative to repo root
                        target_path = os.path.normpath(
                            os.path.join(repo_root, unquoted_path[1:]))
                    else:
                        # Relative to current file
                        target_path = os.path.normpath(
                            os.path.join(os.path.dirname(md_file),
                                         unquoted_path))
                else:
                    # Internal anchor link (e.g., #foo), skip
                    continue

                if not os.path.exists(target_path):
                    msg = (
                        f"Broken link in "
                        f"{os.path.relpath(md_file, skill_dir)}:{line_num}: "
                        f"[{link_text}]({link_target}) -> "
                        f"Target does not exist: {target_path}")
                    if is_modified:
                        results.append(output_api.PresubmitError(msg))
                    else:
                        results.append(output_api.PresubmitPromptWarning(msg))
    return results


def CheckTemplateSyntax(input_api, output_api):
    results = []
    skill_dir = input_api.PresubmitLocalPath()
    templates_dir = os.path.join(skill_dir, 'templates')

    if not os.path.exists(templates_dir):
        return []

    def FileFilter(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=(r'.*\.py\.template$', ),
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
                            f"Template syntax error in "
                            f"{os.path.relpath(template_path, skill_dir)}: {e}"
                        ))
    return results


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(CheckMarkdownLinks(input_api, output_api))
    results.extend(CheckTemplateSyntax(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(CheckMarkdownLinks(input_api, output_api))
    results.extend(CheckTemplateSyntax(input_api, output_api))
    return results
