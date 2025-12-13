# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import unittest
import textwrap

from compiler import _maybe_cache
from compiler import Compiler
from modularize import SOURCE_ROOT
from platforms import Cpu
from platforms import Os


class TestableCompiler(Compiler):
  n = 1

  @_maybe_cache
  def cached_n(self):
    return self.n


class CompilerTest(unittest.TestCase):

  def setUp(self):
    super().setUp()
    self.compiler1 = TestableCompiler(gn_out=pathlib.Path('/tmp/compiler1'),
                                      source_root=SOURCE_ROOT,
                                      error_dir=None,
                                      use_cache=True,
                                      os=Os.Linux,
                                      cpu=Cpu.x64)
    self.compiler1_uncached = TestableCompiler(
        gn_out=pathlib.Path('/tmp/compiler1'),
        source_root=SOURCE_ROOT,
        error_dir=None,
        use_cache=False,
        os=Os.Linux,
        cpu=Cpu.x64)
    self.compiler2 = TestableCompiler(gn_out=pathlib.Path('/tmp/compiler2'),
                                      source_root=SOURCE_ROOT,
                                      error_dir=None,
                                      use_cache=True,
                                      os=Os.Linux,
                                      cpu=Cpu.x64)

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

  def test_parse_depfile(self):
    self.assertListEqual(
        self.compiler1._parse_depfile(
            textwrap.dedent("""\
          /dev/null: /tmp/main.cc \\
            /path/to/foo.txt \\
            ../up.h \\
            path/to/relative \\
            /path/to/absolute.hpp \\
            path\\ with\\ spaces path2.h \\
          """)),
        [
            pathlib.Path('/tmp/up.h'),
            pathlib.Path('/tmp/compiler1/path/to/relative'),
            pathlib.Path('/path/to/absolute.hpp'),
            pathlib.Path('/tmp/compiler1/path with spaces'),
            pathlib.Path('/tmp/compiler1/path2.h'),
        ],
    )


if __name__ == '__main__':
  unittest.main()
