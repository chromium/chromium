# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import unittest
import tempfile
import textwrap

from compiler import _maybe_cache
from compiler import _parse_modulemap
from compiler import Compiler
from modularize import SOURCE_ROOT


class TestableCompiler(Compiler):
  n = 1

  @_maybe_cache
  def cached_n(self):
    return self.n

  # Override these to prevent it from invoking GN
  def _get_os(self):
    return 'linux'

  def _get_cpu(self):
    return 'x64'


class CompilerTest(unittest.TestCase):

  def setUp(self):
    super().setUp()
    self.compiler1 = TestableCompiler(
        gn_out=pathlib.Path('/tmp/compiler1'),
        source_root=SOURCE_ROOT,
        error_dir=None,
        use_cache=True,
    )
    self.compiler1_uncached = TestableCompiler(
        gn_out=pathlib.Path('/tmp/compiler1'),
        source_root=SOURCE_ROOT,
        error_dir=None,
        use_cache=False,
    )
    self.compiler2 = TestableCompiler(
        gn_out=pathlib.Path('/tmp/compiler2'),
        source_root=SOURCE_ROOT,
        error_dir=None,
        use_cache=True,
    )

  def test_maybe_cache(self):
    # Uncached compilers should write to the cache, but not read from it.
    self.compiler1.n = 2
    self.assertEqual(self.compiler1_uncached.cached_n(), 1)
    self.assertEqual(self.compiler1.cached_n(), 1)
    self.compiler1_uncached.n = 3
    self.assertEqual(self.compiler1_uncached.cached_n(), 3)
    self.assertEqual(self.compiler1.cached_n(), 3)

    # This one should be unrelated since it has a different gn_out dir.
    self.compiler2.n = 4
    self.assertEqual(self.compiler2.cached_n(), 4)

  def test_parse_modulemap(self):
    self.assertDictEqual(
        # It's a defaultdict
        _parse_modulemap(SOURCE_ROOT /
                         'build/modules/modularize/testdata/module.modulemap'),
        {
            'first': [
                ('first.h', False),
                ('../first_textual.h', True),
            ],
            'second': [
                ('second.h', False),
                ('../second_textual.h', True),
            ]
        },
    )

  def test_parse_depfile(self):
    self.assertListEqual(
        self.compiler1._parse_depfile(
            textwrap.dedent("""\
          /dev/null: /tmp/main.cc \\
            ../up.cc \\
            path/to/relative.cc \\
            /path/to/absolute.cc \\
            path\\ with\\ spaces.cc \\
          """)),
        [
            pathlib.Path('/tmp/up.cc'),
            pathlib.Path('/tmp/compiler1/path/to/relative.cc'),
            pathlib.Path('/path/to/absolute.cc'),
            pathlib.Path('/tmp/compiler1/path with spaces.cc'),
        ],
    )


if __name__ == '__main__':
  unittest.main()
