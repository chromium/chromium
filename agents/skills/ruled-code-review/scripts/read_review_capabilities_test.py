#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import json
import pathlib
import re
import subprocess
import textwrap
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

from read_review_capabilities import CapabilityGroup
from read_review_capabilities import format_capabilities
from read_review_capabilities import git_diff
from read_review_capabilities import parse_unified_diff
from read_review_capabilities import resolve_capabilities


class TestCapabilityGroup(unittest.TestCase):
    def test_no_filters(self):
        group = CapabilityGroup()
        self.assertTrue(
            group.matches(pathlib.PurePosixPath('src/foo.cc'), ['any line'])
        )
        self.assertTrue(group.matches(pathlib.PurePosixPath('src/bar.py'), []))

    def test_path_expression(self):
        group = CapabilityGroup(
            path_expression=[
                re.compile(r'src/.*\.cc'),
                re.compile(r'src/.*\.h'),
            ],
        )
        self.assertTrue(group.matches(pathlib.PurePosixPath('src/foo.cc'), []))
        self.assertTrue(group.matches(pathlib.PurePosixPath('src/foo.h'), []))
        self.assertFalse(group.matches(pathlib.PurePosixPath('src/foo.py'), []))

    def test_path_expression_exclusion(self):
        group = CapabilityGroup(
            path_expression=[re.compile(r'src/.*\.cc')],
            path_expression_exclusion=[re.compile(r'src/.*_test\.cc')],
        )
        self.assertTrue(group.matches(pathlib.PurePosixPath('src/foo.cc'), []))
        self.assertFalse(
            group.matches(pathlib.PurePosixPath('src/foo_test.cc'), [])
        )

    def test_content_patterns(self):
        group = CapabilityGroup(
            content_patterns=[
                re.compile(r'TODO\b'),
                re.compile(r'FIXME\b'),
            ],
        )
        file_path = pathlib.PurePosixPath('src/foo.cc')
        self.assertTrue(
            group.matches(file_path, ['int a = 1;', '// TODO: fix this'])
        )
        self.assertTrue(group.matches(file_path, ['// FIXME: quick fix']))
        self.assertFalse(group.matches(file_path, ['int a = 1;']))

    def test_content_patterns_with_empty_diff(self):
        group = CapabilityGroup(content_patterns=[re.compile(r'TODO\b')])
        self.assertFalse(group.matches(pathlib.PurePosixPath('src/foo.cc'), []))

    def test_path_and_content_patterns(self):
        group = CapabilityGroup(
            path_expression=[re.compile(r'src/.*\.cc')],
            content_patterns=[re.compile(r'TODO\b')],
        )
        self.assertTrue(
            group.matches(pathlib.PurePosixPath('src/foo.cc'), ['// TODO: fix'])
        )
        self.assertFalse(
            group.matches(pathlib.PurePosixPath('src/foo.cc'), ['int a = 1;'])
        )
        self.assertFalse(
            group.matches(pathlib.PurePosixPath('src/foo.py'), ['// TODO: fix'])
        )


@mock.patch('subprocess.Popen')
class TestGitDiff(unittest.TestCase):
    def test_git_diff_ref_formats(self, mock_popen):
        repo_root = pathlib.Path('/repo')
        git_diff(repo_root, base='origin/main')
        self.assertEqual(mock_popen.call_args[0][0][7], 'origin/main...')

        git_diff(repo_root, base='HEAD~1..HEAD')
        self.assertEqual(mock_popen.call_args[0][0][7], 'HEAD~1..HEAD')

        git_diff(repo_root, base='HEAD~1...HEAD')
        self.assertEqual(mock_popen.call_args[0][0][7], 'HEAD~1...HEAD')

    def test_git_diff_non_utf8_bytes(self, mock_popen):
        repo_root = pathlib.Path('/repo')
        git_diff(repo_root, base='HEAD~1')
        kwargs = mock_popen.call_args.kwargs
        self.assertEqual(kwargs.get('encoding'), 'utf-8')
        self.assertEqual(kwargs.get('errors'), 'replace')

        raw_diff = (
            b'--- a/latin1.txt\n'
            b'+++ b/latin1.txt\n'
            b'@@ -0,0 +1 @@\n'
            b'+trademark \x99 symbol\n'
        )
        stream = io.TextIOWrapper(
            io.BytesIO(raw_diff),
            encoding=kwargs.get('encoding') or 'utf-8',
            errors=kwargs.get('errors'),
        )
        self.assertEqual(
            parse_unified_diff(stream),
            {pathlib.PurePosixPath('latin1.txt'): ['trademark \ufffd symbol']},
        )


