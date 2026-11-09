# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for prepare_commit_message.py."""

import contextlib
import io
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import prepare_commit_message as pcm

_DESCRIPTION = 'Remove the unused `Foo` helper and its "legacy" callers.'
_VALID = f'[cleanup] Remove Foo\n\n{_DESCRIPTION}\n\nBug: 123, 456\n'
# Longer than the max line length, so it can't be wrapped.
_LONG_LINK = 'https://example.com/' + 'a' * 80
_LSC_SKILL = 'java-inline-fqn-cleanup'
_LSC_SKILLS = (
    'fix-outdated-javadocs',
    'histogram-cleanup',
    'java-inline-fqn-cleanup',
    'lint-sync',
    'remove-unused-object-overrides',
    'replace-anonymous-runnables',
)
_NON_LSC_SKILLS = ('accessibility-cleanup', 'histogram-extension')


class PrepareCommitMessageTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = tmp.name

    def _write(self, name, content):
        path = os.path.join(self.tmp, name)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
        return path

    def _run(self, argv, stdin=None):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            try:
                if stdin is None:
                    code = pcm.main(argv)
                else:
                    with mock.patch.object(sys, 'stdin', io.StringIO(stdin)):
                        code = pcm.main(argv)
            except SystemExit as e:
                # argparse exits with 2 on bad arguments.
                code = e.code
        return code, out.getvalue(), err.getvalue()

    def _make(self, skill, description='Body.', title='Remove Foo'):
        """Returns a valid message for |skill| via the library API."""
        cfg = pcm.SKILL_CONFIGS[skill]
        return pcm.generate_commit_message(
            title,
            description,
            cfg.hashtags,
            cfg.parent_bug,
            forbid_lsc_tag='LSC' not in cfg.hashtags,
        )

    def _validate(self, skill, message, extra_args=()):
        path = self._write('commit.txt', message)
        return self._run(['--skill', skill, '--validate', path, *extra_args])

    def test_dedupe_preserve_order(self):
        self.assertEqual(
            pcm.dedupe_preserve_order(['b', 'A', 'B', 'a', 'c']),
            ['b', 'A', 'c'],
        )

    def test_format_bug_footer(self):
        self.assertEqual(
            pcm.format_bug_footer('1', ['2', 'NONE', '1', '3', '2']),
            'Bug: 1, 2, 3',
        )
        self.assertEqual(pcm.format_bug_footer('None', ['7']), 'Bug: 7')
        with self.assertRaises(ValueError):
            pcm.format_bug_footer('none', ['', 'None'])

    def test_skill_configs(self):
        self.assertEqual(list(pcm.SKILL_CONFIGS), sorted(pcm.SKILL_CONFIGS))
        for skill, cfg in pcm.SKILL_CONFIGS.items():
            with self.subTest(skill=skill):
                self.assertEqual(cfg._fields, ('hashtags', 'parent_bug'))
                self.assertTrue(cfg.parent_bug.isdigit())
        lsc_skills = tuple(
            s for s, c in pcm.SKILL_CONFIGS.items() if 'LSC' in c.hashtags
        )
        self.assertEqual(lsc_skills, _LSC_SKILLS)
        self.assertEqual(
            pcm.SKILL_CONFIGS['histogram-cleanup'].hashtags,
            ('histogram-cleanup', 'LSC'),
        )
        self.assertEqual(
            pcm.SKILL_CONFIGS['accessibility-cleanup'].hashtags,
            ('Accessibility Cleanup',),
        )

    def test_lsc_skills_keep_lsc_tag_and_parent_bug(self):
        for skill in _LSC_SKILLS:
            with self.subTest(skill=skill):
                cfg = pcm.SKILL_CONFIGS[skill]
                message = self._make(skill)
                subject = message.split('\n')[0]
                self.assertEqual(
                    subject,
                    ''.join(f'[{t}]' for t in cfg.hashtags) + ' Remove Foo',
                )
                self.assertIn('[LSC]', subject)
                self.assertTrue(
                    message.endswith(f'\n\nBug: {cfg.parent_bug}\n')
                )
                self.assertNotIn('LSC Doc:', message)
                self.assertEqual(
                    pcm.validate_commit_message(
                        message, [cfg.parent_bug], cfg.hashtags
                    ),
                    [],
                )

    def test_generate_commit_message(self):
        message = pcm.generate_commit_message(
            ' Remove Foo ',
            'First paragraph.\n\nSecond paragraph.',
            hashtag='cleanup',
            parent_bug='1',
            extra_bugs=['2'],
            extra_footers=['Change-Id: I123'],
        )
        self.assertEqual(
            message,
            '[cleanup] Remove Foo\n\nFirst paragraph.\n\n'
            'Second paragraph.\n\nChange-Id: I123\n'
            'Bug: 1, 2\n',
        )

    def test_generate_keeps_obsolete_histogram_paragraph(self):
        tags = 'OBSOLETE_HISTOGRAM[A]=expired\nOBSOLETE_HISTOGRAM[B]=expired'
        message = pcm.generate_commit_message(
            'Remove Foo', f'Body.\n\n{tags}', parent_bug='1'
        )
        self.assertEqual(
            message,
            f'Remove Foo\n\nBody.\n\n{tags}\n\nBug: 1\n',
        )

    def test_generate_keeps_long_plural_obsolete_histograms_unwrapped(self):
        tag = 'OBSOLETE_HISTOGRAMS=' + 'Expired and no longer recorded. ' * 3
        tag = tag.strip()
        self.assertGreater(len(tag), pcm.MAX_LINE_LENGTH)
        message = pcm.generate_commit_message(
            'Remove Foo', f'Body.\n\n{tag}', parent_bug='1'
        )
        self.assertEqual(message, f'Remove Foo\n\nBody.\n\n{tag}\n\nBug: 1\n')
        self.assertEqual(pcm.validate_commit_message(message, ['1']), [])

    def test_obsolete_histogram_regex_rejects_malformed_plural(self):
        regex = pcm._OBSOLETE_HISTOGRAM_RE
        self.assertIsNone(regex.match('OBSOLETE_HISTOGRAMS[Foo]=bar'))
        self.assertIsNone(regex.match('OBSOLETE_HISTOGRAMS='))

    def test_generate_wraps_paragraphs(self):
        message = pcm.generate_commit_message(
            'Remove Foo', 'word ' * 40, parent_bug='1'
        )
        self.assertEqual(pcm.validate_commit_message(message, ['1']), [])
        self.assertGreater(len(message.split('\n\n')[1].split('\n')), 1)

    def test_generate_multiple_hashtags(self):
        message = pcm.generate_commit_message(
            'Remove Foo',
            'Body.',
            hashtag=['Code Health', ' LSC ', ''],
            parent_bug='1',
        )
        self.assertTrue(message.startswith('[Code Health][LSC] Remove Foo\n'))

    def test_generate_strips_leading_tags_from_title_when_tagged(self):
        message = pcm.generate_commit_message(
            '[Code Health][LSC] [Code Health] Remove Foo',
            'Body.',
            hashtag=['Code Health', 'LSC'],
            parent_bug='1',
        )
        self.assertTrue(message.startswith('[Code Health][LSC] Remove Foo\n'))

    def test_generate_keeps_title_brackets_without_hashtags(self):
        message = pcm.generate_commit_message(
            '[x] Remove Foo', 'Body.', parent_bug='1'
        )
        self.assertTrue(message.startswith('[x] Remove Foo\n'))

    def test_generate_with_only_tags_in_title_is_rejected(self):
        with self.assertRaises(ValueError):
            pcm.generate_commit_message(
                '[LSC]', 'Body.', hashtag=['LSC'], parent_bug='1'
            )

    def test_generate_keeps_long_url_on_one_line(self):
        link = f'Link: {_LONG_LINK}'
        message = pcm.generate_commit_message(
            'Remove Foo', f'Intro.\n\n{link}', parent_bug='1'
        )
        self.assertIn(f'\n\n{link}\n\n', message)
        self.assertEqual(pcm.validate_commit_message(message, ['1']), [])

    def test_generate_rejects_lsc_doc(self):
        descriptions = {
            'doc': 'Body.\n\nLSC Doc: https://example.com/doc',
            'lower_doc': 'Body.\n\nlsc doc: x',
            'upper_doc': 'Body.\n\nLSC DOC: x',
            'embedded_doc': 'Body. LSC Doc: x',
            'only_doc': 'LSC Doc: https://example.com/doc',
            'wrapped_doc': 'Body. LSC\nDoc: x',
            # Auto-wrapping puts `LSC` and `Doc:` on separate lines.
            'long_prose_wrapped_doc': 'word ' * 13 + 'LSC Doc: x',
        }
        hashtags = (['Code Health', 'LSC'], ['Code Health'], None)
        for name, description in descriptions.items():
            for hashtag in hashtags:
                for forbid_lsc_tag in (False, True):
                    with self.subTest(
                        name=name, hashtag=hashtag, forbid=forbid_lsc_tag
                    ):
                        with self.assertRaisesRegex(ValueError, 'LSC Doc'):
                            pcm.generate_commit_message(
                                'Remove Foo',
                                description,
                                hashtag=hashtag,
                                parent_bug='1',
                                forbid_lsc_tag=forbid_lsc_tag,
                            )

    def test_generate_forbid_lsc_tag(self):
        for title in ('[LSC] Remove Foo', '[lsc] Remove Foo', 'Fix [Lsc] Foo'):
            with self.subTest(title=title):
                with self.assertRaisesRegex(ValueError, 'not allowed'):
                    pcm.generate_commit_message(
                        title,
                        'Body.',
                        hashtag='Code Health',
                        parent_bug='1',
                        forbid_lsc_tag=True,
                    )
        # [LSC] is fine in the title when it is not forbidden.
        message = pcm.generate_commit_message(
            '[LSC] Remove Foo', 'Body.', hashtag='LSC', parent_bug='1'
        )
        self.assertTrue(message.startswith('[LSC] Remove Foo\n'))

    def test_generate_forbid_lsc_tag_allows_lsc_in_body(self):
        message = pcm.generate_commit_message(
            'Remove Foo',
            'Part of the [LSC] rollout.',
            hashtag='Code Health',
            parent_bug='1',
            forbid_lsc_tag=True,
        )
        self.assertIn('\n\nPart of the [LSC] rollout.\n\n', message)

    def test_generate_rejects_bad_input(self):
        subjects = ('', 'two\nlines', 'Ends with period.', 'a' * 73)
        for subject in subjects:
            with self.subTest(subject=subject):
                with self.assertRaises(ValueError):
                    pcm.generate_commit_message(
                        subject, 'Body.', parent_bug='1'
                    )
        with self.assertRaises(ValueError):
            pcm.generate_commit_message('Remove Foo', ' \n ', parent_bug='1')
        with self.assertRaises(ValueError):
            pcm.generate_commit_message('Remove Foo', 'Body.')

    def test_generate_accepts_max_length_subject(self):
        subject = 'a' * pcm.MAX_LINE_LENGTH
        message = pcm.generate_commit_message(subject, 'Body.', parent_bug='1')
        self.assertTrue(message.startswith(subject + '\n\n'))

    def test_validate_valid_message(self):
        self.assertEqual(pcm.validate_commit_message(_VALID), [])
        self.assertEqual(
            pcm.validate_commit_message(_VALID, ['123', 'none']), []
        )
        self.assertEqual(
            pcm.validate_commit_message(' \n'), ['Commit message is empty.']
        )

    def test_validate_subject_problems(self):
        errors = pcm.validate_commit_message(
            'a' * 73 + '.\nnot blank\n\nBug: 1'
        )
        self.assertTrue(any('Subject is 74 chars' in e for e in errors))
        self.assertTrue(any('end with a period' in e for e in errors))
        self.assertTrue(any('blank line' in e for e in errors))

    def test_validate_body_line_length(self):
        errors = pcm.validate_commit_message(
            f'Subject\n\n{"x" * 80}\n\nBug: 1\n'
        )
        self.assertEqual(len(errors), 1)
        self.assertIn('Line 3 is 80 chars', errors[0])
        trailer = 'Cq-Include-Trybots: ' + 'x' * 80
        message = f'Subject\n\nBody.\n\n{trailer}\nBug: 1\n'
        self.assertEqual(pcm.validate_commit_message(message), [])
        tag = 'OBSOLETE_HISTOGRAM[' + 'N' * 80 + ']=expired'
        message = f'Subject\n\nBody.\n\n{tag}\n\nBug: 1\n'
        self.assertEqual(pcm.validate_commit_message(message), [])

    def test_validate_allows_long_url_line(self):
        message = f'Subject\n\nLink: {_LONG_LINK}\n\nBug: 1\n'
        self.assertEqual(pcm.validate_commit_message(message), [])

    def test_validate_rejects_long_prose_with_url(self):
        line = 'word ' * 20 + 'https://x.com'
        errors = pcm.validate_commit_message(f'Subject\n\n{line}\n\nBug: 1\n')
        self.assertEqual(len(errors), 1)
        self.assertIn('Line 3 is', errors[0])

    def test_validate_banned_tags_and_disclaimer(self):
        errors = pcm.validate_commit_message(
            'Subject\n\nBody.\nThis change was generated using a tool.\n\n'
            'Test: foo\nTAG=bar\nBug: 1\n'
        )
        self.assertTrue(any("banned tag 'Test:'" in e for e in errors))
        self.assertTrue(any("banned tag 'TAG='" in e for e in errors))
        self.assertTrue(any('generator disclaimer' in e for e in errors))

    def test_validate_bug_lines(self):
        self.assertEqual(
            pcm.validate_commit_message('Subject\n\nBody.\n'),
            ["Footer is missing a 'Bug:' line."],
        )
        self.assertEqual(
            pcm.validate_commit_message(_VALID, ['123', '999']),
            ["Footer is missing 'Bug: 999'."],
        )
        errors = pcm.validate_commit_message(
            _VALID.replace('123', '1234'), ['123']
        )
        self.assertEqual(errors, ["Footer is missing 'Bug: 123'."])

    def test_validate_required_hashtags(self):
        tags = ['Code Health', 'LSC']
        good = '[Code Health][LSC] Remove Foo\n\nBody.\n\nBug: 1\n'
        self.assertEqual(
            pcm.validate_commit_message(good, required_hashtags=tags), []
        )
        bad_subjects = (
            'Remove Foo',
            '[Code Health] Remove Foo',
            '[Code Health][Lsc] Remove Foo',
            '[code health][LSC] Remove Foo',
            '[LSC][Code Health] Remove Foo',
            '[Code Health][LSC]Remove Foo',
        )
        for subject in bad_subjects:
            with self.subTest(subject=subject):
                message = good.replace('[Code Health][LSC] Remove Foo', subject)
                errors = pcm.validate_commit_message(
                    message, required_hashtags=tags
                )
                self.assertEqual(len(errors), 1)
                self.assertIn('Subject must start with', errors[0])

    def test_validate_rejects_duplicate_leading_tags(self):
        tags = ['Code Health', 'LSC']
        for rest in ('[Code Health][LSC] Foo', '[Other] Foo', '[x][y]Foo'):
            with self.subTest(rest=rest):
                message = f'[Code Health][LSC] {rest}\n\nBody.\n\nBug: 1\n'
                errors = pcm.validate_commit_message(
                    message, required_hashtags=tags
                )
                self.assertEqual(len(errors), 1)
                self.assertIn('extra or duplicate', errors[0])

    def test_validate_rejects_lsc_doc(self):
        base = '[LSC] Remove Foo\n\nBody.\n\nBug: 1\n'
        bad_messages = {
            'doc': base.replace('\n\nBug', '\n\nLSC Doc: https://a.co\n\nBug'),
            'lower_doc': base.replace('\n\nBug', '\n\nlsc doc: x\n\nBug'),
            'upper_doc': base.replace('\n\nBug', '\n\nLSC DOC: x\n\nBug'),
            'in_footer': base.replace('\nBug', '\nLSC Doc: x\nBug'),
            'embedded': base.replace('Body.', 'Body. See LSC Doc: x'),
            'wrapped': base.replace('Body.', 'Body. See LSC\nDoc: x'),
        }
        self.assertEqual(pcm.validate_commit_message(base), [])
        for name, message in bad_messages.items():
            for forbid_lsc_tag in (False, True):
                with self.subTest(name=name, forbid_lsc_tag=forbid_lsc_tag):
                    self.assertNotEqual(message, base)
                    errors = pcm.validate_commit_message(
                        message, forbid_lsc_tag=forbid_lsc_tag
                    )
                    self.assertIn("'LSC Doc:' is not allowed.", errors)

    def test_validate_forbid_lsc_tag(self):
        good = '[Code Health] Remove Foo\n\nBody.\n\nBug: 1\n'
        self.assertEqual(
            pcm.validate_commit_message(good, forbid_lsc_tag=True), []
        )
        bad_messages = (
            good.replace('[Code Health]', '[Code Health][LSC]'),
            good.replace('[Code Health]', '[Code Health][Lsc]'),
            good.replace('[Code Health]', '[lsc]'),
        )
        for message in bad_messages:
            with self.subTest(message=message):
                self.assertEqual(
                    pcm.validate_commit_message(message, forbid_lsc_tag=True),
                    ['Subject must not contain [LSC] for this skill.'],
                )
                # [LSC] in the subject is fine when it is not forbidden.
                self.assertEqual(pcm.validate_commit_message(message), [])

    def test_validate_forbid_lsc_tag_only_checks_subject(self):
        message = (
            '[Code Health] Remove Foo\n\nPart of the [LSC] rollout.\n\nBug: 1\n'
        )
        self.assertEqual(
            pcm.validate_commit_message(message, forbid_lsc_tag=True), []
        )

    def test_cli_generate_to_stdout(self):
        code, out, err = self._run(
            [
                '--skill',
                'accessibility-cleanup',
                '--title',
                'Remove Foo',
                '--bug',
                'none',
                '--bug',
                '2',
                '--bug',
                '2',
                '--footer',
                'Change-Id: I123',
                '--description',
                'Body text.',
            ]
        )
        self.assertEqual((code, err), (0, ''))
        self.assertEqual(
            out,
            '[Accessibility Cleanup] Remove Foo\n\nBody text.\n\n'
            'Change-Id: I123\nBug: 531299755, 2\n',
        )

    def test_cli_generate_multiple_hashtags(self):
        code, out, err = self._run(
            [
                '--skill',
                'replace-anonymous-runnables',
                '--title',
                'Remove Foo',
                '--description',
                'Body text.',
            ]
        )
        self.assertEqual((code, err), (0, ''))
        self.assertEqual(
            out,
            '[Code Health][LSC] Remove Foo\n\nBody text.\n\nBug: 541671762\n',
        )

    def test_cli_generate_description_dash_reads_stdin(self):
        out_commit = os.path.join(self.tmp, 'commit.txt')
        code, _, _ = self._run(
            [
                '--skill',
                'histogram-extension',
                '--title',
                'Remove Foo',
                '--description',
                '-',
                '--out-commit',
                out_commit,
            ],
            stdin=_DESCRIPTION,
        )
        self.assertEqual(code, 0)
        with open(out_commit, encoding='utf-8') as f:
            message = f.read()
        self.assertIn('`Foo`', message)
        self.assertIn('"legacy"', message)

    def test_cli_generate_implicit_piped_stdin(self):
        code, out, _ = self._run(
            ['--skill', 'lint-sync', '--title', 'Remove Foo'],
            stdin=_DESCRIPTION,
        )
        self.assertEqual(code, 0)
        self.assertIn(_DESCRIPTION, out)

    def test_cli_generate_rejects_long_subject_without_truncating(self):
        out_commit = os.path.join(self.tmp, 'commit.txt')
        code, _, err = self._run(
            [
                '--skill',
                'histogram-extension',
                '--title',
                'a' * 80,
                '--description',
                'Body.',
                '--out-commit',
                out_commit,
            ]
        )
        self.assertEqual(code, 2)
        self.assertIn('Subject is', err)
        self.assertFalse(os.path.exists(out_commit))
        code, _, err = self._run(
            [
                '--skill',
                'lint-sync',
                '--title',
                'a' * 70,
                '--description',
                'Body.',
            ]
        )
        self.assertEqual(code, 2)
        self.assertIn('Subject is', err)

    def test_cli_generate_invalid_args_exit_2(self):
        cases = {
            'title': ['--title', ' ', '--description', 'Body.'],
            'no_title': ['--description', 'Body.'],
            'description': ['--title', 'Remove Foo', '--description', ' '],
        }
        for name, argv in cases.items():
            with self.subTest(name=name):
                code, _, err = self._run(['--skill', 'lint-sync', *argv])
                self.assertEqual(code, 2)
                self.assertNotEqual(err, '')

    def test_cli_generate_tty_stdin_without_description_exit_2(self):
        fake_stdin = mock.Mock()
        fake_stdin.isatty.return_value = True
        with mock.patch.object(sys, 'stdin', fake_stdin):
            code, _, err = self._run(
                ['--skill', 'lint-sync', '--title', 'Remove Foo']
            )
        self.assertEqual(code, 2)
        self.assertIn('Description must not be empty', err)

    def test_cli_every_skill_generates_valid_message(self):
        for skill, cfg in pcm.SKILL_CONFIGS.items():
            with self.subTest(skill=skill):
                path = os.path.join(self.tmp, f'{skill}.txt')
                code, _, err = self._run(
                    [
                        '--skill',
                        skill,
                        '--title',
                        'Remove Foo',
                        '--description',
                        'Body.',
                        '--out-commit',
                        path,
                    ]
                )
                self.assertEqual((code, err), (0, ''))
                with open(path, encoding='utf-8') as f:
                    message = f.read()
                prefix = ''.join(f'[{t}]' for t in cfg.hashtags)
                self.assertTrue(message.startswith(f'{prefix} Remove Foo\n\n'))
                self.assertIn(f'\n\nBug: {cfg.parent_bug}\n', message)
                self.assertNotIn('LSC Doc:', message)
                self.assertEqual(
                    '[LSC]' in message.split('\n')[0], 'LSC' in cfg.hashtags
                )
                code, out, err = self._run(
                    ['--validate', path, '--skill', skill]
                )
                self.assertEqual((code, err), (0, ''))
                self.assertIn('valid', out)

    def test_cli_skill_is_required_and_must_be_known(self):
        path = self._write('commit.txt', self._make(_LSC_SKILL))
        cases = {
            'generate_missing': ['--title', 'Remove Foo', '--description', 'x'],
            'validate_missing': ['--validate', path],
            'invalid': ['--skill', 'nope', '--validate', path],
            'empty': ['--skill', '', '--validate', path],
        }
        for name, argv in cases.items():
            with self.subTest(name=name):
                code, out, err = self._run(argv)
                self.assertEqual(code, 2)
                self.assertEqual(out, '')
                self.assertNotEqual(err, '')

    def test_cli_rejects_removed_hashtag_and_parent_bug_flags(self):
        path = self._write('commit.txt', self._make(_LSC_SKILL))
        base = ['--skill', _LSC_SKILL]
        generate = ['--title', 'Remove Foo', '--description', 'Body.']
        validate = [*base, '--validate', path]
        cases = {
            'hashtag': [*base, *generate, '--hashtag', 'LSC'],
            'parent_bug': [*base, *generate, '--parent-bug', '1'],
            'validate_parent_bug': [*validate, '--parent-bug', '1'],
            'validate_hashtag': [*validate, '--hashtag', 'LSC'],
        }
        for name, argv in cases.items():
            with self.subTest(name=name):
                code, out, err = self._run(argv)
                self.assertEqual(code, 2)
                self.assertEqual(out, '')
                self.assertIn('unrecognized arguments', err)

    def test_cli_generate_strips_duplicate_title_tags(self):
        titles = (
            '[Code Health][LSC] Remove Foo',
            '[Code Health][LSC] [Code Health][LSC] Remove Foo',
            '[LSC] [Code Health] Remove Foo',
        )
        for title in titles:
            with self.subTest(title=title):
                code, out, err = self._run(
                    [
                        '--skill',
                        _LSC_SKILL,
                        '--title',
                        title,
                        '--description',
                        'Body.',
                    ]
                )
                self.assertEqual((code, err), (0, ''))
                self.assertTrue(
                    out.startswith('[Code Health][LSC] Remove Foo\n')
                )
                self.assertEqual(out.count('[Code Health]'), 1)
                self.assertEqual(out.count('[LSC]'), 1)

    def test_cli_validate_rejects_double_prefixed_subject(self):
        message = self._make(_LSC_SKILL).replace(
            '[Code Health][LSC] ', '[Code Health][LSC] [Code Health][LSC] ', 1
        )
        code, _, err = self._validate(_LSC_SKILL, message)
        self.assertEqual(code, 1)
        self.assertIn('extra or duplicate', err)

    def test_cli_histogram_cleanup_keeps_obsolete_paragraph_last(self):
        tag = 'OBSOLETE_HISTOGRAMS=Expired and no longer recorded.'
        code, out, err = self._run(
            [
                '--skill',
                'histogram-cleanup',
                '--title',
                'Remove expired Foo histograms',
                '--bug',
                '123',
                '--description',
                f'Summary.\n\n{tag}',
            ]
        )
        self.assertEqual((code, err), (0, ''))
        self.assertEqual(
            out,
            '[histogram-cleanup][LSC] Remove expired Foo histograms'
            f'\n\nSummary.\n\n{tag}\n\n'
            'Bug: 499059525, 123\n',
        )

    def test_cli_histogram_cleanup_keeps_multiple_obsolete_paragraphs(self):
        tags = 'OBSOLETE_HISTOGRAM[A]=gone\nOBSOLETE_HISTOGRAM[B]=gone'
        code, out, err = self._run(
            [
                '--skill',
                'histogram-cleanup',
                '--title',
                'Remove Foo',
                '--description',
                f'Summary.\n\n{tags}',
            ]
        )
        self.assertEqual((code, err), (0, ''))
        self.assertIn(f'\n\n{tags}\n\nBug: 499059525\n', out)

    def test_cli_validate(self):
        message = self._make(_LSC_SKILL)
        path = self._write('commit.txt', message)
        code, out, _ = self._run(
            ['--skill', _LSC_SKILL, '--validate', path, '--bug', '528570333']
        )
        self.assertEqual(code, 0)
        self.assertIn('valid', out)
        code, _, err = self._run(
            ['--skill', _LSC_SKILL, '--validate', path, '--bug', '999']
        )
        self.assertEqual(code, 1)
        self.assertIn('Bug: 999', err)

    def test_cli_validate_missing_file(self):
        code, _, err = self._run(
            [
                '--skill',
                'lint-sync',
                '--validate',
                os.path.join(self.tmp, 'missing.txt'),
            ]
        )
        self.assertEqual(code, 2)
        self.assertNotEqual(err, '')

    def test_cli_validate_reports_wrong_skill(self):
        # A message for one skill must not validate for another.
        message = self._make('fix-outdated-javadocs')
        code, _, _ = self._validate('histogram-cleanup', message)
        self.assertEqual(code, 1)
        code, _, _ = self._validate('accessibility-cleanup', message)
        self.assertEqual(code, 1)

    def test_cli_non_lsc_skills_forbid_lsc_tag_on_generate(self):
        titles = {
            'title': '[LSC] Remove Foo',
            'lower_title': '[lsc] Remove Foo',
        }
        for skill in _NON_LSC_SKILLS:
            for name, title in titles.items():
                with self.subTest(skill=skill, name=name):
                    code, out, err = self._run(
                        [
                            '--skill',
                            skill,
                            '--title',
                            title,
                            '--description',
                            'Body.',
                        ]
                    )
                    self.assertEqual(code, 2)
                    self.assertEqual(out, '')
                    self.assertIn('[LSC] is not allowed', err)

    def test_cli_non_lsc_skills_forbid_lsc_tag_on_validate(self):
        for skill in _NON_LSC_SKILLS:
            good = self._make(skill)
            self.assertEqual(self._validate(skill, good)[0], 0)
            bad_messages = {
                'lsc_tag': good.replace('] Remove Foo', '][LSC] Remove Foo'),
                'lsc_tag_lowercase': good.replace(
                    '] Remove Foo', '][lsc] Remove Foo'
                ),
            }
            for name, message in bad_messages.items():
                with self.subTest(skill=skill, name=name):
                    self.assertNotEqual(message, good)
                    code, _, err = self._validate(skill, message)
                    self.assertEqual(code, 1)
                    self.assertIn('Subject must not contain [LSC]', err)

    def test_cli_non_lsc_skills_allow_lsc_in_body(self):
        description = 'Part of the [LSC] rollout.'
        for skill in _NON_LSC_SKILLS:
            with self.subTest(skill=skill):
                path = os.path.join(self.tmp, f'{skill}.txt')
                code, _, err = self._run(
                    [
                        '--skill',
                        skill,
                        '--title',
                        'Remove Foo',
                        '--description',
                        description,
                        '--out-commit',
                        path,
                    ]
                )
                self.assertEqual((code, err), (0, ''))
                with open(path, encoding='utf-8') as f:
                    message = f.read()
                self.assertIn(description, message)
                self.assertNotIn('[LSC]', message.split('\n')[0])
                self.assertEqual(self._validate(skill, message)[0], 0)

    def test_cli_every_skill_rejects_lsc_doc_on_generate(self):
        descriptions = {
            'doc': 'Body.\n\nLSC Doc: https://example.com/doc',
            'lower_doc': 'Body.\n\nlsc doc: x',
            'upper_doc': 'LSC DOC: x\n\nBody.',
            'embedded_doc': 'Body. LSC Doc: x',
            'only_doc': 'LSC Doc: https://example.com/doc',
            'wrapped_doc': 'Body. LSC\nDoc: x',
            # Auto-wrapping puts `LSC` and `Doc:` on separate lines.
            'long_prose_wrapped_doc': 'word ' * 13 + 'LSC Doc: x',
        }
        for skill in sorted(pcm.SKILL_CONFIGS):
            for name, description in descriptions.items():
                with self.subTest(skill=skill, name=name):
                    out_commit = os.path.join(self.tmp, f'{skill}-{name}.txt')
                    code, out, err = self._run(
                        [
                            '--skill',
                            skill,
                            '--title',
                            'Remove Foo',
                            '--description',
                            description,
                            '--out-commit',
                            out_commit,
                        ]
                    )
                    self.assertEqual(code, 2)
                    self.assertEqual(out, '')
                    self.assertIn("'LSC Doc:' is not allowed", err)
                    self.assertFalse(os.path.exists(out_commit))

    def test_cli_every_skill_rejects_lsc_doc_on_validate(self):
        for skill, cfg in pcm.SKILL_CONFIGS.items():
            good = self._make(skill)
            self.assertEqual(self._validate(skill, good)[0], 0)
            footer = f'\n\nBug: {cfg.parent_bug}'
            bad_messages = {
                'doc': good.replace(
                    footer, f'\n\nLSC Doc: https://example.com/doc{footer}'
                ),
                'lower_doc': good.replace(footer, f'\n\nlsc doc: x{footer}'),
                'upper_doc': good.replace(footer, f'\n\nLSC DOC: x{footer}'),
                'embedded_doc': good.replace('Body.', 'See lsc doc: x'),
                'wrapped_doc': good.replace('Body.', 'See LSC\nDoc: x'),
            }
            for name, message in bad_messages.items():
                with self.subTest(skill=skill, name=name):
                    self.assertNotEqual(message, good)
                    code, out, err = self._validate(skill, message)
                    self.assertEqual(code, 1)
                    self.assertEqual(out, '')
                    self.assertIn("'LSC Doc:' is not allowed", err)

    def test_cli_non_lsc_skills_validate_extra_bug(self):
        good = self._run(
            [
                '--skill',
                'histogram-extension',
                '--title',
                'Extend Foo',
                '--bug',
                '77',
                '--description',
                'Body.',
            ]
        )[1]
        self.assertIn('Bug: 499059543, 77\n', good)
        skill = 'histogram-extension'
        code, _, _ = self._validate(skill, good, ['--bug', '77'])
        self.assertEqual(code, 0)
        code, _, _ = self._validate(skill, good, ['--bug', '78'])
        self.assertEqual(code, 1)


if __name__ == '__main__':
    unittest.main()
