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
_LONG_URL = (
    'https://docs.google.com/document/d/'
    '1J1B9BV8eEedE6j6qhUUDbC43IalN2cfo3VHX3mGHbGc/edit?tab=t.0'
)
_LSC_DOC = f'LSC Doc: {_LONG_URL}'
_LSC_SKILL = 'java-inline-fqn-cleanup'
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
            lsc_doc=cfg.lsc_doc,
            forbid_lsc=cfg.lsc_doc is None,
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
        self.assertEqual(pcm.LSC_DOC_URL, _LONG_URL)
        for skill, cfg in pcm.SKILL_CONFIGS.items():
            with self.subTest(skill=skill):
                self.assertTrue(cfg.parent_bug.isdigit())
                self.assertEqual('LSC' in cfg.hashtags, cfg.lsc_doc is not None)
        self.assertEqual(
            pcm.SKILL_CONFIGS['histogram-cleanup'].hashtags,
            ('histogram-cleanup', 'LSC'),
        )
        self.assertEqual(
            pcm.SKILL_CONFIGS['accessibility-cleanup'].hashtags,
            ('Accessibility Cleanup',),
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
        doc = f'LSC Doc: {_LONG_URL}'
        message = pcm.generate_commit_message(
            'Remove Foo', f'Intro.\n\n{doc}', parent_bug='1'
        )
        self.assertIn(f'\n\n{doc}\n\n', message)
        self.assertEqual(pcm.validate_commit_message(message, ['1']), [])

    def test_generate_lsc_doc_appended_at_end_of_body(self):
        message = pcm.generate_commit_message(
            'Remove Foo',
            'One.\n\nTwo.',
            parent_bug='1',
            lsc_doc=_LONG_URL,
        )
        self.assertEqual(
            message, f'Remove Foo\n\nOne.\n\nTwo.\n\n{_LSC_DOC}\n\nBug: 1\n'
        )

    def test_generate_lsc_doc_replaces_standalone_doc_paragraph(self):
        docs = ('LSC Doc: https://example.com/old', 'lsc doc: old', _LSC_DOC)
        for doc in docs:
            with self.subTest(doc=doc):
                message = pcm.generate_commit_message(
                    'Remove Foo',
                    f'One.\n\n{doc}\n\nTwo.',
                    parent_bug='1',
                    lsc_doc=_LONG_URL,
                )
                self.assertEqual(
                    message,
                    f'Remove Foo\n\nOne.\n\nTwo.\n\n{_LSC_DOC}\n\nBug: 1\n',
                )

    def test_generate_lsc_doc_rejects_embedded_doc(self):
        descriptions = (
            'See LSC Doc: foo for details.',
            'Body.\nLSC Doc: foo',
            'Body.\n\nLSC Doc: foo\nmore',
        )
        for description in descriptions:
            with self.subTest(description=description):
                with self.assertRaisesRegex(ValueError, 'own paragraph'):
                    pcm.generate_commit_message(
                        'Remove Foo',
                        description,
                        parent_bug='1',
                        lsc_doc=_LONG_URL,
                    )

    def test_generate_lsc_doc_rejects_empty_body(self):
        for description in (_LSC_DOC, f'{_LSC_DOC}\n\nLSC Doc: other', ''):
            with self.subTest(description=description):
                with self.assertRaisesRegex(ValueError, 'must not be empty'):
                    pcm.generate_commit_message(
                        'Remove Foo',
                        description,
                        parent_bug='1',
                        lsc_doc=_LONG_URL,
                    )

    def test_generate_lsc_doc_goes_before_obsolete_histograms(self):
        tags = 'OBSOLETE_HISTOGRAM[A]=gone\nOBSOLETE_HISTOGRAM[B]=gone'
        message = pcm.generate_commit_message(
            'Remove Foo',
            f'Body.\n\n{tags}',
            parent_bug='1',
            lsc_doc=_LONG_URL,
        )
        self.assertEqual(
            message,
            f'Remove Foo\n\nBody.\n\n{_LSC_DOC}\n\n{tags}\n\nBug: 1\n',
        )

    def test_generate_forbid_lsc(self):
        cases = {
            'doc': ('Remove Foo', f'Body.\n\n{_LSC_DOC}'),
            'lower_doc': ('Remove Foo', 'Body.\n\nlsc doc: x'),
            'embedded_doc': ('Remove Foo', 'Body. LSC Doc: x'),
            'title': ('[lsc] Remove Foo', 'Body.'),
        }
        for name, (title, description) in cases.items():
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    pcm.generate_commit_message(
                        title,
                        description,
                        hashtag='Code Health',
                        parent_bug='1',
                        forbid_lsc=True,
                    )

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
        message = f'Subject\n\nLSC Doc: {_LONG_URL}\n\nBug: 1\n'
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

    def test_validate_required_lsc_doc(self):
        subject = '[LSC] Remove Foo'
        good = f'{subject}\n\nBody.\n\n{_LSC_DOC}\n\nBug: 1\n'
        self.assertEqual(
            pcm.validate_commit_message(good, required_lsc_doc=_LONG_URL), []
        )
        bad_messages = {
            'missing': f'{subject}\n\nBody.\n\nBug: 1\n',
            'wrong_url': good.replace(_LONG_URL, 'https://example.com'),
            'duplicate': good.replace(
                f'\n\n{_LSC_DOC}\n\n', f'\n\n{_LSC_DOC}\n\n{_LSC_DOC}\n\n'
            ),
            'in_footer': f'{subject}\n\nBody.\n\n{_LSC_DOC}\nBug: 1\n',
            'embedded': f'{subject}\n\nBody. {_LSC_DOC}\n\nBug: 1\n',
        }
        for name, message in bad_messages.items():
            with self.subTest(name=name):
                errors = pcm.validate_commit_message(
                    message, required_lsc_doc=_LONG_URL
                )
                self.assertTrue(errors)

    def test_validate_forbid_lsc(self):
        good = '[Code Health] Remove Foo\n\nBody.\n\nBug: 1\n'
        self.assertEqual(pcm.validate_commit_message(good, forbid_lsc=True), [])
        bad_messages = (
            good.replace('[Code Health]', '[Code Health][LSC]'),
            good.replace('[Code Health]', '[Code Health][Lsc]'),
            good.replace('\n\nBug', f'\n\n{_LSC_DOC}\n\nBug'),
            good.replace('Body.', 'See lsc doc: x'),
        )
        for message in bad_messages:
            with self.subTest(message=message):
                self.assertTrue(
                    pcm.validate_commit_message(message, forbid_lsc=True)
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
            f'[Code Health][LSC] Remove Foo\n\nBody text.\n\n{_LSC_DOC}\n\n'
            'Bug: 541671762\n',
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
                self.assertEqual(
                    message.count('LSC Doc:'), 1 if cfg.lsc_doc else 0
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

    def test_cli_generate_replaces_wrong_lsc_doc(self):
        code, out, err = self._run(
            [
                '--skill',
                _LSC_SKILL,
                '--title',
                'Remove Foo',
                '--description',
                'Body.\n\nLSC Doc: https://example.com/wrong',
            ]
        )
        self.assertEqual((code, err), (0, ''))
        self.assertNotIn('example.com', out)
        self.assertEqual(out.count('LSC Doc:'), 1)
        self.assertIn(f'\n\n{_LSC_DOC}\n\nBug: 528570333\n', out)

    def test_cli_generate_deduplicates_canonical_lsc_doc(self):
        code, out, err = self._run(
            [
                '--skill',
                _LSC_SKILL,
                '--title',
                'Remove Foo',
                '--description',
                f'Body.\n\n{_LSC_DOC}\n\n{_LSC_DOC}',
            ]
        )
        self.assertEqual((code, err), (0, ''))
        self.assertEqual(out.count('LSC Doc:'), 1)
        self.assertEqual(
            out,
            f'[Code Health][LSC] Remove Foo\n\nBody.\n\n{_LSC_DOC}\n\n'
            'Bug: 528570333\n',
        )

    def test_cli_generate_rejects_lsc_doc_only_or_embedded(self):
        cases = {
            'only_doc': (_LSC_DOC, 'must not be empty'),
            'only_wrong_doc': ('LSC Doc: https://x.com', 'must not be empty'),
            'embedded': ('Body. LSC Doc: https://x.com', 'own paragraph'),
        }
        for name, (description, expected) in cases.items():
            with self.subTest(name=name):
                out_commit = os.path.join(self.tmp, f'{name}.txt')
                code, _, err = self._run(
                    [
                        '--skill',
                        _LSC_SKILL,
                        '--title',
                        'Remove Foo',
                        '--description',
                        description,
                        '--out-commit',
                        out_commit,
                    ]
                )
                self.assertEqual(code, 2)
                self.assertIn(expected, err)
                self.assertFalse(os.path.exists(out_commit))

    def test_cli_histogram_cleanup_puts_lsc_doc_before_obsolete(self):
        tag = 'OBSOLETE_HISTOGRAMS=Expired and no longer recorded.'
        for description in (
            f'Summary.\n\n{tag}',
            f'Summary.\n\n{tag}\n\n{_LSC_DOC}',
            f'Summary.\n\n{_LSC_DOC}\n\n{tag}',
            f'Summary.\n\nLSC Doc: https://example.com/old\n\n{tag}',
        ):
            with self.subTest(description=description):
                code, out, err = self._run(
                    [
                        '--skill',
                        'histogram-cleanup',
                        '--title',
                        'Remove expired Foo histograms',
                        '--bug',
                        '123',
                        '--description',
                        description,
                    ]
                )
                self.assertEqual((code, err), (0, ''))
                self.assertEqual(
                    out,
                    '[histogram-cleanup][LSC] Remove expired Foo histograms'
                    f'\n\nSummary.\n\n{_LSC_DOC}\n\n{tag}\n\n'
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
        self.assertIn(f'\n\n{_LSC_DOC}\n\n{tags}\n\nBug: 499059525\n', out)

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

    def test_cli_validate_rejects_invalid_lsc_message(self):
        good = self._make(_LSC_SKILL)
        code, out, err = self._validate(
            _LSC_SKILL, good.replace(f'{_LSC_DOC}\n\n', '')
        )
        self.assertEqual(code, 1)
        self.assertEqual(out, '')
        self.assertIn('LSC Doc', err)

    def test_cli_validate_reports_wrong_skill(self):
        # A message for one skill must not validate for another.
        message = self._make('fix-outdated-javadocs')
        code, _, _ = self._validate('histogram-cleanup', message)
        self.assertEqual(code, 1)
        code, _, _ = self._validate('accessibility-cleanup', message)
        self.assertEqual(code, 1)

    def test_cli_non_lsc_skills_forbid_lsc_on_generate(self):
        cases = {
            'title': ('[LSC] Remove Foo', 'Body.'),
            'lower_title': ('[lsc] Remove Foo', 'Body.'),
            'doc': ('Remove Foo', f'Body.\n\n{_LSC_DOC}'),
            'lower_doc': ('Remove Foo', 'Body.\n\nlsc doc: x'),
            'embedded_doc': ('Remove Foo', 'Body. LSC Doc: x'),
        }
        for skill in _NON_LSC_SKILLS:
            for name, (title, description) in cases.items():
                with self.subTest(skill=skill, name=name):
                    code, out, err = self._run(
                        [
                            '--skill',
                            skill,
                            '--title',
                            title,
                            '--description',
                            description,
                        ]
                    )
                    self.assertEqual(code, 2)
                    self.assertEqual(out, '')
                    self.assertIn('LSC', err)

    def test_cli_non_lsc_skills_forbid_lsc_on_validate(self):
        for skill in _NON_LSC_SKILLS:
            good = self._make(skill)
            parent_bug = pcm.SKILL_CONFIGS[skill].parent_bug
            self.assertEqual(self._validate(skill, good)[0], 0)
            bad_messages = {
                'lsc_tag': good.replace('] Remove Foo', '][LSC] Remove Foo'),
                'lsc_tag_lowercase': good.replace(
                    '] Remove Foo', '][lsc] Remove Foo'
                ),
                'lsc_doc': good.replace(
                    f'\n\nBug: {parent_bug}',
                    f'\n\n{_LSC_DOC}\n\nBug: {parent_bug}',
                ),
            }
            for name, message in bad_messages.items():
                with self.subTest(skill=skill, name=name):
                    self.assertNotEqual(message, good)
                    code, _, err = self._validate(skill, message)
                    self.assertEqual(code, 1)
                    self.assertNotEqual(err, '')

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