class TestParseUnifiedDiff(unittest.TestCase):
    def test_empty_stream(self):
        added_lines_by_path = parse_unified_diff(io.StringIO(''))
        self.assertEqual(added_lines_by_path, {})

    def test_multiple_files(self):
        diff = textwrap.dedent("""\
            diff --git a/dev/null b/added.py
            --- /dev/null
            +++ b/added.py
            @@ -0,0 +1,3 @@
            +def added():
            +    pass
            diff --git a/deleted.py b/dev/null
            --- a/deleted.py
            +++ /dev/null
            @@ -1,2 +0,0 @@
            -def old():
            -    pass
            diff --git a/modified.cc b/modified.cc
            --- a/modified.cc
            +++ b/modified.cc
            @@ -1,2 +1,4 @@
             void keep();
            -void remove();
            +    indent();
            +
            """)
        added_lines_by_path = parse_unified_diff(io.StringIO(diff))
        self.assertEqual(
            added_lines_by_path,
            {
                pathlib.PurePosixPath('added.py'): [
                    'def added():',
                    '    pass',
                ],
                pathlib.PurePosixPath('modified.cc'): [
                    '    indent();',
                    '',
                ],
            },
        )

    def test_crlf_line_endings(self):
        diff = (
            '--- a/win.cc\r\n'
            '+++ b/win.cc\r\n'
            '@@ -1 +1,2 @@\r\n'
            ' context\r\n'
            '+added line\r\n'
        )
        added_lines_by_path = parse_unified_diff(io.StringIO(diff))
        self.assertEqual(
            added_lines_by_path,
            {pathlib.PurePosixPath('win.cc'): ['added line']},
        )


@mock.patch('subprocess.Popen')
class TestResolveCapabilities(unittest.TestCase):
    def test_resolve_capabilities(self, mock_popen):
        repo_root = pathlib.Path('/repo')
        mock_proc = mock.Mock(
            stdout=io.StringIO(
                json.dumps(
                    {
                        'dirs': {
                            'src/a': {
                                'agenticReview': {
                                    'capabilityGroups': [
                                        {
                                            'pathExpression': [r'.*\.cc'],
                                            'rules': ['rules/style.md'],
                                            'reviewCapabilitySkills': [
                                                'skills/cc-review',
                                            ],
                                        },
                                        {
                                            'pathExpression': [r'.*\.py'],
                                            'rules': ['rules/python.md'],
                                        },
                                    ],
                                },
                            },
                        },
                    }
                )
            )
        )
        mock_popen.return_value = contextlib.nullcontext(mock_proc)

        added_lines_by_path = {
            pathlib.PurePosixPath('src/a/file1.cc'): ['int x = 1;'],
            pathlib.PurePosixPath('src/a/file2.cc'): ['int y = 2;'],
            pathlib.PurePosixPath('src/b/file3.h'): ['void foo();'],
        }
        group = resolve_capabilities(repo_root, added_lines_by_path)

        mock_popen.assert_called_once_with(
            [
                'dirmd',
                'read',
                '-form',
                'sparse',
                repo_root / 'src' / 'a',
                repo_root / 'src' / 'b',
            ],
            cwd=repo_root,
            stdout=subprocess.PIPE,
            text=True,
        )
        self.assertEqual(
            group.rules,
            {pathlib.PurePosixPath('rules/style.md')},
        )
        self.assertEqual(
            group.skills,
            {pathlib.PurePosixPath('skills/cc-review')},
        )

    def test_exclude_rules_and_skills(self, mock_popen):
        repo_root = pathlib.Path('/repo')
        mock_proc = mock.Mock(
            stdout=io.StringIO(
                json.dumps(
                    {
                        'dirs': {
                            'src/a': {
                                'agenticReview': {
                                    'capabilityGroups': [
                                        {
                                            'rules': [
                                                'rules/rule1.md',
                                                'rules/rule2.md',
                                            ],
                                            'reviewCapabilitySkills': [
                                                'skills/skill1',
                                                'skills/skill2',
                                            ],
                                        },
                                    ],
                                    'excludeRules': [
                                        'rules/rule2.md',
                                        'rules/nonexistent.md',
                                    ],
                                    'excludeSkills': [
                                        'skills/skill2',
                                        'skills/nonexistent',
                                    ],
                                },
                            },
                        },
                    }
                )
            )
        )
        mock_popen.return_value = contextlib.nullcontext(mock_proc)

        added_lines_by_path = {pathlib.PurePosixPath('src/a/file.cc'): []}
        group = resolve_capabilities(repo_root, added_lines_by_path)

        mock_popen.assert_called_once_with(
            [
                'dirmd',
                'read',
                '-form',
                'sparse',
                repo_root / 'src' / 'a',
            ],
            cwd=repo_root,
            stdout=subprocess.PIPE,
            text=True,
        )
        self.assertEqual(
            group.rules,
            {pathlib.PurePosixPath('rules/rule1.md')},
        )
        self.assertEqual(
            group.skills,
            {pathlib.PurePosixPath('skills/skill1')},
        )

    def test_empty_diff(self, mock_popen):
        repo_root = pathlib.Path('/repo')
        mock_proc = mock.Mock(stdout=io.StringIO(json.dumps({})))
        mock_popen.return_value = contextlib.nullcontext(mock_proc)

        group = resolve_capabilities(repo_root, {})

        mock_popen.assert_called_once_with(
            ['dirmd', 'read', '-form', 'sparse'],
            cwd=repo_root,
            stdout=subprocess.PIPE,
            text=True,
        )
        self.assertEqual(group.rules, set())
        self.assertEqual(group.skills, set())


