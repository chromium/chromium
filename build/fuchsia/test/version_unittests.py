#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing version.py."""

import unittest
import unittest.mock as mock

from typing import Callable, List

import version


def _test(args: List[str], f: Callable) -> None:
    with mock.patch('sys.argv', args):
        # pylint: disable=protected-access
        version._GIT_ARGS = version._load_git_args()
        f()


_TRY_ARGS = [
    'version.py', '--git-revision=e98127af84bf5b33a6e657c90dfd3f3a731eb28c',
    '--gerrit-issue=5009604', '--gerrit-patchset=16',
    '--buildbucket-id=8756180599882888289'
]

_0_TRY_ARGS = [
    'version.py', '--git-revision=e98127af84bf5b33a6e657c90dfd3f3a731eb28c',
    '--gerrit-issue=0', '--gerrit-patchset=16',
    '--buildbucket-id=8756180599882888289'
]

_CI_ARGS = [
    'version.py', '--git-revision=e98127af84bf5b33a6e657c90dfd3f3a731eb28c'
]


# pylint: disable=missing-function-docstring
class VersionTest(unittest.TestCase):
    """Tests of version.py."""

    def test_is_try_build(self) -> None:
        _test(_TRY_ARGS, lambda: self.assertTrue(version.is_try_build()))

    def test_is_not_try_build(self) -> None:
        _test(_CI_ARGS, lambda: self.assertFalse(version.is_try_build()))

    def test_try_git_revision(self) -> None:
        _test(
            _TRY_ARGS, lambda: self.assertEqual(
                version.git_revision(),
                'e98127af84bf5b33a6e657c90dfd3f3a731eb28c/5009604/16'))

    def test_ci_git_revision(self) -> None:
        _test(
            _CI_ARGS, lambda: self.assertEqual(
                version.git_revision(),
                'e98127af84bf5b33a6e657c90dfd3f3a731eb28c'))

    def test_is_try_build_0(self) -> None:
        _test(_0_TRY_ARGS, lambda: self.assertTrue(version.is_try_build()))

    def test_try_git_revision_0(self) -> None:
        _test(
            _0_TRY_ARGS, lambda: self.assertEqual(
                version.git_revision(),
                'e98127af84bf5b33a6e657c90dfd3f3a731eb28c/0/16'))
