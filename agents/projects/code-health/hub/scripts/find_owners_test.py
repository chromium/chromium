# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for find_owners.py."""

import contextlib
import io
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import find_owners

BOT = 'chromium-autoroll@skia-public.iam.gserviceaccount.com'
ME = 'me@chromium.org'
OLD_DATE = '2020-01-01T00:00:00'
TAB_DIR = 'chrome/android/java/src/org/chromium/chrome/browser/tab'


def _emails(candidates):
    return [c['email'] for c in candidates]


def _owners(pairs):
    return [email for email, _ in pairs]


class FindOwnersTestCase(unittest.TestCase):
    """Provides a temporary git repo (on main, user.email=ME) as |self.root|."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.outer = os.path.realpath(tmp.name)
        self.root = os.path.join(self.outer, 'repo')
        os.makedirs(self.root)
        self._counter = 0
        self.git('init', '-q')
        self.git('symbolic-ref', 'HEAD', 'refs/heads/main')
        self.git('config', 'user.email', ME)
        self.git('config', 'user.name', 'Me')
        self.git('config', 'commit.gpgsign', 'false')

    def git(self, *args, env=None):
        result = subprocess.run(
            ['git', *args],
            cwd=self.root,
            env={**os.environ, **(env or {})},
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
        self.assertEqual(0, result.returncode, result.stdout)
        return result.stdout

    def write(self, rel, text=None, root=None):
        path = os.path.join(root or self.root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        if text is None:
            self._counter += 1
            text = f'content {self._counter}\n'
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)

    def commit(self, files, author=BOT, reviewers=(), date=None):
        """Commits |files| (dict of path -> text, or list of paths)."""
        for rel, text in (
            files.items()
            if isinstance(files, dict)
            else ((f, None) for f in files)
        ):
            self.write(rel, text)
            self.git('add', '--', rel)
        env = {
            'GIT_AUTHOR_NAME': 'Author',
            'GIT_AUTHOR_EMAIL': author,
            'GIT_COMMITTER_NAME': 'Author',
            'GIT_COMMITTER_EMAIL': author,
        }
        if date:
            env.update(GIT_AUTHOR_DATE=date, GIT_COMMITTER_DATE=date)
        args = ['commit', '-q', '-m', 'Change']
        if reviewers:
            args += ['-m', '\n'.join(f'Reviewed-by: {r}' for r in reviewers)]
        self.git(*args, env=env)

    def analyze(self, *files, **kwargs):
        return find_owners.analyze(self.root, list(files), **kwargs)

    def only_directory(self, *files, **kwargs):
        directories = self.analyze(*files, **kwargs)['directories']
        self.assertEqual(1, len(directories))
        return directories[0]

    def run_main(self, *args, cwd=None):
        old_cwd = os.getcwd()
        os.chdir(cwd or self.root)
        try:
            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout):
                self.assertEqual(0, find_owners.main(list(args)))
        finally:
            os.chdir(old_cwd)
        return stdout.getvalue()


class ResolveOwnersTest(FindOwnersTestCase):
    def resolve(self, rel_path):
        return find_owners.resolve_owners(self.root, rel_path)

    def test_broad_owners_paths(self):
        for owners in (
            'OWNERS',
            'chrome/OWNERS',
            'chrome/android/OWNERS',
            'content/browser/OWNERS',
            'components/embedder_support/OWNERS',
        ):
            self.assertTrue(find_owners.is_broad_owners_path(owners), owners)
        for owners in ('chrome/android/junit/OWNERS', 'a/b/c/OWNERS'):
            self.assertFalse(find_owners.is_broad_owners_path(owners), owners)

    def test_includes(self):
        self.write('a/b/shared/OWNERS', 'shared@chromium.org\n')
        for line in (
            'file://a/b/shared/OWNERS',
            'file: //a/b/shared/OWNERS',
            'include ../shared/OWNERS',
            'include /a/b/shared/OWNERS',
        ):
            self.write('a/b/c/OWNERS', line + '\n')
            specific, _, _ = self.resolve('a/b/c/Foo.java')
            self.assertEqual(
                [('shared@chromium.org', 'a/b/shared/OWNERS')], specific, line
            )

    def test_include_cycle_terminates(self):
        self.write('a/b/c/OWNERS', 'include /a/b/x/OWNERS\none@chromium.org\n')
        self.write('a/b/x/OWNERS', 'include /a/b/c/OWNERS\ntwo@chromium.org\n')
        specific, _, _ = self.resolve('a/b/c/Foo.java')
        self.assertEqual(
            ['two@chromium.org', 'one@chromium.org'], _owners(specific)
        )

    def test_sibling_includes_may_reinclude_same_file(self):
        self.write('a/b/common/OWNERS', 'common@chromium.org\n')
        for name in ('s1', 's2'):
            self.write(
                f'a/b/{name}/OWNERS',
                f'{name}@chromium.org\ninclude /a/b/common/OWNERS\n',
            )
        self.write(
            'a/b/c/OWNERS', 'file://a/b/s1/OWNERS\nfile://a/b/s2/OWNERS\n'
        )
        specific, _, _ = self.resolve('a/b/c/Foo.java')
        self.assertEqual(
            [
                's1@chromium.org',
                'common@chromium.org',
                's2@chromium.org',
                'common@chromium.org',
            ],
            _owners(specific),
        )

    def test_missing_and_outside_includes_are_ignored(self):
        self.write('OWNERS_outside', 'leak@chromium.org\n', root=self.outer)
        self.write(
            'a/b/c/OWNERS',
            'include /a/b/missing/OWNERS\n'
            'file://nope/OWNERS\ninclude ../../../../OWNERS_outside\n'
            'file://../OWNERS_outside\nreal@chromium.org\n',
        )
        specific, _, _ = self.resolve('a/b/c/Foo.java')
        self.assertEqual(['real@chromium.org'], _owners(specific))

    def test_per_file_globs_and_unmatched(self):
        self.write(
            'a/b/c/OWNERS',
            'per-file *.java,*.kt=j@chromium.org,k@chromium.org\n',
        )
        specific, _, unmatched = self.resolve('a/b/c/Foo.kt')
        self.assertEqual(
            ['j@chromium.org', 'k@chromium.org'], _owners(specific)
        )
        self.assertFalse(unmatched)
        specific, _, unmatched = self.resolve('a/b/c/Foo.txt')
        self.assertEqual([], specific)
        self.assertTrue(unmatched)

    def test_per_file_include(self):
        self.write('a/b/shared/OWNERS', 'shared@chromium.org\n')
        self.write('a/b/c/OWNERS', 'per-file *.xml=file://a/b/shared/OWNERS\n')
        self.assertEqual(
            ['shared@chromium.org'], _owners(self.resolve('a/b/c/x.xml')[0])
        )

    def test_set_noparent_stops_walk(self):
        self.write('a/b/OWNERS', 'parent@chromium.org\n')
        self.write('a/b/c/OWNERS', 'set noparent\ndirect@chromium.org\n')
        self.write('OWNERS', 'root@chromium.org\n')
        specific, fallback, _ = self.resolve('a/b/c/Foo.java')
        self.assertEqual(['direct@chromium.org'], _owners(specific))
        self.assertEqual([], fallback)

    def test_per_file_set_noparent(self):
        self.write('a/b/OWNERS', 'parent@chromium.org\n')
        self.write(
            'a/b/c/OWNERS',
            'per-file Foo.java=set noparent\n'
            'per-file Foo.java=foo@chromium.org\n',
        )
        specific, _, _ = self.resolve('a/b/c/Foo.java')
        self.assertEqual([('foo@chromium.org', 'a/b/c/OWNERS')], specific)
        specific, _, unmatched = self.resolve('a/b/c/Bar.java')
        self.assertEqual([('parent@chromium.org', 'a/b/OWNERS')], specific)
        self.assertTrue(unmatched)

    def test_broad_owners_are_only_fallback(self):
        self.write('OWNERS', 'root@chromium.org\n')
        self.write('chrome/android/OWNERS', 'broad@chromium.org\n')
        self.write('a/b/c/OWNERS', 'direct@chromium.org\n')
        specific, fallback, _ = self.resolve(f'{TAB_DIR}/Tab.java')
        self.assertEqual([], specific)
        self.assertEqual(
            [
                ('broad@chromium.org', 'chrome/android/OWNERS'),
                ('root@chromium.org', 'OWNERS'),
            ],
            fallback,
        )

    def test_broad_file_include_stays_fallback(self):
        self.write('chrome/android/OWNERS', 'broad@chromium.org\n')
        self.write(
            'chrome/android/junit/OWNERS',
            'direct@chromium.org\nfile://chrome/android/OWNERS\n',
        )
        specific, fallback, _ = self.resolve(
            'chrome/android/junit/FooTest.java'
        )
        self.assertEqual(
            [('direct@chromium.org', 'chrome/android/junit/OWNERS')], specific
        )
        self.assertEqual(
            [('broad@chromium.org', 'chrome/android/OWNERS')], fallback[:1]
        )
        info = self.only_directory('chrome/android/junit/FooTest.java')
        self.assertEqual(['direct@chromium.org'], info['specific_owners'])
        self.assertEqual(['chrome/android/OWNERS'], info['fallback_sources'])
        self.assertEqual(['broad@chromium.org'], info['fallback_owners'])

    def test_last_resort_marker_is_per_line(self):
        self.write(
            'a/b/c/OWNERS',
            '# Last resort contacts are listed below.\n'
            'a@chromium.org\nb@chromium.org # {LAST_RESORT_SUGGESTION}\n'
            'c@chromium.org\n',
        )
        specific, fallback, _ = self.resolve('a/b/c/Foo.java')
        self.assertEqual(
            ['a@chromium.org', 'c@chromium.org'], _owners(specific)
        )
        self.assertEqual(['b@chromium.org'], _owners(fallback))

    def test_last_resort_section_marker(self):
        self.write(
            'a/b/c/OWNERS',
            'a@chromium.org\n# {LAST_RESORT_SUGGESTION}\nb@chromium.org\n',
        )
        specific, fallback, _ = self.resolve('a/b/c/Foo.java')
        self.assertEqual(['a@chromium.org'], _owners(specific))
        self.assertEqual(['b@chromium.org'], _owners(fallback))

    def test_wildcard_is_ignored(self):
        self.write('a/b/c/OWNERS', '*\nreal@chromium.org\n')
        specific, fallback, _ = self.resolve('a/b/c/Foo.java')
        self.assertEqual(['real@chromium.org'], _owners(specific))
        self.assertEqual([], fallback)

    def test_upper_non_broad_primary_owners_become_fallback(self):
        self.write(
            'a/b/OWNERS',
            'parent@chromium.org\n'
            'last@chromium.org # {LAST_RESORT_SUGGESTION}\n',
        )
        self.write('a/b/c/OWNERS', 'per-file *.java=direct@chromium.org\n')
        specific, fallback, unmatched = self.resolve('a/b/c/Foo.java')
        self.assertEqual(['direct@chromium.org'], _owners(specific))
        # Parent primary owners precede parent last-resort owners.
        self.assertEqual(
            [
                ('parent@chromium.org', 'a/b/OWNERS'),
                ('last@chromium.org', 'a/b/OWNERS'),
            ],
            fallback,
        )
        self.assertFalse(unmatched)
        info = self.only_directory('a/b/c/Foo.java')
        self.assertEqual(['direct@chromium.org'], info['specific_owners'])
        self.assertEqual(
            ['parent@chromium.org', 'last@chromium.org'],
            info['fallback_owners'],
        )


class AnalyzeTest(FindOwnersTestCase):
    def test_unrelated_per_file_and_history_beat_broad_owners(self):
        self.commit(
            {
                'chrome/android/OWNERS': (
                    'broad1@chromium.org\nbroad2@chromium.org\n'
                ),
                f'{TAB_DIR}/OWNERS': 'per-file *.xml=xml@chromium.org\n',
                f'{TAB_DIR}/Tab.java': None,
                f'{TAB_DIR}/TabHelper.java': None,
            }
        )
        for _ in range(2):
            self.commit(
                [f'{TAB_DIR}/Tab.java'],
                author='ckitagawa@chromium.org',
                reviewers=['Mohamed <mfiaz@chromium.org>'],
            )
        self.commit([f'{TAB_DIR}/TabHelper.java'], author='dirdev@chromium.org')

        result = self.analyze(f'{TAB_DIR}/Tab.java')
        info = result['directories'][0]
        self.assertEqual([], info['specific_owners'])
        self.assertEqual(['chrome/android/OWNERS'], info['fallback_sources'])
        self.assertEqual(
            ['broad1@chromium.org', 'broad2@chromium.org'],
            info['fallback_owners'],
        )
        self.assertEqual(
            [
                'ckitagawa@chromium.org',
                'mfiaz@chromium.org',
                'dirdev@chromium.org',
            ],
            _emails(info['recommended']),
        )
        self.assertEqual(2, info['recommended'][0]['file_commits'])
        self.assertEqual(2, info['recommended'][1]['file_reviews'])
        self.assertEqual(
            _emails(info['recommended']), _emails(result['overall'])
        )

    def test_missing_rule_note_is_deduplicated_per_directory(self):
        self.write(f'{TAB_DIR}/OWNERS', 'per-file *.xml=xml@chromium.org\n')
        files = [f'{TAB_DIR}/Tab.java', f'{TAB_DIR}/TabHelper.java']
        note = self.only_directory(*files)['note']
        self.assertEqual(1, note.count(f'{TAB_DIR}/OWNERS'))
        self.assertIn('Tab.java, TabHelper.java', note)
        self.assertEqual(1, self.run_main('--files', *files).count('Note:'))

    def test_specific_owner_ranks_first(self):
        self.commit(
            {'a/b/c/OWNERS': 'owner@chromium.org\n', 'a/b/c/Foo.java': None}
        )
        for _ in range(2):
            self.commit(['a/b/c/Foo.java'], author='busy@chromium.org')
        info = self.only_directory('a/b/c/Foo.java')
        self.assertEqual(['a/b/c/OWNERS'], info['specific_sources'])
        self.assertEqual(
            ['owner@chromium.org', 'busy@chromium.org'],
            _emails(info['recommended']),
        )
        self.assertEqual(
            [True, False], [c['specific'] for c in info['recommended']]
        )

    def test_ties_are_sorted_by_email(self):
        self.commit(['a/b/c/Foo.java'], author='bbb@chromium.org')
        self.commit(['a/b/c/Foo.java'], author='aaa@chromium.org')
        info = self.only_directory('a/b/c/Foo.java')
        self.assertEqual(
            ['aaa@chromium.org', 'bbb@chromium.org'],
            _emails(info['recommended']),
        )

    def test_top_limits_recommendations(self):
        for i in range(4):
            self.commit(['a/b/c/Foo.java'], author=f'dev{i}@chromium.org')
        info = self.only_directory('a/b/c/Foo.java', top=2)
        self.assertEqual(2, len(info['recommended']))

    def test_google_and_chromium_identities_merge(self):
        self.commit(['a/b/c/Foo.java'], author='dev@google.com')
        self.commit(
            ['a/b/c/Foo.java'],
            author='other@chromium.org',
            reviewers=['Dev <dev@chromium.org>'],
        )
        info = self.only_directory('a/b/c/Foo.java')
        emails = _emails(info['recommended'])
        self.assertEqual(['dev@chromium.org', 'other@chromium.org'], emails)
        self.assertEqual(1, info['recommended'][0]['file_commits'])
        self.assertEqual(1, info['recommended'][0]['file_reviews'])

    def test_google_only_identity_keeps_google_email(self):
        self.commit(['a/b/c/Foo.java'], author='solo@google.com')
        info = self.only_directory('a/b/c/Foo.java')
        self.assertEqual(['solo@google.com'], _emails(info['recommended']))

    def test_bots_and_groups_are_filtered(self):
        self.commit(['a/b/c/Foo.java'], author='foo-reviews@google.com')
        self.commit(
            ['a/b/c/Foo.java'],
            author='real@chromium.org',
            reviewers=[
                'mdb.fake@google.com',
                'x-owners@google.com',
                'tricorder@google.com',
                'someone-team@google.com',
                'nobody-noreply@google.com',
                'chrome-metrics-team@google.com',
                'clank-foo-team@google.com',
                'autoroller@chromium.org',
                'Good <good@chromium.org>',
            ],
        )
        info = self.only_directory('a/b/c/Foo.java')
        self.assertEqual(
            ['good@chromium.org', 'real@chromium.org'],
            sorted(_emails(info['recommended'])),
        )

    def test_team_review_aliases_are_filtered(self):
        self.commit(
            ['a/b/c/Foo.java'], author='enterprise-policy-review@google.com'
        )
        self.commit(
            ['a/b/c/Foo.java'],
            author='real@chromium.org',
            reviewers=[
                'foo-reviews@google.com',
                'foo+reviews@google.com',
                'reviews@google.com',
                'foo-owners@google.com',
                'owners@google.com',
                'Good <good@chromium.org>',
            ],
        )
        info = self.only_directory('a/b/c/Foo.java')
        self.assertEqual(
            ['good@chromium.org', 'real@chromium.org'],
            sorted(_emails(info['recommended'])),
        )

    def test_current_user_is_excluded_everywhere(self):
        self.write('a/b/c/OWNERS', f'{ME}\nother@chromium.org\n')
        self.commit(['a/b/c/Foo.java'])
        self.commit(
            ['a/b/c/Foo.java'],
            author=ME,
            reviewers=['other@example.com', 'keep@chromium.org'],
        )
        self.commit(['a/b/c/Foo.java'], author='me@google.com')
        self.commit(['a/b/c/Foo.java'], author='other@example.com')
        info = self.only_directory('a/b/c/Foo.java')
        self.assertEqual(['other@chromium.org'], info['specific_owners'])
        self.assertEqual(
            ['other@chromium.org', 'other@example.com', 'keep@chromium.org'],
            _emails(info['recommended']),
        )

    def test_cold_file_retries_without_one_year_limit(self):
        self.commit(['a/b/c/Cold.java'], date=OLD_DATE)
        self.commit(
            ['a/b/c/Cold.java'], author='old@chromium.org', date=OLD_DATE
        )
        info = self.only_directory('a/b/c/Cold.java')
        self.assertEqual(['old@chromium.org'], _emails(info['recommended']))
        self.assertEqual(1, info['recommended'][0]['file_commits'])

    def test_broad_fallback_owners_used_without_git_activity(self):
        self.commit(
            {
                'chrome/android/OWNERS': (
                    'broad1@chromium.org\nbroad2@chromium.org\n'
                ),
                f'{TAB_DIR}/Tab.java': None,
            }
        )
        info = self.only_directory(f'{TAB_DIR}/Tab.java')
        self.assertEqual(
            ['broad1@chromium.org', 'broad2@chromium.org'],
            _emails(info['recommended']),
        )
        self.assertTrue(
            all(
                c['fallback'] and not c['specific'] for c in info['recommended']
            )
        )
        text = self.run_main('--files', f'{TAB_DIR}/Tab.java')
        self.assertIn(
            'Fallback OWNERS: chrome/android/OWNERS (2 owner(s))', text
        )
        self.assertIn('broad2@chromium.org', text)

    def test_root_level_file_skips_directory_activity(self):
        self.commit({'DEPS': 'deps\n'})
        self.commit(['DEPS'], author='dev@chromium.org')
        with mock.patch.object(
            find_owners, '_activity', wraps=find_owners._activity
        ) as activity:
            info = self.only_directory('DEPS')
        # Only the file's own history is scanned; never `.` or an empty path,
        # which would scan the whole repo.
        self.assertEqual(
            [['DEPS']], [call.args[1] for call in activity.call_args_list]
        )
        self.assertEqual('', info['directory'])
        self.assertEqual(['dev@chromium.org'], _emails(info['recommended']))
        self.assertEqual(0, info['recommended'][0]['dir_commits'])

    def test_fallback_owners_not_listed_when_history_exists(self):
        self.commit(
            {
                'chrome/android/OWNERS': 'broad1@chromium.org\n',
                f'{TAB_DIR}/Tab.java': None,
            }
        )
        self.commit([f'{TAB_DIR}/Tab.java'], author='dev@chromium.org')
        text = self.run_main('--files', f'{TAB_DIR}/Tab.java')
        self.assertIn('dev@chromium.org', text)
        self.assertNotIn('broad1@chromium.org', text)

    def test_all_fallback_sources_are_kept_in_order(self):
        self.write('OWNERS', 'root@chromium.org\n')
        self.write('chrome/android/OWNERS', 'broad@chromium.org\n')
        info = self.only_directory(f'{TAB_DIR}/Tab.java')
        self.assertEqual(
            ['chrome/android/OWNERS', 'OWNERS'], info['fallback_sources']
        )

    def test_deleted_file_uses_history_and_owners(self):
        self.commit({'a/b/c/OWNERS': 'owner@chromium.org\n'})
        self.commit(['a/b/c/Gone.java'], author='dev@chromium.org')
        os.remove(os.path.join(self.root, 'a/b/c/Gone.java'))
        files = find_owners.detect_modified_files(self.root)
        self.assertEqual(['a/b/c/Gone.java'], files)
        info = self.only_directory(*files)
        self.assertEqual(
            ['owner@chromium.org', 'dev@chromium.org'],
            _emails(info['recommended']),
        )

    def test_directories_are_grouped_and_overall_prefers_wider_people(self):
        self.commit(['a/b/prod/Foo.java', 'a/b/tests/FooTest.java'])
        self.commit(['a/b/prod/Foo.java'], author='aaa@chromium.org')
        self.commit(
            ['a/b/prod/Foo.java', 'a/b/tests/FooTest.java'],
            author='zzz@chromium.org',
        )
        result = self.analyze('a/b/tests/FooTest.java', 'a/b/prod/Foo.java')
        dirs = result['directories']
        self.assertEqual(
            ['a/b/prod', 'a/b/tests'], [d['directory'] for d in dirs]
        )
        self.assertEqual(
            ['aaa@chromium.org', 'zzz@chromium.org'],
            _emails(dirs[0]['recommended']),
        )
        self.assertEqual(['zzz@chromium.org'], _emails(dirs[1]['recommended']))
        self.assertEqual('zzz@chromium.org', result['overall'][0]['email'])
        text = self.run_main(
            '--files', 'a/b/tests/FooTest.java', 'a/b/prod/Foo.java'
        )
        self.assertIn('Overall:', text)

    def test_overall_counts_each_directory_once(self):
        self.commit(
            [
                'a/b/prod/Foo.java',
                'a/b/prod/Bar.java',
                'a/b/tests/FooTest.java',
            ],
            author='dev@chromium.org',
        )
        result = self.analyze(
            'a/b/prod/Foo.java', 'a/b/prod/Bar.java', 'a/b/tests/FooTest.java'
        )
        dev = result['overall'][0]
        self.assertEqual(2, dev['directories'])
        self.assertEqual(2, dev['file_commits'])
        self.assertEqual(2, dev['dir_commits'])

    def test_repo_without_commits_does_not_crash(self):
        self.assertEqual([], find_owners.detect_modified_files(self.root))
        self.assertEqual(
            [], self.only_directory('a/b/c/Foo.java')['recommended']
        )


class FilesTest(FindOwnersTestCase):
    def test_working_tree_ignores_untracked_files(self):
        self.commit(['a/b/c/Foo.java', 'a/b/c/Staged.java', 'a/b/c/Clean.java'])
        self.write('a/b/c/Foo.java')
        self.write('a/b/c/Staged.java')
        self.git('add', 'a/b/c/Staged.java')
        self.write('a/b/c/Untracked.java')
        self.assertEqual(
            ['a/b/c/Foo.java', 'a/b/c/Staged.java'],
            find_owners.detect_modified_files(self.root),
        )

    def test_clean_feature_branch_diffs_from_main(self):
        self.commit(['a/b/c/Foo.java'])
        self.git('checkout', '-q', '-b', 'feature')
        self.commit(['a/b/c/Bar.java'], author=ME)
        self.commit(['a/b/c/Baz.java'], author=ME)
        self.assertEqual(
            ['a/b/c/Bar.java', 'a/b/c/Baz.java'],
            find_owners.detect_modified_files(self.root),
        )

    def test_clean_main_reports_nothing(self):
        self.commit(['a/b/c/Foo.java'])
        self.assertEqual([], find_owners.detect_modified_files(self.root))
        self.assertIn('No modified files', self.run_main())

    def test_files_resolve_against_cwd_then_repo_root(self):
        self.commit(['a/b/c/Foo.java'], author='dev@chromium.org')
        subdir = os.path.join(self.root, 'a/b')
        rel = find_owners._repo_relative
        self.assertEqual('a/b/c/Foo.java', rel('c/Foo.java', self.root, subdir))
        self.assertEqual(
            'a/b/c/Foo.java', rel('a/b/c/Foo.java', self.root, self.outer)
        )
        self.assertEqual('', rel('../outside.java', self.root, self.root))
        self.assertIn(
            'a/b/c/ (Foo.java)',
            self.run_main('--files', 'c/Foo.java', cwd=subdir),
        )

    def test_files_resolve_through_symlinked_workspace(self):
        link = os.path.join(self.outer, 'link')
        os.symlink(self.root, link)
        rel = find_owners._repo_relative
        self.assertEqual(
            'a/b/c/Foo.java', rel('a/b/c/Foo.java', link, self.root)
        )
        self.assertEqual(
            'a/b/c/Foo.java',
            rel(os.path.join(link, 'a/b/c/Foo.java'), self.root, self.root),
        )
        self.assertEqual(
            'a/b/c/Foo.java',
            rel(os.path.join(self.root, 'a/b/c/Foo.java'), link, link),
        )
        self.assertEqual(
            '', rel(os.path.join(self.outer, 'x.java'), link, link)
        )


if __name__ == '__main__':
    unittest.main()
