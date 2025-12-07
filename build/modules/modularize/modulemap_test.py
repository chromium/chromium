# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import pathlib
import unittest

import modulemap
import graph

_TESTDATA = pathlib.Path(__file__).parent / 'testdata'
_FRAMEWORK = graph.IncludeDir.Framework
_SYSROOT = graph.IncludeDir.SysrootModule


@dataclasses.dataclass
class HeaderProps:
  include_dir: graph.IncludeDir
  abs: str
  module: str
  textual: bool = False
  umbrella: bool = False


class ModulemapTest(unittest.TestCase):

  def setUp(self):
    super().setUp()
    self.maxDiff = None

  def test_parse_modulemap(self):
    got_modules, got_headers = modulemap.calculate_modules([
        (_TESTDATA / 'Frameworks', graph.IncludeDir.Framework),
        (_TESTDATA / 'sysroot', graph.IncludeDir.SysrootModule),
    ])

    got_headers = {
        hdr.rel:
        HeaderProps(
            include_dir=hdr.include_dir,
            abs=str(hdr.abs.relative_to(_TESTDATA)),
            module=hdr.root_module,
            textual=hdr.textual,
            umbrella=hdr.umbrella,
        )
        for hdr in got_headers
    }

    self.assertEqual(
        got_headers, {
            'umbrella/first.h':
            HeaderProps(
                include_dir=_FRAMEWORK,
                abs='Frameworks/umbrella.framework/Headers/first.h',
                module='umbrella',
            ),
            'umbrella/second.h':
            HeaderProps(
                include_dir=_FRAMEWORK,
                abs='Frameworks/umbrella.framework/Headers/second.h',
                module='umbrella',
            ),
            'first.h':
            HeaderProps(
                include_dir=_SYSROOT,
                abs='sysroot/first.h',
                module='first',
            ),
            'first/first.h':
            HeaderProps(
                include_dir=_SYSROOT,
                abs='sysroot/first/first.h',
                module='first',
            ),
            'first_textual.h':
            HeaderProps(
                include_dir=_SYSROOT,
                abs='sysroot/first_textual.h',
                module='first',
                textual=True,
            ),
            'nested/nested.h':
            HeaderProps(
                include_dir=_FRAMEWORK,
                abs='Frameworks/nested.framework/Headers/nested.h',
                module='nested',
                textual=True,
            ),
            'second.h':
            HeaderProps(
                include_dir=_SYSROOT,
                abs='sysroot/second.h',
                module='second',
            ),
            'second_textual.h':
            HeaderProps(
                include_dir=_SYSROOT,
                abs='sysroot/second_textual.h',
                module='second',
                textual=True,
            ),
            'simple/simple.h':
            HeaderProps(
                include_dir=_FRAMEWORK,
                abs='Frameworks/simple.framework/Headers/simple.h',
                module='simple',
            ),
            'subdir/subdir.h':
            HeaderProps(
                include_dir=_SYSROOT,
                abs='sysroot/subdir/subdir.h',
                module='subdir',
            ),
            'submodule/umbrella.h':
            HeaderProps(
                include_dir=_FRAMEWORK,
                abs=
                'Frameworks/nested.framework/Frameworks/submodule.framework/Headers/umbrella.h',
                module='nested',
                umbrella=True,
            ),
        })

    got_modules = {
        mod: str(path.relative_to(_TESTDATA))
        for mod, path in got_modules.items()
    }

    self.assertEqual(
        got_modules, {
            'container': 'sysroot/module.modulemap',
            'first': 'sysroot/importable.modulemap',
            'nested': 'Frameworks/nested.framework/Modules/module.modulemap',
            'second': 'sysroot/importable.modulemap',
            'simple': 'Frameworks/simple.framework/Modules/module.modulemap',
            'subdir': 'sysroot/subdir/module.modulemap',
            'umbrella':
            'Frameworks/umbrella.framework/Modules/module.modulemap',
        })


if __name__ == '__main__':
  unittest.main()
