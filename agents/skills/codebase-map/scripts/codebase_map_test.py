#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for codebase_map.py: OWNERS resolution, DIR_METADATA parsing, header
comment extraction, git activity, tree extraction, the annotate/targets round
trip, rendering, serving, and the viewer: its pure helpers and, against a
small fake DOM, the page script itself (both under Node)."""

import http.client
import io
import json
import os
import platform
import queue
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import textwrap
import threading
import unittest
from unittest import mock

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _SCRIPT_DIR)

import codebase_map  # noqa: E402


class _TempRepo:
    """A throwaway directory tree; `write('a/b', text)` creates parents."""

    def __init__(self):
        self._tmp = tempfile.TemporaryDirectory()
        # realpath: on macOS the temp dir is behind a /var -> /private/var
        # symlink, and git reports the resolved path.
        self.root = os.path.realpath(self._tmp.name)

    def write(self, rel, text=''):
        path = os.path.join(self.root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        mode = 'wb' if isinstance(text, bytes) else 'w'
        encoding = None if isinstance(text, bytes) else 'utf-8'
        with open(path, mode, encoding=encoding) as f:
            f.write(text if isinstance(text, bytes) else textwrap.dedent(text))
        return path

    def cleanup(self):
        self._tmp.cleanup()


class _GitRepo(_TempRepo):
    """A _TempRepo that is also a git repository, isolated from user config."""

    def __init__(self):
        super().__init__()
        self.git('init', '-q')

    def git(self, *args, env=None):
        full_env = dict(os.environ)
        full_env.update(
            GIT_CONFIG_GLOBAL=os.devnull,
            GIT_CONFIG_NOSYSTEM='1',
            **(env or {}),
        )
        return subprocess.run(
            ['git', '-C', self.root] + list(args),
            check=True,
            capture_output=True,
            text=True,
            env=full_env,
        ).stdout

    def commit(self, files, author, message):
        """Writes `files` ({rel: text}), stages them, commits as `author`.

        `author` is (name, email). Returns the full commit hash.
        """
        for rel, text in files.items():
            self.write(rel, text)
        self.git('add', '--', *files)
        name, email = author
        self.git(
            'commit',
            '-q',
            '-m',
            message,
            env={
                'GIT_AUTHOR_NAME': name,
                'GIT_AUTHOR_EMAIL': email,
                'GIT_COMMITTER_NAME': name,
                'GIT_COMMITTER_EMAIL': email,
            },
        )
        return self.git('rev-parse', 'HEAD').strip()


def _try_symlink(target, link):
    """Creates a symlink; False where the platform or account can't.

    Windows needs developer mode or admin rights for `os.symlink`.
    """
    try:
        os.symlink(target, link)
    except (OSError, NotImplementedError):
        return False
    return True


class OwnersIndexTest(unittest.TestCase):
    def setUp(self):
        self.repo = _TempRepo()
        self.addCleanup(self.repo.cleanup)
        self.index = codebase_map.OwnersIndex(self.repo.root)

    def test_parse_basic(self):
        self.repo.write(
            'a/OWNERS',
            """\
            # Team lead.
            alice@chromium.org
            bob@chromium.org
            """,
        )
        info = self.index.parse('a')
        self.assertEqual(
            info['owners'], ['alice@chromium.org', 'bob@chromium.org']
        )
        self.assertFalse(info['noparent'])
        self.assertEqual(info['comments'], {'alice@chromium.org': 'Team lead.'})

    def test_missing_owners_is_none(self):
        self.assertIsNone(self.index.parse('nope'))

    def test_chain_stops_at_noparent(self):
        self.repo.write('OWNERS', 'root@chromium.org\n')
        self.repo.write('a/OWNERS', 'set noparent\nmid@chromium.org\n')
        self.repo.write('a/b/OWNERS', 'leaf@chromium.org\n')
        paths = [i['path'] for i in self.index.chain('a/b')]
        self.assertEqual(paths, ['a/b/OWNERS', 'a/OWNERS'])

    def test_chain_reaches_root_without_noparent(self):
        self.repo.write('OWNERS', 'root@chromium.org\n')
        self.repo.write('a/b/OWNERS', 'leaf@chromium.org\n')
        paths = [i['path'] for i in self.index.chain('a/b/c')]
        self.assertEqual(paths, ['a/b/OWNERS', 'OWNERS'])

    def test_resolve_nearest_owners_only(self):
        self.repo.write('OWNERS', 'root@chromium.org\n')
        self.repo.write('a/OWNERS', 'mid@chromium.org\n')
        self.repo.write('a/b/OWNERS', 'leaf@chromium.org\n')
        r = self.index.resolve_file('a/b/x.cc')
        self.assertEqual(r['owners'], ['leaf@chromium.org'])
        self.assertEqual(r['chain'], ['a/b/OWNERS', 'a/OWNERS', 'OWNERS'])

    def test_resolve_skips_empty_owners_file(self):
        # An OWNERS file with only per-file rules doesn't list "people", so
        # unmatched files fall through to the parent.
        self.repo.write('a/OWNERS', 'mid@chromium.org\n')
        self.repo.write('a/b/OWNERS', 'per-file *.gn=gn@chromium.org\n')
        self.assertEqual(
            self.index.resolve_file('a/b/x.cc')['owners'], ['mid@chromium.org']
        )
        r = self.index.resolve_file('a/b/BUILD.gn')
        # per-file owners come first, then the nearest directory owners.
        self.assertEqual(r['owners'], ['gn@chromium.org', 'mid@chromium.org'])
        self.assertEqual(r['rules'][0]['pattern'], '*.gn')

    def test_per_file_noparent_excludes_dir_owners(self):
        self.repo.write(
            'a/OWNERS',
            """\
            dir@chromium.org
            per-file secret_*=set noparent
            per-file secret_*=sec@chromium.org
            """,
        )
        r = self.index.resolve_file('a/secret_thing.cc')
        self.assertEqual(r['owners'], ['sec@chromium.org'])
        self.assertTrue(r['rules'][0]['noparent'])
        self.assertEqual(
            self.index.resolve_file('a/other.cc')['owners'],
            ['dir@chromium.org'],
        )

    def test_per_file_noparent_ends_the_chain(self):
        # Under `per-file ... set noparent` only the rule's owners can
        # approve: not the directory owners of the same OWNERS file, and not
        # anything above it. `chain` feeds the viewer's "owner via" badge.
        self.repo.write('OWNERS', 'root@chromium.org\n')
        self.repo.write(
            'a/OWNERS',
            'dir@chromium.org\n'
            'per-file *.mojom=set noparent\n'
            'per-file *.mojom=sec@chromium.org\n',
        )
        self.repo.write('a/sub/OWNERS', 'per-file *.gn=gn@chromium.org\n')
        r = self.index.resolve_file('a/x.mojom')
        self.assertEqual(r['owners'], ['sec@chromium.org'])
        self.assertEqual(r['chain'], [])
        # The rule also covers subdirectories; an OWNERS file on the way that
        # lists nobody stays in the chain, it grants nothing.
        r = self.index.resolve_file('a/sub/x.mojom')
        self.assertEqual(r['owners'], ['sec@chromium.org'])
        self.assertEqual(r['chain'], ['a/sub/OWNERS'])
        # Files the rule doesn't match keep the whole chain.
        self.assertEqual(
            self.index.resolve_file('a/x.cc')['chain'], ['a/OWNERS', 'OWNERS']
        )

    def test_per_file_multiple_patterns_and_owners(self):
        self.repo.write(
            'a/OWNERS', 'per-file foo.cc,bar.cc=x@chromium.org,y@chromium.org\n'
        )
        info = self.index.parse('a')
        self.assertEqual(
            [p['pattern'] for p in info['per_file']], ['foo.cc', 'bar.cc']
        )
        self.assertEqual(
            info['per_file'][1]['owners'], ['x@chromium.org', 'y@chromium.org']
        )

    def test_file_include_expands_owners(self):
        self.repo.write('team/OWNERS', 't1@chromium.org\nt2@chromium.org\n')
        self.repo.write('a/OWNERS', 'file://team/OWNERS\nlocal@chromium.org\n')
        info = self.index.parse('a')
        self.assertEqual(
            info['owners'],
            ['t1@chromium.org', 't2@chromium.org', 'local@chromium.org'],
        )
        self.assertEqual(info['includes'], ['team/OWNERS'])

    def test_file_include_of_non_owners_file(self):
        self.repo.write('a/SECURITY_OWNERS', 'sec@chromium.org  # comment\n')
        self.repo.write(
            'a/OWNERS', 'per-file *.mojom=file://a/SECURITY_OWNERS\n'
        )
        self.assertEqual(
            self.index.resolve_file('a/x.mojom')['owners'], ['sec@chromium.org']
        )

    def test_include_cycle_lists_each_owner_once(self):
        self.repo.write('a/OWNERS', 'file://b/OWNERS\na@chromium.org\n')
        self.repo.write('b/OWNERS', 'file://a/OWNERS\nb@chromium.org\n')
        self.repo.write('c/OWNERS', 'file://c/OWNERS\nc@chromium.org\n')
        info = self.index.parse('a')  # Must terminate.
        # b's include of a leads back to the file being parsed: nothing is
        # missing, so it is not reported as unresolved.
        self.assertEqual(info['owners'], ['b@chromium.org', 'a@chromium.org'])
        self.assertEqual(info['unresolved'], [])
        # Entering the cycle from the other side finds the same people.
        self.assertCountEqual(
            self.index.parse('b')['owners'],
            ['a@chromium.org', 'b@chromium.org'],
        )
        # A file that includes itself.
        self.assertEqual(self.index.parse('c')['owners'], ['c@chromium.org'])
        self.assertEqual(self.index.parse('c')['unresolved'], [])

    def test_depth_truncated_parse_is_not_cached(self):
        # d0 includes d1 includes ... d5, which lists a real owner. Reaching
        # d4 from d0 hits the depth limit, but a direct lookup of d3 must not
        # see that truncated result.
        for i in range(5):
            self.repo.write(f'd{i}/OWNERS', f'file://d{i + 1}/OWNERS\n')
        self.repo.write('d5/OWNERS', 'real@chromium.org\n')
        d0 = self.index.parse('d0')
        self.assertEqual(d0['owners'], [])
        self.assertEqual(d0['unresolved'], ['file://d5/OWNERS'])
        self.assertEqual(
            self.index.parse('d3')['owners'], ['real@chromium.org']
        )

    def test_set_noparent_with_trailing_comment(self):
        self.repo.write('OWNERS', 'root@chromium.org\n')
        self.repo.write(
            'a/OWNERS', 'set noparent  # security review\nmid@chromium.org\n'
        )
        info = self.index.parse('a')
        self.assertTrue(info['noparent'])
        self.assertEqual(info['owners'], ['mid@chromium.org'])
        self.assertEqual(
            [i['path'] for i in self.index.chain('a')], ['a/OWNERS']
        )

    def test_per_file_set_noparent_with_trailing_comment(self):
        self.repo.write(
            'a/OWNERS',
            'dir@chromium.org\n'
            'per-file *.mojom=set noparent  # IPC review\n'
            'per-file *.mojom=sec@chromium.org\n',
        )
        r = self.index.resolve_file('a/x.mojom')
        self.assertEqual(r['owners'], ['sec@chromium.org'])

    def test_file_include_with_trailing_comment(self):
        self.repo.write('team/OWNERS', 't@chromium.org\n')
        self.repo.write('a/OWNERS', 'file://team/OWNERS  # the team\n')
        info = self.index.parse('a')
        self.assertEqual(info['owners'], ['t@chromium.org'])
        self.assertEqual(info['includes'], ['team/OWNERS'])

    def test_ancestor_per_file_applies_to_subdirectories(self):
        # Gerrit's find-owners glob prefixes patterns with {**/,}, so a rule
        # in a/ covers a/sub/x.mojom when nothing nearer lists people.
        self.repo.write(
            'a/OWNERS',
            'dir@chromium.org\nper-file *.mojom=ipc@chromium.org\n',
        )
        r = self.index.resolve_file('a/sub/x.mojom')
        self.assertEqual(r['owners'], ['ipc@chromium.org', 'dir@chromium.org'])
        self.assertEqual(r['rules'][0]['source'], 'a/OWNERS')
        self.assertEqual(
            self.index.resolve_file('a/sub/x.cc')['owners'],
            ['dir@chromium.org'],
        )

    def test_ancestor_per_file_beyond_nearest_owners_is_not_nearest(self):
        # Once a nearer OWNERS lists people, the walk stops there for the
        # "nearest approvers" set, whether or not an ancestor rule matches.
        self.repo.write(
            'a/OWNERS',
            'dir@chromium.org\nper-file *.mojom=ipc@chromium.org\n',
        )
        self.repo.write('a/sub/OWNERS', 'leaf@chromium.org\n')
        r = self.index.resolve_file('a/sub/x.mojom')
        self.assertEqual(r['owners'], ['leaf@chromium.org'])
        self.assertEqual(r['chain'], ['a/sub/OWNERS', 'a/OWNERS'])
        # The ancestor rule's owners can approve too; it is reported apart
        # from the nearest set.
        self.assertEqual(
            [(x['source'], x['owners']) for x in r['inherited_rules']],
            [('a/OWNERS', ['ipc@chromium.org'])],
        )
        self.assertEqual(
            self.index.resolve_file('a/sub/x.cc')['inherited_rules'], []
        )

    def test_ancestor_per_file_noparent_ends_the_chain_there(self):
        # The rule sits above the nearest OWNERS that lists people. That
        # nearer file still applies, but the rule's own OWNERS file and
        # everything above it no longer grant directory-level approval.
        self.repo.write('OWNERS', 'root@chromium.org\n')
        self.repo.write(
            'a/OWNERS',
            'dir@chromium.org\n'
            'per-file *.mojom=set noparent\n'
            'per-file *.mojom=sec@chromium.org\n',
        )
        self.repo.write('a/sub/OWNERS', 'leaf@chromium.org\n')
        r = self.index.resolve_file('a/sub/deep/x.mojom')
        self.assertEqual(r['owners'], ['leaf@chromium.org'])
        self.assertEqual(r['chain'], ['a/sub/OWNERS'])
        self.assertEqual(
            [(x['source'], x['owners']) for x in r['inherited_rules']],
            [('a/OWNERS', ['sec@chromium.org'])],
        )
        # A file the rule doesn't match keeps the whole chain.
        self.assertEqual(
            self.index.resolve_file('a/sub/deep/x.cc')['chain'],
            ['a/sub/OWNERS', 'a/OWNERS', 'OWNERS'],
        )

    def test_per_file_pattern_with_directory(self):
        self.repo.write(
            'a/OWNERS',
            'dir@chromium.org\nper-file sub/*.cc=sub@chromium.org\n',
        )
        self.assertEqual(
            self.index.resolve_file('a/sub/x.cc')['owners'],
            ['sub@chromium.org', 'dir@chromium.org'],
        )
        # `*` doesn't cross `/`, and the directory name must match.
        self.assertEqual(
            self.index.resolve_file('a/other/x.cc')['owners'],
            ['dir@chromium.org'],
        )
        self.assertEqual(
            self.index.resolve_file('a/sub/deeper/x.cc')['owners'],
            ['dir@chromium.org'],
        )

    def test_resolve_dir_skips_owners_file_without_people(self):
        self.repo.write('a/OWNERS', 'mid@chromium.org\n')
        self.repo.write('a/b/OWNERS', 'per-file *.gn=gn@chromium.org\n')
        self.assertEqual(
            self.index.resolve_dir('a/b'), (['mid@chromium.org'], 'a/OWNERS')
        )
        self.assertEqual(
            self.index.resolve_dir('a'), (['mid@chromium.org'], 'a/OWNERS')
        )
        self.assertEqual(self.index.resolve_dir('elsewhere'), ([], None))

    def test_inline_comment_stripped(self):
        self.repo.write('a/OWNERS', 'alice@chromium.org  # tabs\n')
        self.assertEqual(
            self.index.parse('a')['owners'], ['alice@chromium.org']
        )

    def test_star_owner(self):
        self.repo.write('a/OWNERS', '*\n')
        self.assertEqual(self.index.resolve_file('a/x.cc')['owners'], ['*'])

    def test_include_relative_and_absolute(self):
        self.repo.write('infra/g1/OWNERS', 'one@chromium.org\n')
        self.repo.write('infra/g2/OWNERS', 'two@chromium.org\n')
        self.repo.write(
            'a/b/OWNERS',
            """\
            include ../../infra/g1/OWNERS
            include /infra/g2/OWNERS  # absolute
            """,
        )
        info = self.index.parse('a/b')
        self.assertEqual(
            info['owners'], ['one@chromium.org', 'two@chromium.org']
        )
        self.assertEqual(
            info['includes'], ['infra/g1/OWNERS', 'infra/g2/OWNERS']
        )
        self.assertEqual(info['ignored'], [])

    def test_code_owners_file_syntax(self):
        # Seen in chrome/browser/ui/tabs/tab_drag_api/OWNERS.
        self.repo.write('g/OWNERS', 'g@chromium.org\n')
        self.repo.write('a/sub/OWNERS', 'sub@chromium.org\n')
        self.repo.write(
            'a/OWNERS',
            """\
            file: //g/OWNERS
            file:sub/OWNERS
            per-file *.gn=file: //g/OWNERS
            file:other-project:main:/x/OWNERS
            """,
        )
        info = self.index.parse('a')
        self.assertEqual(info['owners'], ['g@chromium.org', 'sub@chromium.org'])
        self.assertEqual(info['includes'], ['g/OWNERS', 'a/sub/OWNERS'])
        self.assertEqual(info['per_file'][0]['owners'], ['g@chromium.org'])
        self.assertEqual(info['ignored'], ['file:other-project:main:/x/OWNERS'])

    def test_unknown_directive_is_not_an_owner(self):
        self.repo.write(
            'a/OWNERS',
            """\
            alice@chromium.org
            set something-else
            bogus
            """,
        )
        info = self.index.parse('a')
        self.assertEqual(info['owners'], ['alice@chromium.org'])
        self.assertEqual(info['ignored'], ['set something-else', 'bogus'])

    def test_unresolvable_include_is_reported_not_an_owner(self):
        self.repo.write('OWNERS', 'root@chromium.org\n')
        self.repo.write(
            'a/OWNERS',
            'include ../nowhere/OWNERS\nper-file *.gn=file://gone/GN_OWNERS\n',
        )
        info = self.index.parse('a')
        self.assertEqual(info['owners'], [])
        self.assertEqual(info['unresolved'], ['file://nowhere/OWNERS'])
        self.assertEqual(info['per_file'][0]['owners'], [])
        self.assertEqual(
            info['per_file'][0]['unresolved'], ['file://gone/GN_OWNERS']
        )
        self.assertEqual(info['ignored'], [])
        # a/OWNERS lists nobody, so approval falls through to the parent: the
        # planner is never handed a `file://` token as a reviewer. The rules
        # that lost their owners are still reported so it can flag them.
        r = self.index.resolve_file('a/BUILD.gn')
        self.assertEqual(r['owners'], ['root@chromium.org'])
        self.assertEqual(
            [(x['source'], x['pattern'], x['unresolved']) for x in r['rules']],
            [
                ('a/OWNERS', '*.gn', ['file://gone/GN_OWNERS']),
                ('a/OWNERS', None, ['file://nowhere/OWNERS']),
                ('OWNERS', None, []),
            ],
        )
        self.assertEqual(
            self.index.resolve_dir('a'), (['root@chromium.org'], 'OWNERS')
        )

    def test_include_inside_non_owners_file(self):
        self.repo.write('groups/OWNERS', 'deep@chromium.org\n')
        self.repo.write('groups/LIST', 'include OWNERS\nlist@chromium.org\n')
        self.repo.write('a/OWNERS', 'file://groups/LIST\n')
        self.assertEqual(
            self.index.parse('a')['owners'],
            ['deep@chromium.org', 'list@chromium.org'],
        )

    def test_last_resort_suggestion(self):
        self.repo.write(
            'inc/OWNERS', 'lr2@chromium.org #{LAST_RESORT_SUGGESTION}\n'
        )
        self.repo.write(
            'a/OWNERS',
            """\
            main@chromium.org
            lr@chromium.org  #{LAST_RESORT_SUGGESTION}
            file://inc/OWNERS
            """,
        )
        info = self.index.parse('a')
        self.assertEqual(
            info['owners'],
            ['main@chromium.org', 'lr@chromium.org', 'lr2@chromium.org'],
        )
        # Tags inside included files count too.
        self.assertEqual(
            info['last_resort'], ['lr@chromium.org', 'lr2@chromium.org']
        )

    def test_last_resort_is_scoped_to_the_tagged_entry(self):
        self.repo.write(
            'a/OWNERS',
            'boss@chromium.org  #{LAST_RESORT_SUGGESTION}\n'
            'mid@chromium.org\n'
            'per-file *.gn=gn@chromium.org  #{LAST_RESORT_SUGGESTION}\n',
        )
        self.repo.write('a/b/OWNERS', 'boss@chromium.org\n')
        r = self.index.resolve_file('a/x.cc')
        self.assertEqual(r['owners'], ['boss@chromium.org', 'mid@chromium.org'])
        self.assertEqual(r['last_resort'], ['boss@chromium.org'])
        # The nearer OWNERS lists boss without the tag: a regular owner here.
        r = self.index.resolve_file('a/b/y.cc')
        self.assertEqual(r['owners'], ['boss@chromium.org'])
        self.assertEqual(r['last_resort'], [])
        # A tag on a per-file rule applies to that rule's owners.
        r = self.index.resolve_file('a/BUILD.gn')
        self.assertEqual(
            r['last_resort'], ['gn@chromium.org', 'boss@chromium.org']
        )


class DirMetadataTest(unittest.TestCase):
    def setUp(self):
        self.repo = _TempRepo()
        self.addCleanup(self.repo.cleanup)

    def test_flat_and_nested(self):
        path = self.repo.write(
            'a/DIR_METADATA',
            """\
            # Comment line.
            monorail {
              component: "UI>Browser>Tabs"
            }
            buganizer_public: {
              component_id: 1456383  # trailing comment
            }
            team_email: "tabs@chromium.org"
            os: LINUX
            os: WINDOWS
            """,
        )
        md = codebase_map._parse_dir_metadata(path)
        self.assertEqual(md['monorail']['component'], 'UI>Browser>Tabs')
        self.assertEqual(md['buganizer_public']['component_id'], '1456383')
        self.assertEqual(md['team_email'], 'tabs@chromium.org')
        self.assertEqual(md['os'], ['LINUX', 'WINDOWS'])

    def test_missing_file_parses_to_none(self):
        self.assertIsNone(
            codebase_map._parse_dir_metadata(
                os.path.join(self.repo.root, 'nope')
            )
        )

    def test_one_line_block_does_not_swallow_later_keys(self):
        path = self.repo.write(
            'a/DIR_METADATA',
            """\
            monorail { component: "Internals>Accessibility" }
            team_email: "a11y@chromium.org"
            buganizer_public: { component_id: 123 }
            """,
        )
        md = codebase_map._parse_dir_metadata(path)
        self.assertEqual(
            md,
            {
                'monorail': {'component': 'Internals>Accessibility'},
                'team_email': 'a11y@chromium.org',
                'buganizer_public': {'component_id': '123'},
            },
        )

    def test_mixins_fill_in_missing_fields(self):
        self.repo.write(
            'common/COMMON_METADATA',
            """\
            buganizer_public { component_id: 999 }
            team_email: "common@chromium.org"
            os: LINUX
            """,
        )
        path = self.repo.write(
            'a/DIR_METADATA',
            """\
            mixins: "//common/COMMON_METADATA"
            team_email: "own@chromium.org"
            """,
        )
        md = codebase_map._parse_dir_metadata(path, self.repo.root)
        self.assertEqual(md['team_email'], 'own@chromium.org')  # Own wins.
        self.assertEqual(md['buganizer_public']['component_id'], '999')
        self.assertEqual(md['os'], 'LINUX')
        # Without a repo root, mixins are left unresolved.
        self.assertNotIn(
            'buganizer_public', codebase_map._parse_dir_metadata(path)
        )

    def test_inherits_missing_fields_from_parents(self):
        self.repo.write(
            'DIR_METADATA', 'team_email: "root@chromium.org"\nos: LINUX\n'
        )
        self.repo.write(
            'a/DIR_METADATA',
            'buganizer_public { component_id: 1 }\n'
            'team_email: "a@chromium.org"\n',
        )
        self.repo.write(
            'a/b/c/DIR_METADATA', 'buganizer_public { component_id: 3 }\n'
        )
        cache = {}

        def resolve(rel_dir):
            return codebase_map._inherited_dir_metadata(
                self.repo.root, rel_dir, cache
            )

        # The nearest file that sets a field wins; the rest is inherited.
        self.assertEqual(
            resolve('a/b/c'),
            (
                {
                    'buganizer_public': {'component_id': '3'},
                    'team_email': 'a@chromium.org',
                    'os': 'LINUX',
                },
                'a/b/c/DIR_METADATA',
            ),
        )
        # No file of its own: the parent's values, and where they came from.
        self.assertEqual(
            resolve('a/b'),
            (
                {
                    'buganizer_public': {'component_id': '1'},
                    'team_email': 'a@chromium.org',
                    'os': 'LINUX',
                },
                'a/DIR_METADATA',
            ),
        )
        self.assertEqual(
            resolve(''),
            (
                {'team_email': 'root@chromium.org', 'os': 'LINUX'},
                'DIR_METADATA',
            ),
        )

    def test_no_metadata_up_to_the_root(self):
        self.assertEqual(
            codebase_map._inherited_dir_metadata(self.repo.root, 'x/y', {}),
            (None, None),
        )

    def test_component_preference(self):
        component = codebase_map._component
        self.assertEqual(
            component(
                {
                    'monorail': {'component': 'Old>Comp'},
                    'buganizer': {'component_id': '2'},
                    'buganizer_public': {'component_id': '1'},
                }
            ),
            '1',
        )
        self.assertEqual(
            component(
                {
                    'monorail': {'component': 'Old>Comp'},
                    'buganizer': {'component_id': '2'},
                }
            ),
            '2',
        )
        self.assertEqual(
            component({'monorail': {'component': 'Old>Comp'}}), 'Old>Comp'
        )
        self.assertEqual(component({'team_email': 'x@y'}), '')
        self.assertEqual(component(None), '')


class HeaderCommentTest(unittest.TestCase):
    LICENSE = """\
    // Copyright 2026 The Chromium Authors
    // Use of this source code is governed by a BSD-style license that can be
    // found in the LICENSE file.

    """

    def setUp(self):
        self.repo = _TempRepo()
        self.addCleanup(self.repo.cleanup)

    def _doc(self, body):
        return codebase_map._header_comment(
            self.repo.write('x.h', self.LICENSE + body)
        )

    def test_file_level_comment(self):
        self.assertEqual(
            self._doc("""\
            // Utilities for tabs.
            // Second line.

            #ifndef X_H_
            #define X_H_
            """),
            'Utilities for tabs. Second line.',
        )

    def test_class_comment_after_includes_and_namespace(self):
        self.assertEqual(
            self._doc("""\
            #ifndef X_H_
            #define X_H_

            #include <memory>

            namespace tabs {

            class Helper;

            // TODO(someone): remove this.

            // Owns the tab strip.
            class TabStrip {
             public:
            };

            }  // namespace tabs
            #endif
            """),
            'Owns the tab strip.',
        )

    def test_todo_is_not_a_description(self):
        self.assertEqual(
            self._doc("""\
            #include <memory>

            // TODO(a): fix.
            class Foo {};
            """),
            '',
        )

    def test_no_comment(self):
        self.assertEqual(self._doc('#include <memory>\nclass Foo {};\n'), '')

    def test_truncates_long_blocks(self):
        lines = '\n'.join(f'// line {i}' for i in range(20))
        doc = self._doc(lines + '\n\nclass Foo {};\n')
        self.assertIn('line 0', doc)
        self.assertNotIn(f'line {codebase_map._MAX_HEADER_COMMENT_LINES}', doc)


class ClassifyTest(unittest.TestCase):
    def test_classifies_by_name_and_directory(self):
        c = codebase_map._classify
        self.assertEqual(c('OWNERS', 'a'), 'meta')
        self.assertEqual(c('BUILD.gn', 'a'), 'build')
        self.assertEqual(c('README.md', 'a'), 'docs')
        self.assertEqual(c('foo_unittest.cc', 'a'), 'test')
        self.assertEqual(c('foo_browsertest.cc', 'a'), 'test')
        self.assertEqual(c('mock_foo.h', 'a'), 'test')
        self.assertEqual(c('foo.h', 'a/test'), 'test')
        self.assertEqual(c('foo.h', 'a/public'), 'public')
        self.assertEqual(c('foo.h', 'a'), 'header')
        self.assertEqual(c('foo.cc', 'a'), 'impl')
        self.assertEqual(c('foo.mojom', 'a'), 'mojom')
        self.assertEqual(c('foo.ts', 'a'), 'webui')

    def test_test_directories(self):
        c = codebase_map._classify
        self.assertEqual(c('foo.cc', 'a/tests'), 'test')
        self.assertEqual(c('foo.cc', 'a/browser_tests'), 'test')
        self.assertEqual(c('foo.cc', 'a/test/data'), 'test')
        # Names that merely end in "test" are not test directories.
        self.assertEqual(c('foo.cc', 'a/latest'), 'impl')
        self.assertEqual(c('foo.cc', 'a/contest'), 'impl')


class CountLinesTest(unittest.TestCase):
    def setUp(self):
        self.repo = _TempRepo()
        self.addCleanup(self.repo.cleanup)

    def test_text(self):
        self.assertEqual(
            codebase_map._count_lines(self.repo.write('a.txt', 'x\ny\nz')), 3
        )

    def test_binary_counts_as_zero(self):
        path = self.repo.write('a.png', b'\x89PNG\r\n\x00\x00\n\n\n')
        self.assertEqual(codebase_map._count_lines(path), 0)

    def test_missing_file_counts_as_zero_lines(self):
        self.assertEqual(
            codebase_map._count_lines(os.path.join(self.repo.root, 'nope')), 0
        )


def _fake_map():
    """A minimal map JSON, as `extract` would produce it."""

    def act(commits, subjects=()):
        return dict(
            codebase_map._EMPTY_ACTIVITY,
            commits=commits,
            recent=[{'subject': s} for s in subjects],
        )

    def f(path, commits, loc=10, role='impl'):
        return {
            'name': path.split('/')[-1],
            'path': path,
            'type': 'file',
            'role': role,
            'loc': loc,
            'description': '',
            'doc': '',
            'activity': act(commits),
        }

    return {
        'schema_version': codebase_map._SCHEMA_VERSION,
        'root': 'a',
        'repo_root': '/repo',
        'summary': '',
        'highlights': [],
        'tree': {
            'name': 'a',
            'path': 'a',
            'type': 'dir',
            'loc': 40,
            'description': '',
            'readme': 'Readme para.',
            'dir_metadata': None,
            'activity': act(5, ['Add x']),
            'children': [
                {
                    'name': 'sub',
                    'path': 'a/sub',
                    'type': 'dir',
                    'loc': 10,
                    'description': '',
                    'readme': '',
                    'dir_metadata': None,
                    'activity': act(1),
                    'children': [f('a/sub/z.cc', 1)],
                },
                f('a/hot.cc', 9),
                f('a/warm.cc', 3),
                f('a/cold.cc', 0),
            ],
        },
    }


class TargetsAndAnnotateTest(unittest.TestCase):
    def test_targets_lists_dirs_then_hot_files(self):
        t = codebase_map.targets(_fake_map(), hot=2)
        paths = [x['path'] for x in t['targets']]
        # All dirs first, then the 2 hottest files; cold.cc (0 commits) never.
        self.assertEqual(paths, ['a', 'a/sub', 'a/hot.cc', 'a/warm.cc'])
        root = t['targets'][0]
        self.assertEqual(root['readme'], 'Readme para.')
        self.assertEqual(
            root['children'], ['sub/', 'hot.cc', 'warm.cc', 'cold.cc']
        )
        self.assertEqual(root['recent_subjects'], ['Add x'])

    def test_targets_excludes_untouched_files(self):
        t = codebase_map.targets(_fake_map(), hot=50)
        self.assertNotIn('a/cold.cc', [x['path'] for x in t['targets']])

    def test_annotate_merges_and_reports_unknown(self):
        data = _fake_map()
        result = codebase_map.annotate(
            data,
            {
                'summary': 'Sum.',
                'highlights': ['h1', 'h2'],
                'descriptions': {
                    'a/sub': ' Sub dir. ',
                    'a/hot.cc': 'Hot file.',
                    'a/missing.cc': 'nope',
                    'a/warm.cc': '',
                },
            },
        )
        self.assertEqual(result['applied'], 2)
        self.assertEqual(result['unknown'], ['a/missing.cc'])
        self.assertEqual(data['summary'], 'Sum.')
        self.assertEqual(data['highlights'], ['h1', 'h2'])
        sub = data['tree']['children'][0]
        self.assertEqual(sub['description'], 'Sub dir.')
        self.assertEqual(
            data['tree']['children'][1]['description'], 'Hot file.'
        )
        self.assertEqual(data['tree']['children'][2]['description'], '')

    def test_annotate_without_summary_keeps_existing(self):
        data = _fake_map()
        data['summary'] = 'Keep me.'
        codebase_map.annotate(data, {'descriptions': {}})
        self.assertEqual(data['summary'], 'Keep me.')

    def test_annotate_string_highlights_is_one_bullet(self):
        data = _fake_map()
        result = codebase_map.annotate(data, {'highlights': 'Just one.'})
        self.assertEqual(data['highlights'], ['Just one.'])
        self.assertEqual(result['warnings'], [])

    def test_annotate_skips_bad_types_and_keeps_the_rest(self):
        data = _fake_map()
        result = codebase_map.annotate(
            data,
            {
                'summary': 42,
                'highlights': ['ok', 7, ''],
                'descriptions': {
                    'a/hot.cc': {'text': 'wrong shape'},
                    'a/warm.cc': 'Warm file.',
                },
            },
        )
        self.assertEqual(result['applied'], 1)
        self.assertEqual(data['summary'], '')
        self.assertEqual(data['highlights'], ['ok'])
        self.assertEqual(data['tree']['children'][1]['description'], '')
        self.assertEqual(
            data['tree']['children'][2]['description'], 'Warm file.'
        )
        self.assertEqual(len(result['warnings']), 3)
        self.assertTrue(any('a/hot.cc' in w for w in result['warnings']))

    def test_annotate_ignores_annotations_that_are_not_an_object(self):
        data = _fake_map()
        result = codebase_map.annotate(data, ['a/hot.cc'])
        self.assertEqual(result['applied'], 0)
        self.assertEqual(len(result['warnings']), 1)
        self.assertEqual(data['tree']['children'][1]['description'], '')

    def test_annotate_descriptions_not_a_dict(self):
        data = _fake_map()
        result = codebase_map.annotate(data, {'descriptions': ['a/hot.cc']})
        self.assertEqual(result['applied'], 0)
        self.assertEqual(len(result['warnings']), 1)

    def test_default_output_slug(self):
        self.assertEqual(
            codebase_map.default_output(
                {'repo_root': '/repo', 'root': 'chrome/browser/ui/tabs'}
            ),
            os.path.join(
                '/repo',
                'out',
                'codebase_map',
                'chrome__browser__ui__tabs.html',
            ),
        )

    def test_slug_does_not_collide_on_underscores(self):
        self.assertNotEqual(
            codebase_map.map_slug('a/b_c'), codebase_map.map_slug('a_b/c')
        )

    def test_default_output_from_directory(self):
        repo = _GitRepo()
        self.addCleanup(repo.cleanup)
        repo.write('a/b/x.cc', '')
        self.assertEqual(
            codebase_map.default_output(os.path.join(repo.root, 'a', 'b')),
            os.path.join(repo.root, 'out', 'codebase_map', 'a__b.html'),
        )


def _embedded_payload(html):
    start = html.index('const DATA = ') + len('const DATA = ')
    end = html.index(';\n', start)
    return html[start:end]


class RenderTest(unittest.TestCase):
    def test_render_embeds_json_and_escapes_script_close(self):
        data = _fake_map()
        data['summary'] = 'Has </script> inside.'
        html = codebase_map.render(data)
        self.assertNotIn('</script> inside', html)
        self.assertNotIn('/*__CODEBASE_MAP_DATA__*/', html)
        # \u003c is a plain JSON escape, so the payload parses as-is.
        payload = json.loads(_embedded_payload(html))
        self.assertEqual(payload['summary'], data['summary'])

    def test_render_escapes_every_less_than(self):
        # `<!--` followed by `<script` would put the HTML tokenizer in the
        # double-escaped state, where the real </script> no longer closes.
        data = _fake_map()
        data['summary'] = 'a <!-- b <script> c'
        data['tree']['readme'] = '<b>bold</b>'
        payload = _embedded_payload(codebase_map.render(data))
        self.assertNotIn('<', payload)
        self.assertEqual(json.loads(payload)['summary'], data['summary'])

    def test_render_drops_repo_root(self):
        data = _fake_map()
        data['repo_root'] = '/usr/local/home/someone/chromium/src'
        html = codebase_map.render(data)
        self.assertNotIn('/usr/local/home/someone', html)
        self.assertNotIn('repo_root', json.loads(_embedded_payload(html)))
        # The caller's dict is untouched; default_output still needs it.
        self.assertIn('repo_root', data)

    def test_render_rejects_other_schema_versions(self):
        data = _fake_map()
        data['schema_version'] = codebase_map._SCHEMA_VERSION + 1
        with self.assertRaises(codebase_map.MapError):
            codebase_map.render(data)


class GitActivityTest(unittest.TestCase):
    ALICE = ('Alice A', 'alice@chromium.org')
    BOB = ('Bob B', 'bob@chromium.org')
    CAROL = ('Carol C', 'carol@chromium.org')

    def setUp(self):
        self.repo = _GitRepo()
        self.addCleanup(self.repo.cleanup)
        self.c1 = self.repo.commit(
            {'a/x.cc': 'x\n', 'a/sub/y.cc': 'y\n'},
            self.ALICE,
            'Add x and y\n\nBody text.\n\n'
            'Reviewed-by: Bob B <bob@chromium.org>\n'
            'Reviewed-by: Alice A <alice@chromium.org>\n'
            'Reviewed-by: Bob B <bob@chromium.org>\n',
        )
        self.c2 = self.repo.commit(
            {'a/sub/y.cc': 'y2\n', 'other/z.cc': 'z\n'},
            self.BOB,
            'Touch y and z\n\nReviewed-by: Alice A <alice@chromium.org>\n',
        )
        # Outside the mapped directory: must not show up at all.
        self.repo.commit(
            {'other/z.cc': 'z2\n'},
            self.CAROL,
            'Only z\n\nReviewed-by: Bob B <bob@chromium.org>\n',
        )
        self.per_path, self.commits, self.people = codebase_map._git_activity(
            self.repo.root, 'a', '1.year'
        )

    def test_commit_list(self):
        self.assertEqual([c['hash'] for c in self.commits], [self.c2, self.c1])
        self.assertEqual(len(self.commits[0]['hash']), 40)
        self.assertEqual(self.commits[0]['subject'], 'Touch y and z')
        self.assertEqual(self.commits[0]['author'], 'Bob B')
        # Only in-scope files are counted.
        self.assertEqual(self.commits[0]['files'], 1)
        self.assertEqual(self.commits[1]['files'], 2)

    def test_reviewers_drop_self_approval_and_duplicates(self):
        self.assertEqual(self.commits[1]['reviewers'], ['bob@chromium.org'])
        self.assertEqual(self.commits[0]['reviewers'], ['alice@chromium.org'])

    def test_directory_counts_are_distinct_commits(self):
        # c1 touched two files under a/, c2 one: two commits, not three.
        self.assertEqual(self.per_path['a']['commits'], 2)
        self.assertEqual(self.per_path['a/sub']['commits'], 2)
        self.assertEqual(self.per_path['a/sub/y.cc']['commits'], 2)
        self.assertEqual(self.per_path['a/x.cc']['commits'], 1)
        self.assertEqual(self.per_path['a']['distinct_authors'], 2)
        self.assertNotIn('other', self.per_path)
        self.assertNotIn('other/z.cc', self.per_path)

    def test_per_path_people(self):
        root = self.per_path['a']
        self.assertEqual(
            {a['email']: a['commits'] for a in root['authors']},
            {'alice@chromium.org': 1, 'bob@chromium.org': 1},
        )
        self.assertEqual(
            {r['email']: r['reviews'] for r in root['reviewers']},
            {'alice@chromium.org': 1, 'bob@chromium.org': 1},
        )
        self.assertEqual(
            self.per_path['a/x.cc']['reviewers'],
            [{'email': 'bob@chromium.org', 'reviews': 1}],
        )
        self.assertEqual(
            [r['hash'] for r in self.per_path['a/sub/y.cc']['recent']],
            [self.c2, self.c1],
        )

    def test_non_ascii_path_keeps_its_activity(self):
        # Without `-z`, git log prints such a path C-quoted and it would no
        # longer look like it is under a/.
        path = 'a/caf\u00e9.cc'
        self.repo.commit({path: 'x\n'}, self.CAROL, 'Add a file')
        per_path, _, _ = codebase_map._git_activity(
            self.repo.root, 'a', '1.year'
        )
        self.assertEqual(per_path[path]['commits'], 1)
        self.assertEqual(per_path['a']['commits'], 3)

    def test_people_totals(self):
        self.assertEqual(
            {e: (p['commits'], p['reviews']) for e, p in self.people.items()},
            {'alice@chromium.org': (1, 1), 'bob@chromium.org': (1, 1)},
        )
        self.assertEqual(self.people['bob@chromium.org']['name'], 'Bob B')


class ExtractTest(unittest.TestCase):
    def setUp(self):
        self.repo = _GitRepo()
        self.addCleanup(self.repo.cleanup)
        self.repo.commit(
            {
                'a/OWNERS': 'dir@chromium.org\n',
                'a/DIR_METADATA': 'buganizer_public { component_id: 5 }\n',
                'a/x.cc': 'one\ntwo\n',
                'a/sub/OWNERS': 'per-file *.gn=gn@chromium.org\n',
                'a/sub/BUILD.gn': 'group("x") {}\n',
                'a/blob.bin': b'\x00\x01\n\n',
            },
            ('Dev', 'dev@chromium.org'),
            'Initial\n\nReviewed-by: Rev <rev@chromium.org>\n',
        )
        # Tracked symlink to a directory: skipped, so it can't loop. Where
        # symlinks can't be created the rest of the class still runs.
        self.linked = _try_symlink(
            'sub', os.path.join(self.repo.root, 'a', 'link')
        )
        if self.linked:
            self.repo.git('add', 'a/link')
            self.repo.git(
                '-c',
                'user.name=Dev',
                '-c',
                'user.email=dev@chromium.org',
                'commit',
                '-q',
                '-m',
                'Add link',
            )
        # Untracked and generated files: not part of the map.
        self.repo.write('a/untracked.cc', 'nope\n')
        self.repo.write('a/sub/gen/out.cc', 'nope\n')
        self.data = codebase_map.extract(
            os.path.join(self.repo.root, 'a'), '1.year'
        )
        self.nodes = {
            n['path']: n for n in codebase_map._walk(self.data['tree'])
        }

    def test_extract_emits_expected_keys_and_stats(self):
        self.assertEqual(
            self.data['schema_version'], codebase_map._SCHEMA_VERSION
        )
        self.assertEqual(self.data['root'], 'a')
        self.assertEqual(self.data['repo_root'], self.repo.root)
        for key in (
            'generated_at',
            'since',
            'summary',
            'highlights',
            'stats',
            'people',
            'owners_files',
            'recent_commits',
            'tree',
        ):
            self.assertIn(key, self.data)
        self.assertEqual(
            self.data['stats'],
            {
                'files': 6,
                'dirs': 2,
                'loc': self.data['tree']['loc'],
                # The initial commit, plus the one adding the symlink.
                'commits': 2 if self.linked else 1,
                'contributors': 1,
                'reviewers': 1,
            },
        )

    def test_only_tracked_regular_files(self):
        self.assertEqual(
            sorted(self.nodes),
            [
                'a',
                'a/DIR_METADATA',
                'a/OWNERS',
                'a/blob.bin',
                'a/sub',
                'a/sub/BUILD.gn',
                'a/sub/OWNERS',
                'a/x.cc',
            ],
        )

    def test_tree_facts(self):
        self.assertEqual(self.nodes['a/x.cc']['loc'], 2)
        self.assertEqual(self.nodes['a/blob.bin']['loc'], 0)
        self.assertEqual(self.nodes['a']['children'][0]['path'], 'a/sub')
        self.assertEqual(self.nodes['a']['file_count'], 6)
        self.assertEqual(self.nodes['a']['component'], '5')
        # a/sub has no DIR_METADATA of its own and inherits a/'s.
        self.assertEqual(self.nodes['a/sub']['component'], '5')
        self.assertEqual(
            self.nodes['a/sub']['dir_metadata_source'], 'a/DIR_METADATA'
        )
        self.assertEqual(self.nodes['a/x.cc']['last_resort'], [])
        self.assertEqual(self.nodes['a/x.cc']['activity']['commits'], 1)
        self.assertEqual(
            self.nodes['a/sub/BUILD.gn']['owners'],
            ['gn@chromium.org', 'dir@chromium.org'],
        )

    def test_directory_owners_use_nearest_with_people(self):
        sub = self.nodes['a/sub']
        self.assertEqual(sub['owners'], ['dir@chromium.org'])
        self.assertEqual(sub['owners_source'], 'a/OWNERS')
        self.assertEqual(sub['owners_file']['path'], 'a/sub/OWNERS')
        self.assertEqual(sub['owners_chain'], ['a/sub/OWNERS', 'a/OWNERS'])

    def test_refuses_repo_root(self):
        with self.assertRaises(codebase_map.MapError):
            codebase_map.extract(self.repo.root, '1.year')

    def test_build_renders(self):
        html = codebase_map.render(self.data)
        self.assertNotIn(self.repo.root, html)
        self.assertIn('"a/sub/BUILD.gn"', html)


class NestedRepoTest(unittest.TestCase):
    """A directory inside a git submodule, the way Chromium pulls in v8."""

    def setUp(self):
        self.inner = _GitRepo()
        self.addCleanup(self.inner.cleanup)
        self.inner.commit(
            {'src/x.cc': 'x\n', 'src/OWNERS': 'inner@chromium.org\n'},
            ('In', 'in@chromium.org'),
            'Inner\n\nReviewed-by: Rev <rev@chromium.org>\n',
        )
        self.outer = _GitRepo()
        self.addCleanup(self.outer.cleanup)
        self.outer.commit(
            {'OWNERS': 'top@chromium.org\n'}, ('Out', 'out@chromium.org'), 'Top'
        )
        self.outer.git(
            '-c',
            'protocol.file.allow=always',
            'submodule',
            'add',
            '-q',
            self.inner.root,
            'sub',
        )
        self.outer.git(
            '-c',
            'user.name=Out',
            '-c',
            'user.email=out@chromium.org',
            'commit',
            '-q',
            '-m',
            'Add sub',
        )

    def nodes(self, data):
        return {n['path']: n for n in codebase_map._walk(data['tree'])}

    def test_submodule_root_maps_against_superproject(self):
        data = codebase_map.extract(
            os.path.join(self.outer.root, 'sub'), '1.year'
        )
        self.assertEqual(data['repo_root'], self.outer.root)
        self.assertEqual(data['root'], 'sub')
        nodes = self.nodes(data)
        self.assertEqual(
            sorted(nodes), ['sub', 'sub/src', 'sub/src/OWNERS', 'sub/src/x.cc']
        )
        # History comes from the submodule, paths from the superproject.
        self.assertEqual(nodes['sub/src/x.cc']['activity']['commits'], 1)
        self.assertEqual(data['stats']['reviewers'], 1)
        # OWNERS above the submodule apply.
        self.assertEqual(nodes['sub']['owners'], ['top@chromium.org'])
        self.assertEqual(
            nodes['sub/src/x.cc']['owners'], ['inner@chromium.org']
        )
        self.assertEqual(
            codebase_map.default_output(data),
            os.path.join(self.outer.root, 'out', 'codebase_map', 'sub.html'),
        )

    def test_submodule_subdirectory(self):
        data = codebase_map.extract(
            os.path.join(self.outer.root, 'sub', 'src'), '1.year'
        )
        self.assertEqual(data['root'], 'sub/src')
        self.assertEqual(
            sorted(self.nodes(data)),
            ['sub/src', 'sub/src/OWNERS', 'sub/src/x.cc'],
        )

    def test_superproject_skips_submodule_contents(self):
        self.outer.commit(
            {'top.cc': ''}, ('Out', 'out@chromium.org'), 'Add top.cc'
        )
        files = codebase_map._tracked_files(self.outer.root, '.')
        self.assertIn('top.cc', files)
        self.assertNotIn('sub', files)  # The gitlink (mode 160000).


class ReadmeExcerptTest(unittest.TestCase):
    def setUp(self):
        self.repo = _TempRepo()
        self.addCleanup(self.repo.cleanup)

    def test_first_paragraph_after_title(self):
        self.repo.write('a/README.md', '# Title\n\nFirst para.\n\nSecond.\n')
        self.assertEqual(
            codebase_map._readme_excerpt(os.path.join(self.repo.root, 'a')),
            'First para.',
        )

    def test_truncated_and_missing(self):
        self.repo.write('a/README', 'x' * 1000)
        excerpt = codebase_map._readme_excerpt(
            os.path.join(self.repo.root, 'a')
        )
        self.assertEqual(excerpt, 'x' * 600)
        self.assertEqual(codebase_map._readme_excerpt(self.repo.root), '')


class MainTest(unittest.TestCase):
    """The CLI, end to end, on a small repository."""

    def setUp(self):
        self.repo = _GitRepo()
        self.addCleanup(self.repo.cleanup)
        self.repo.commit(
            {'a/OWNERS': 'dir@chromium.org\n', 'a/x.cc': 'x\ny\nz\n'},
            ('Dev', 'dev@chromium.org'),
            'Initial',
        )
        self.dir = os.path.join(self.repo.root, 'a')
        self.json = os.path.join(self.repo.root, 'work', 'map.json')
        self.stderr = io.StringIO()
        patcher = mock.patch.object(sys, 'stderr', self.stderr)
        patcher.start()
        self.addCleanup(patcher.stop)

    def load(self, path):
        with open(path, encoding='utf-8') as f:
            return json.load(f)

    def test_extract_annotate_render_round_trip(self):
        codebase_map.main(
            ['extract', self.dir, '--since', '1.year', '-o', self.json]
        )
        annotations = self.repo.write(
            'work/annotations.json',
            json.dumps(
                {'summary': 'S.', 'descriptions': {'a': 'Dir.', 'a/nope': 'x'}}
            ),
        )
        codebase_map.main(['annotate', self.json, annotations])
        # `annotate` overwrites its input by default.
        data = self.load(self.json)
        self.assertEqual(data['summary'], 'S.')
        self.assertEqual(data['tree']['description'], 'Dir.')
        self.assertIn('no such node in map: a/nope', self.stderr.getvalue())

        codebase_map.main(['render', self.json])
        out = os.path.join(self.repo.root, 'out', 'codebase_map', 'a.html')
        with open(out, encoding='utf-8') as f:
            html = f.read()
        self.assertIn('Dir.', html)
        self.assertNotIn(self.repo.root, html)
        self.assertIn('serve', self.stderr.getvalue())

    def test_targets(self):
        codebase_map.main(['extract', self.dir, '-o', self.json])
        out = os.path.join(self.repo.root, 'work', 'targets.json')
        codebase_map.main(['targets', self.json, '--hot', '1', '-o', out])
        targets = self.load(out)
        self.assertEqual(
            [t['path'] for t in targets['targets']], ['a', 'a/x.cc']
        )

    def test_build_with_explicit_output(self):
        out = os.path.join(self.repo.root, 'work', 'x.html')
        codebase_map.main(['build', self.dir, '-o', out])
        self.assertTrue(os.path.isfile(out))

    def test_serve_needs_a_file_output(self):
        with self.assertRaises(SystemExit):
            codebase_map.main(['build', self.dir, '-o', '-', '--serve'])

    def test_errors_exit_with_a_message(self):
        with self.assertRaises(SystemExit) as caught:
            codebase_map.main(['extract', self.repo.root])
        self.assertEqual(
            str(caught.exception),
            'error: refusing to map the whole repository',
        )


class ServeLifecycleTest(unittest.TestCase):
    """`serve()` itself: reuse, port fallback and idle shutdown."""

    def setUp(self):
        self.repo = _TempRepo()
        self.addCleanup(self.repo.cleanup)
        self.html = self.repo.write('maps/one.html', '<p>map</p>')
        # Never touch the real preferred port from a test. With port 0 there
        # is no server to reuse and the bind picks an ephemeral port, so no
        # port is chosen in advance for another process to take.
        for patcher in (
            mock.patch.object(codebase_map, '_PREFERRED_PORT', 0),
            mock.patch.object(sys, 'stderr', io.StringIO()),
        ):
            patcher.start()
            self.addCleanup(patcher.stop)

    def start(self, idle_timeout_min=0, **kwargs):
        """Runs serve() in a thread; returns (thread, port, result dict).

        The server has no idle timeout unless the test asks for one. It is
        stopped through `self.stop` when the test ends, and the test fails
        if it does not stop.
        """
        ready = queue.Queue()
        result = {}
        self.stop = threading.Event()

        def run():
            result['status'] = codebase_map.serve(
                self.html,
                open_browser=False,
                idle_timeout_min=idle_timeout_min,
                on_ready=ready.put,
                stop_event=self.stop,
                **kwargs,
            )

        thread = threading.Thread(target=run, daemon=True)
        thread.start()

        def stop_and_join():
            self.stop.set()
            thread.join(timeout=15)
            self.assertFalse(thread.is_alive(), 'serve() did not stop')

        self.addCleanup(stop_and_join)
        return thread, ready.get(timeout=10), result

    def get(self, port, host, address='127.0.0.1'):
        conn = http.client.HTTPConnection(address, port, timeout=5)
        try:
            conn.request('GET', '/one.html', headers={'Host': f'{host}:{port}'})
            return conn.getresponse().status
        finally:
            conn.close()

    def assert_not_listening(self, port):
        with self.assertRaises(OSError):
            socket.create_connection(('127.0.0.1', port), timeout=1).close()

    def test_proxy_mode_accepts_machine_name_and_allowed_hosts(self):
        with mock.patch.object(
            codebase_map, '_machine_names', return_value={'box.example.com'}
        ):
            _, port, _ = self.start(
                proxy=True, allow_hosts=['Proxy.Example', '.corp.example']
            )
        self.assertEqual(self.get(port, 'localhost'), 200)
        self.assertEqual(self.get(port, 'box.example.com'), 200)
        self.assertEqual(self.get(port, 'proxy.example'), 200)
        self.assertEqual(self.get(port, 'a.corp.example'), 200)
        self.assertEqual(self.get(port, 'evil.example'), 403)
        self.assertEqual(self.get(port, 'corp.example.evil.example'), 403)
        # A leading-dot entry covers names under the domain, not the bare
        # suffix itself.
        self.assertEqual(self.get(port, '.corp.example'), 403)
        self.assertIn('--allow-host evil.example', sys.stderr.getvalue())

    def test_proxy_mode_trusts_no_domain_by_default(self):
        with mock.patch.object(
            codebase_map, '_machine_names', return_value={'box.example.com'}
        ):
            _, port, _ = self.start(proxy=True)
        self.assertEqual(self.get(port, 'box.example.com'), 200)
        self.assertEqual(self.get(port, 'b1.proxy.example.com'), 403)

    def test_proxy_mode_without_a_machine_name(self):
        # A host named `localhost` has no name a proxy could use; serve
        # still starts and offers the localhost URL.
        with mock.patch.object(
            codebase_map, '_machine_names', return_value=set()
        ):
            _, port, _ = self.start(proxy=True)
        self.assertEqual(self.get(port, 'localhost'), 200)
        self.assertIn(
            f'http://localhost:{port}/one.html', sys.stderr.getvalue()
        )

    def test_proxy_mode_binds_every_interface(self):
        for proxy, expected in (
            (True, ('::', '0.0.0.0')),
            (False, ('127.0.0.1',)),
        ):
            httpd = codebase_map._make_server(
                0, codebase_map._MapHandler, proxy
            )
            self.addCleanup(httpd.server_close)
            self.assertIn(httpd.server_address[0], expected)

    def test_reuse_requires_the_same_mode(self):
        _, port, _ = self.start()
        directory = os.path.dirname(self.html)
        self.assertTrue(codebase_map._running_server_for(directory, port))
        self.assertFalse(
            codebase_map._running_server_for(directory, port, proxy=True)
        )

    def test_stop_event_stops_the_server(self):
        thread, port, result = self.start()
        self.assertEqual(self.get(port, 'localhost'), 200)
        self.stop.set()
        thread.join(timeout=15)
        self.assertFalse(thread.is_alive())
        self.assertEqual(result['status'], 'stopped')
        self.assert_not_listening(port)

    def test_idle_timeout_stops_the_server(self):
        # The one test that waits on the clock: about a second of idleness.
        thread, port, result = self.start(idle_timeout_min=0.02)
        thread.join(timeout=15)
        self.assertFalse(thread.is_alive())
        self.assertEqual(result['status'], 'stopped')
        self.assertIn('Idle timeout reached', sys.stderr.getvalue())
        self.assert_not_listening(port)

    def test_second_serve_reuses_the_running_server(self):
        _, port, _ = self.start()
        # The running server holds `port`; make that the preferred one.
        with mock.patch.object(codebase_map, '_PREFERRED_PORT', port):
            status = codebase_map.serve(self.html, open_browser=False)
        self.assertEqual(status, 'reused')

    def test_falls_back_when_preferred_port_is_busy(self):
        with socket.socket() as busy:
            busy.bind(('127.0.0.1', 0))
            busy.listen()
            preferred = busy.getsockname()[1]
            with mock.patch.object(codebase_map, '_PREFERRED_PORT', preferred):
                _, port, _ = self.start()
            self.assertNotEqual(port, preferred)
            self.assertEqual(self.get(port, 'localhost'), 200)


class ServeTest(unittest.TestCase):
    def setUp(self):
        self.repo = _TempRepo()
        self.addCleanup(self.repo.cleanup)
        self.repo.write('maps/one.html', '<p>map</p>')
        self.repo.write('maps/notes.txt', 'secret')
        self.repo.write('outside.html', 'outside')
        self.directory = os.path.join(self.repo.root, 'maps')
        handler = type(
            '_TestHandler',
            (codebase_map._MapHandler,),
            {'directory': self.directory},
        )
        self.httpd = codebase_map.http.server.ThreadingHTTPServer(
            ('127.0.0.1', 0), handler
        )
        self.httpd.last_request = 0
        self.port = self.httpd.server_address[1]
        thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(self.httpd.server_close)
        self.addCleanup(self.httpd.shutdown)

    def get(self, path, host='localhost', method='GET'):
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=5)
        self.addCleanup(conn.close)
        conn.request(method, path, headers={'Host': f'{host}:{self.port}'})
        resp = conn.getresponse()
        return resp.status, resp.read().decode()

    def test_serves_html_maps(self):
        self.assertEqual(self.get('/one.html'), (200, '<p>map</p>'))
        self.assertEqual(self.get('/one.html', host='127.0.0.1')[0], 200)
        self.assertEqual(self.get('/one.html', host='[::1]')[0], 200)

    def test_head_has_no_body(self):
        self.assertEqual(self.get('/one.html', method='HEAD'), (200, ''))

    def test_symlinked_html_is_not_served(self):
        if not _try_symlink(
            os.path.join(self.repo.root, 'outside.html'),
            os.path.join(self.directory, 'link.html'),
        ):
            self.skipTest('symlinks are not available here')
        self.assertEqual(self.get('/link.html')[0], 404)

    def test_serves_nothing_else(self):
        self.assertEqual(self.get('/notes.txt')[0], 404)
        self.assertEqual(self.get('/')[0], 404)
        self.assertEqual(self.get('/../outside.html')[0], 404)
        self.assertEqual(self.get('/%2e%2e/outside.html')[0], 404)
        self.assertEqual(self.get('/missing.html')[0], 404)

    def test_rejects_non_local_host(self):
        self.assertEqual(self.get('/one.html', host='evil.example')[0], 403)
        self.assertEqual(
            self.get('/one.html', host='localhost.evil.example')[0], 403
        )
        # The machine's own name is only accepted in proxy mode.
        name = sorted(codebase_map._machine_names() or {'x.example'})[0]
        self.assertEqual(self.get('/one.html', host=name)[0], 403)

    def test_refused_requests_do_not_count_as_activity(self):
        # Otherwise a page that keeps probing would keep the server alive.
        self.get('/one.html', host='evil.example')
        self.assertEqual(self.httpd.last_request, 0)
        self.get('/one.html')
        self.assertGreater(self.httpd.last_request, 0)

    def test_probe_and_reuse(self):
        status, body = self.get(codebase_map._PROBE_PATH)
        self.assertEqual(status, 200)
        self.assertEqual(
            json.loads(body),
            {
                'directory_id': codebase_map._directory_id(self.directory),
                'proxy': False,
            },
        )
        # The probe is readable by anyone who can reach the port.
        self.assertNotIn(self.directory, body)
        self.assertTrue(
            codebase_map._running_server_for(self.directory, self.port)
        )
        self.assertFalse(
            codebase_map._running_server_for(self.repo.root, self.port)
        )


def _find_node():
    """Chromium's checked-in Node for this platform, else one on PATH.

    The choice goes by platform, as in third_party/node/node.py, not by which
    binary exists: DEPS downloads the Linux one on every host, so on Windows
    and Mac it is present but cannot run.
    """
    src = os.path.normpath(os.path.join(_SCRIPT_DIR, *[os.pardir] * 4))
    if platform.machine() == 'arm64':
        mac = ('mac_arm64', 'node-darwin-arm64', 'bin', 'node')
    else:
        mac = ('mac', 'node-darwin-x64', 'bin', 'node')
    rel = {
        'Darwin': mac,
        'Linux': ('linux', 'node-linux-x64', 'bin', 'node'),
        'Windows': ('win', 'node.exe'),
    }.get(platform.system())
    if rel:
        path = os.path.join(src, 'third_party', 'node', *rel)
        if os.path.isfile(path):
            return path
    return shutil.which('node')


class FindNodeTest(unittest.TestCase):
    def find(self, system, machine='x86_64'):
        """_find_node() on a host where every checked-in binary exists."""
        with (
            mock.patch.object(platform, 'system', return_value=system),
            mock.patch.object(platform, 'machine', return_value=machine),
            mock.patch.object(os.path, 'isfile', return_value=True),
        ):
            found = _find_node()
        return found.replace(os.sep, '/').split('third_party/node/')[1]

    def test_picks_the_binary_for_the_host_platform(self):
        # Regression: the Linux binary, present on every host, was tried
        # first and failed to start on Windows.
        self.assertEqual(self.find('Windows'), 'win/node.exe')
        self.assertEqual(self.find('Linux'), 'linux/node-linux-x64/bin/node')
        self.assertEqual(self.find('Darwin'), 'mac/node-darwin-x64/bin/node')
        self.assertEqual(
            self.find('Darwin', 'arm64'),
            'mac_arm64/node-darwin-arm64/bin/node',
        )

    def test_falls_back_to_path_on_other_platforms(self):
        with (
            mock.patch.object(platform, 'system', return_value='FreeBSD'),
            mock.patch.object(shutil, 'which', return_value='/usr/bin/node'),
        ):
            self.assertEqual(_find_node(), '/usr/bin/node')


def _run_node(test, node, source, *args):
    """Runs `source` as a Node script; returns the JSON it prints.

    Fails `test` with Node's stderr when the script throws.
    """
    with tempfile.NamedTemporaryFile(
        'w', suffix='.js', delete=False, encoding='utf-8'
    ) as f:
        f.write(source)
    test.addCleanup(os.unlink, f.name)
    # Node writes UTF-8, and the viewer's text has non-ASCII characters.
    proc = subprocess.run(
        [node, f.name, *args],
        capture_output=True,
        encoding='utf-8',
        check=False,
    )
    if proc.returncode:
        test.fail(f'node exited with {proc.returncode}:\n{proc.stderr}')
    return json.loads(proc.stdout)


class ViewerLibTest(unittest.TestCase):
    """Runs the template's `map-lib` script block under Node."""

    @classmethod
    def setUpClass(cls):
        cls.node = _find_node()
        if not cls.node:
            raise unittest.SkipTest('node not found')
        with open(codebase_map._TEMPLATE, encoding='utf-8') as f:
            template = f.read()
        m = re.search(r'<script id="map-lib">(.*?)</script>', template, re.S)
        cls.lib = m.group(1)

    def run_js(self, expr, data):
        """Evaluates `expr` with MapLib and `input` in scope; returns JSON."""
        source = (
            f'{self.lib}\n'
            'const input = JSON.parse(process.argv[2]);\n'
            f'console.log(JSON.stringify({expr}));\n'
        )
        return _run_node(self, self.node, source, json.dumps(data))

    PLAN = """(() => {
      const inv = e => input.involvement[e] || 0;
      const r = MapLib.planReviewers(
        input.files, new Map(input.pinned), inv);
      return {
        chosen: r.chosen,
        free: r.free.map(f => f.path),
        groups: r.groups.map(g => ({
          key: g.key, owners: g.owners, n: g.files.length,
        })),
      };
    })()"""

    FILES = [
        {'path': 'a', 'owners': ['x', 'y']},
        {'path': 'b', 'owners': ['y', 'z']},
        {'path': 'c', 'owners': ['z']},
        {'path': 'd', 'owners': ['*']},
        {'path': 'e', 'owners': []},
    ]

    def test_greedy_cover_breaks_ties_by_involvement(self):
        r = self.run_js(
            self.PLAN,
            {'files': self.FILES, 'pinned': [], 'involvement': {'y': 1}},
        )
        # y and z both cover two files; y is more involved, so it goes first.
        self.assertEqual(
            r['chosen'],
            [
                {'email': 'y', 'covers': ['a', 'b'], 'pinned': False},
                {'email': 'z', 'covers': ['c'], 'pinned': False},
            ],
        )
        self.assertEqual(r['free'], ['d', 'e'])

    def test_pinned_owner_is_seeded_first(self):
        r = self.run_js(
            self.PLAN,
            {
                'files': self.FILES,
                'pinned': [['x,y', 'x']],
                'involvement': {'y': 1},
            },
        )
        self.assertEqual(
            r['chosen'],
            [
                {'email': 'x', 'covers': ['a'], 'pinned': True},
                {'email': 'z', 'covers': ['b', 'c'], 'pinned': False},
            ],
        )

    def test_groups_by_owner_set(self):
        files = self.FILES[:3] + [{'path': 'a2', 'owners': ['y', 'x']}]
        r = self.run_js(
            self.PLAN,
            {'files': files, 'pinned': [], 'involvement': {'x': 3, 'y': 1}},
        )
        self.assertEqual(
            r['groups'],
            [
                {'key': 'x,y', 'owners': ['x', 'y'], 'n': 2},
                {'key': 'y,z', 'owners': ['y', 'z'], 'n': 1},
                {'key': 'z', 'owners': ['z'], 'n': 1},
            ],
        )

    def test_visible_paths(self):
        tree = {
            'name': 'a',
            'path': 'a',
            'owners': [],
            'children': [
                {
                    'name': 'sub',
                    'path': 'a/sub',
                    'owners': [],
                    'children': [
                        {'name': 'y.cc', 'path': 'a/sub/y.cc', 'owners': []},
                    ],
                },
                {
                    'name': 'x.cc',
                    'path': 'a/x.cc',
                    'owners': ['bob@x.org'],
                    'description': 'Entry point.',
                },
            ],
        }
        expr = '[...MapLib.visiblePaths(input, %s)].sort()'
        self.assertEqual(
            self.run_js(expr % "'Y.CC'", tree), ['a', 'a/sub', 'a/sub/y.cc']
        )
        self.assertEqual(self.run_js(expr % "'bob'", tree), ['a', 'a/x.cc'])
        self.assertEqual(self.run_js(expr % "'entry'", tree), ['a', 'a/x.cc'])
        self.assertEqual(self.run_js(expr % "'nothing'", tree), [])

    def test_last_resort_owner_only_when_nobody_else_can(self):
        files = [
            {'path': 'a', 'owners': ['lr', 'x'], 'last_resort': ['lr']},
            {'path': 'b', 'owners': ['lr', 'y'], 'last_resort': ['lr']},
            {'path': 'c', 'owners': ['lr'], 'last_resort': ['lr']},
        ]
        r = self.run_js(
            self.PLAN,
            {
                'files': files,
                'pinned': [],
                'involvement': {'lr': 99},
            },
        )
        # lr covers all three and is the most involved, but is only taken
        # for the file nobody else owns.
        self.assertEqual([c['email'] for c in r['chosen']], ['x', 'y', 'lr'])
        self.assertEqual(r['chosen'][2]['covers'], ['c'])
        # And it is listed last in "pick one of".
        self.assertEqual(r['groups'][0]['owners'], ['x', 'lr'])

    def test_last_resort_is_per_file(self):
        # boss is last-resort for p (tagged in a parent OWNERS) but a regular
        # owner of q, so asking boss for both beats adding dev.
        files = [
            {'path': 'p', 'owners': ['boss', 'dev'], 'last_resort': ['boss']},
            {'path': 'q', 'owners': ['boss']},
        ]
        r = self.run_js(
            self.PLAN,
            {'files': files, 'pinned': [], 'involvement': {'boss': 1}},
        )
        self.assertEqual(
            r['chosen'],
            [{'email': 'boss', 'covers': ['p', 'q'], 'pinned': False}],
        )
        # Without q, boss is only a last resort and dev is asked instead.
        r = self.run_js(
            self.PLAN,
            {'files': files[:1], 'pinned': [], 'involvement': {'boss': 1}},
        )
        self.assertEqual([c['email'] for c in r['chosen']], ['dev'])

    def test_owner_status_respects_per_file_noparent(self):
        # End to end: the extracted chain, read by the viewer's ownerStatus.
        # Directory and parent owners must not be badged as able to approve
        # a file under `per-file ... set noparent`.
        repo = _GitRepo()
        self.addCleanup(repo.cleanup)
        repo.commit(
            {
                'OWNERS': 'root@chromium.org\n',
                'a/OWNERS': 'dir@chromium.org\n'
                'per-file *.mojom=set noparent\n'
                'per-file *.mojom=sec@chromium.org\n',
                'a/x.mojom': 'module x;\n',
                'a/x.cc': 'int x;\n',
                'a/sub/OWNERS': 'leaf@chromium.org\n',
                'a/sub/y.mojom': 'module y;\n',
            },
            ('Dev', 'dev@chromium.org'),
            'Initial',
        )
        data = codebase_map.extract(os.path.join(repo.root, 'a'), '1.year')
        nodes = {n['path']: n for n in codebase_map._walk(data['tree'])}
        expr = """['sec', 'dir', 'root'].map(who => MapLib.ownerStatus(
            input.node, who + '@chromium.org', input.files))"""

        def status(path):
            return self.run_js(
                expr, {'node': nodes[path], 'files': data['owners_files']}
            )

        self.assertEqual(status('a/x.mojom'), [{'kind': 'direct'}, None, None])
        self.assertEqual(
            status('a/x.cc'),
            [None, {'kind': 'direct'}, {'kind': 'inherited', 'via': 'OWNERS'}],
        )
        # The rule is above the nearest owners (a/sub/OWNERS): its owner can
        # approve through it, a/'s and the root's directory owners cannot.
        self.assertEqual(
            status('a/sub/y.mojom'),
            [{'kind': 'inherited', 'via': 'a/OWNERS'}, None, None],
        )

    def test_rule_flags_unresolved_include(self):
        rule = {
            'source': 'a/OWNERS',
            'pattern': '*.gn',
            'noparent': False,
            'unresolved': ['file://gone/GN_OWNERS'],
        }
        files = [{'path': 'a/BUILD.gn', 'owner_rules': [rule]}]
        out = self.run_js('MapLib.ruleFlags(input, "a/OWNERS")', files)
        self.assertEqual(
            out,
            [
                {
                    'file': 'a/BUILD.gn',
                    'text': 'a/OWNERS includes file://gone/GN_OWNERS, which '
                    'could not be read — its owners are missing from the '
                    'suggestion',
                    'noparent': False,
                },
                {
                    'file': 'a/BUILD.gn',
                    'text': 'matches per-file *.gn (a/OWNERS)',
                    'noparent': False,
                },
            ],
        )

    def test_resolve_path(self):
        cases = [
            ('base_export.h', 'base/base_export.h'),
            ('base/x.h', 'base/x.h'),
            ('base', 'base'),
            ('', 'base'),
            ('sub/y.cc', 'base/sub/y.cc'),
        ]
        out = self.run_js(
            'input.map(p => MapLib.resolvePath("base", p))',
            [c[0] for c in cases],
        )
        self.assertEqual(out, [c[1] for c in cases])

    def test_owner_status(self):
        expr = """['a', 'b', 'c'].map(e => MapLib.ownerStatus(
            input.node, e, input.files))"""
        out = self.run_js(
            expr,
            {
                'node': {
                    'owners': ['a'],
                    'owners_chain': ['x/OWNERS', 'OWNERS'],
                },
                'files': {'x/OWNERS': [], 'OWNERS': ['b']},
            },
        )
        self.assertEqual(
            out,
            [{'kind': 'direct'}, {'kind': 'inherited', 'via': 'OWNERS'}, None],
        )

    def test_rule_flags(self):
        per_file = {'source': 'a/OWNERS', 'pattern': '*.gn', 'noparent': True}
        root_noparent = {
            'source': 'a/OWNERS',
            'pattern': None,
            'noparent': True,
        }
        sub_noparent = {
            'source': 'a/s/OWNERS',
            'pattern': None,
            'noparent': True,
        }
        files = [
            {'path': 'a/BUILD.gn', 'owner_rules': [per_file, root_noparent]},
            {'path': 'a/BUILD.gn', 'owner_rules': [per_file]},  # Duplicate.
            {'path': 'a/s/x.cc', 'owner_rules': [sub_noparent]},
            {'path': 'a/y.cc'},
        ]
        out = self.run_js('MapLib.ruleFlags(input, "a/OWNERS")', files)
        self.assertEqual(
            out,
            [
                {
                    'file': 'a/BUILD.gn',
                    'text': 'matches per-file *.gn (a/OWNERS) — set noparent, '
                    'only these owners can approve',
                    'noparent': True,
                },
                {
                    'file': 'a/s/x.cc',
                    'text': 'a/s/OWNERS has set noparent',
                    'noparent': True,
                },
            ],
        )

    def test_picked_counts(self):
        expr = """(() => {
          const parents = input.parents;
          const counts = MapLib.pickedCounts(
            new Set(input.picked), p => parents[p]);
          return Object.fromEntries([...counts].sort());
        })()"""
        out = self.run_js(
            expr,
            {
                'parents': {
                    'a/s/x': 'a/s',
                    'a/s/y': 'a/s',
                    'a/z': 'a',
                    'a/s': 'a',
                },
                'picked': ['a/s/x', 'a/s/y', 'a/z'],
            },
        )
        self.assertEqual(out, {'a': 3, 'a/s': 2})