class TestFormatCapabilities(fake_filesystem_unittest.TestCase):
    def setUp(self):
        self.setUpPyfakefs()
        self.repo_root = pathlib.Path('/repo')

    def test_format_rules_and_skills(self):
        self.fs.create_file(
            '/repo/rules/sample.md',
            contents='**Rule**: Use weak pointers.',
        )
        self.fs.create_file(
            '/repo/skills/my-skill/SKILL.md',
            contents=textwrap.dedent("""\
                ---
                name: my-skill
                description: A sample review skill.
                ---
                # My Skill Body
                """),
        )

        group = CapabilityGroup(
            rules={pathlib.PurePosixPath('rules/sample.md')},
            skills={pathlib.PurePosixPath('skills/my-skill/SKILL.md')},
        )
        output = format_capabilities(self.repo_root, group)
        expected = textwrap.dedent("""\
            # Review Capabilities

            ## Rules

            ### rules/sample.md

            **Rule**: Use weak pointers.

            ## Skills

            ### my-skill
            - Path: skills/my-skill/SKILL.md
            - Description: A sample review skill.
            """)
        self.assertEqual(output, expected)

    def test_format_skill_without_description(self):
        self.fs.create_file(
            '/repo/skills/no-desc/SKILL.md',
            contents=textwrap.dedent("""\
                ---
                name: no-desc-skill
                ---
                # Body
                """),
        )

        group = CapabilityGroup(
            skills={pathlib.PurePosixPath('skills/no-desc/SKILL.md')},
        )
        output = format_capabilities(self.repo_root, group)
        expected = textwrap.dedent("""\
            # Review Capabilities

            ## Skills

            ### no-desc-skill
            - Path: skills/no-desc/SKILL.md
            """)
        self.assertEqual(output, expected)

    def test_format_no_capabilities(self):
        group = CapabilityGroup()
        self.assertEqual(
            format_capabilities(self.repo_root, group),
            'No review capabilities found.',
        )

    def test_format_missing_rule_file(self):
        group = CapabilityGroup(
            rules={pathlib.PurePosixPath('rules/missing.md')},
        )
        output = format_capabilities(self.repo_root, group)
        self.assertIn('Error reading rule file:', output)


if __name__ == '__main__':
    unittest.main()