class ViewerDomTest(unittest.TestCase):
    """Drives the viewer's page script against a small fake DOM under Node.

    The page is assembled from the template's own markup and script blocks
    plus `fake_dom_for_test.js`, and the data comes from `extract` on a real
    repository. Scenarios act through `page` (defined in that file) the way a
    user would: clicking rows, ticking checkboxes, typing in the filter.
    """

    @classmethod
    def setUpClass(cls):
        cls.node = _find_node()
        if not cls.node:
            raise unittest.SkipTest('node not found')
        with open(codebase_map._TEMPLATE, encoding='utf-8') as f:
            template = f.read()
        cls.body = re.search(r'<body>(.*?)<script>', template, re.S).group(1)
        scripts = re.findall(r'<script[^>]*>(.*?)</script>', template, re.S)
        # The last two blocks are the code under test: `map-lib`, then the
        # page script. Before them come the theme snippet and the DATA slot.
        cls.scripts = scripts[-2:]
        fake_dom = os.path.join(_SCRIPT_DIR, 'fake_dom_for_test.js')
        with open(fake_dom, encoding='utf-8') as f:
            cls.fake_dom = f.read()

        repo = _GitRepo()
        cls.addClassCleanup(repo.cleanup)
        repo.commit(
            {
                'a/OWNERS': 'dir@chromium.org\n',
                'a/DIR_METADATA': 'buganizer_public { component_id: 5 }\n',
                'a/x.cc': 'int x;\n',
                'a/y.h': 'int y;\n',
                'a/other/v.cc': 'int v;\n',
                'a/sub/OWNERS': 'sub@chromium.org\n',
                'a/sub/z.cc': 'int z;\n',
                'a/sub/deep/w.cc': 'int w;\n',
            },
            ('Dev', 'dev@chromium.org'),
            'Initial\n\nReviewed-by: Rev <rev@chromium.org>\n',
        )
        cls.data = codebase_map.extract(os.path.join(repo.root, 'a'), '1.year')
        codebase_map.annotate(
            cls.data,
            {'descriptions': {'a/x.cc': '<img src=x onerror=alert(1)> entry'}},
        )

    def run_page(self, scenario, fragment=''):
        """Loads the page at `#fragment`, runs `scenario`, returns its value.

        `scenario` is the body of a JS function with `page` in scope.
        """
        source = '\n'.join(
            [
                self.fake_dom,
                f'const page = installFakeDom({json.dumps(self.body)}, '
                f'{json.dumps("#" + fragment)});',
                f'const DATA = {json.dumps(self.data)};',
                *self.scripts,
                f'console.log(JSON.stringify((() => {{ {scenario} }})()));',
            ]
        )
        return _run_node(self, self.node, source)

    def test_directory_rows_are_built_on_first_open(self):
        out = self.run_page("""
            const before = page.builtRows();
            page.row('a/sub/deep').click();
            return {before, after: page.builtRows()};
        """)
        self.assertIn('a/sub/deep', out['before'])
        self.assertNotIn('a/sub/deep/w.cc', out['before'])
        self.assertIn('a/sub/deep/w.cc', out['after'])

    def test_filter_shows_matches_and_their_ancestors(self):
        out = self.run_page("""
            page.filter('W.CC');
            return {
              rows: page.visibleRows(),
              marked: page.$$('#tree mark').map(m => m.textContent),
            };
        """)
        self.assertEqual(
            out['rows'], ['a', 'a/sub', 'a/sub/deep', 'a/sub/deep/w.cc']
        )
        self.assertEqual(out['marked'], ['w.cc'])

    def test_filter_applies_to_a_directory_collapsed_before_typing(self):
        # Regression: a/sub was opened (its rows exist) and then collapsed.
        # Filtering reopened it without filtering those rows, so every
        # child showed.
        rows = self.run_page("""
            page.$('#collapse-all').click();
            page.filter('z.cc');
            return page.visibleRows();
        """)
        self.assertEqual(rows, ['a', 'a/sub', 'a/sub/z.cc'])

    def test_narrowing_the_filter_hides_earlier_matches(self):
        rows = self.run_page("""
            page.filter('.cc');
            page.filter('v.cc');
            return page.visibleRows();
        """)
        self.assertEqual(rows, ['a', 'a/other', 'a/other/v.cc'])

    def test_escape_clears_the_filter(self):
        out = self.run_page("""
            page.filter('z.cc');
            page.press('Escape');
            return {
              rows: page.visibleRows(),
              value: page.$('#filter').value,
              marked: page.$$('#tree mark').length,
            };
        """)
        self.assertEqual(out['value'], '')
        self.assertEqual(out['marked'], 0)
        for path in ('a/x.cc', 'a/other', 'a/sub/z.cc', 'a/sub/OWNERS'):
            self.assertIn(path, out['rows'])

    def test_expand_all_and_collapse_all(self):
        out = self.run_page("""
            page.$('#expand-all').click();
            const expanded = page.visibleRows();
            page.$('#collapse-all').click();
            return {expanded, collapsed: page.visibleRows()};
        """)
        every_node = sorted(
            n['path'] for n in codebase_map._walk(self.data['tree'])
        )
        self.assertEqual(sorted(out['expanded']), every_node)
        # The root stays open; its subdirectories close.
        self.assertEqual(
            sorted(out['collapsed']),
            [
                'a',
                'a/DIR_METADATA',
                'a/OWNERS',
                'a/other',
                'a/sub',
                'a/x.cc',
                'a/y.h',
            ],
        )

    def test_picking_a_directory_picks_its_files(self):
        out = self.run_page("""
            page.row('a/sub').querySelector('.pick').click();
            return {
              checked: page.boxes('checked'),
              partial: page.boxes('indeterminate'),
              count: page.text('#pick-count'),
              reviewers: page.text('#pane-reviewers'),
            };
        """)
        self.assertEqual(
            out['checked'],
            ['a/sub', 'a/sub/deep', 'a/sub/z.cc', 'a/sub/OWNERS'],
        )
        self.assertEqual(out['partial'], ['a'])
        # a/sub/OWNERS, a/sub/z.cc and the not yet built a/sub/deep/w.cc.
        self.assertEqual(out['count'], '3')
        self.assertIn('R=sub@chromium.org', out['reviewers'])

    def test_unpicking_clears_the_plan(self):
        out = self.run_page("""
            const box = page.row('a/sub').querySelector('.pick');
            box.click();
            box.click();
            return {
              checked: page.boxes('checked'),
              partial: page.boxes('indeterminate'),
              reviewers: page.text('#pane-reviewers'),
            };
        """)
        self.assertEqual(out['checked'], [])
        self.assertEqual(out['partial'], [])
        self.assertNotIn('R=', out['reviewers'])

    def test_deep_link_selects_picks_and_opens_a_tab(self):
        out = self.run_page(
            """
            return {
              selected: page.$$('#tree .row.selected').map(r => r.dataset.path),
              tab: page.activeTab(),
              checked: page.boxes('checked'),
              reviewers: page.text('#pane-reviewers'),
            };
            """,
            fragment='sel=sub/z.cc&pick=x.cc,sub/z.cc&tab=reviewers',
        )
        self.assertEqual(out['selected'], ['a/sub/z.cc'])
        self.assertEqual(out['tab'], 'reviewers')
        self.assertEqual(out['checked'], ['a/sub/z.cc', 'a/x.cc'])
        self.assertIn('R=dir@chromium.org, sub@chromium.org', out['reviewers'])

    def test_deep_link_with_unknown_tab_keeps_the_details_tab(self):
        out = self.run_page(
            """
            return {
              tab: page.activeTab(),
              panes: page.$$('.pane.active').map(p => p.id),
            };
            """,
            fragment='tab=bogus',
        )
        self.assertEqual(out, {'tab': 'detail', 'panes': ['pane-detail']})

    def test_details_name_the_source_of_inherited_metadata(self):
        out = self.run_page("""
            page.row('a/sub').click();
            const sub = page.text('#pane-detail');
            page.row('a').click();
            return {sub, root: page.text('#pane-detail')};
        """)
        self.assertIn('Metadata froma/DIR_METADATA', out['sub'])
        self.assertIn('sub@chromium.org', out['sub'])
        # a/ has its own DIR_METADATA, so there is nothing to attribute.
        self.assertNotIn('Metadata from', out['root'])

    def test_descriptions_are_text_not_markup(self):
        out = self.run_page("""
            return {
              images: page.$$('img').length,
              label: page.row('a/x.cc').textContent,
            };
        """)
        self.assertEqual(out['images'], 0)
        self.assertIn('<img src=x onerror=alert(1)> entry', out['label'])


if __name__ == '__main__':
    unittest.main()
