#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import unittest

from generate_system_modulemap import parse_modulemap, Header, calculate_transitive_headers, combine_modulemaps

_TESTDATA = (pathlib.Path(__file__).parent / 'testdata').resolve()
_LIBCXX = _TESTDATA / 'libc++'
_SYSROOT = _TESTDATA / 'sysroot'
_CHROMIUM = pathlib.Path(__file__).parents[3]
_CLANG = _CHROMIUM / 'third_party/llvm-build/Release+Asserts/bin/clang++'
_WANT_MODULEMAP = """module "//system" [system] {
module root {
  requires gnuinlineasm, !systemz
  header "../../libc++/root.h"

  module submodule {
    requires arm64
    requires x64, gnuinlineasm
    header "../../libc++/subdir/requires_unmet.h"
  }
  textual header "../../libc++/textual.h"
  private header "../../libc++/private.h"
}

module "missing.h" {
  private textual header "../../libc++/subdir/bits/missing.h"
  export *
}
module "missing.h_2" {
  private textual header "../../libc++/subdir/missing.h"
  export *
}
module "public_root.h" {
  header "../../sysroot/usr/include/public_root.h"
  export *
}
module "sysroot.h" {
  private header "../../sysroot/usr/include/sysroot.h"
  export *
}
module "public_subdir.h" {
  textual header "../../sysroot/usr/include/x86_64-linux-gnu/bits/public_subdir.h"
  export *
}
module "sysroot.h_2" {
  private textual header "../../sysroot/usr/include/x86_64-linux-gnu/bits/sysroot.h"
  export *
}
}
"""


class GenerateSysrootModulemapTest(unittest.TestCase):
  maxDiff = None

  def test_generate_system_modulemap(self):
    common_dir, headers = parse_modulemap(_TESTDATA / 'gen/module.modulemap')
    self.assertEqual(common_dir, _LIBCXX)

    self.assertEqual(headers, [
        Header(path=pathlib.Path('root.h'),
               private=False,
               textual=False,
               requires=['gnuinlineasm', '!systemz']),
        Header(path=pathlib.Path('subdir/requires_unmet.h'),
               private=False,
               textual=False,
               requires=[
                   'gnuinlineasm', '!systemz', 'arm64', 'x64', 'gnuinlineasm'
               ]),
        Header(path=pathlib.Path('textual.h'),
               private=False,
               textual=True,
               requires=['gnuinlineasm', '!systemz']),
        Header(path=pathlib.Path('private.h'),
               private=True,
               textual=False,
               requires=['gnuinlineasm', '!systemz']),
    ])

    deps = calculate_transitive_headers(
        clang_args=[_CLANG, '-isystem',
                    str(_LIBCXX), f'--sysroot={_SYSROOT}'],
        include_dirs=[(common_dir, headers)],
        sysroot=_SYSROOT,
        extra_public_headers=['public_root.h', 'bits/public_subdir.h'],
        target_os='linux',
        target_cpu='x64',
    )

    self.assertEqual(sorted(deps), [
        Header(
            path=_LIBCXX / 'subdir/bits/missing.h', private=True, textual=True),
        Header(path=_LIBCXX / 'subdir/missing.h', private=True, textual=True),
        Header(path=_SYSROOT / 'usr/include/public_root.h',
               private=False,
               textual=False),
        Header(path=_SYSROOT / 'usr/include/sysroot.h',
               private=True,
               textual=False),
        Header(
            path=_SYSROOT / 'usr/include/x86_64-linux-gnu/bits/public_subdir.h',
            private=False,
            textual=True),
        Header(path=_SYSROOT / 'usr/include/x86_64-linux-gnu/bits/sysroot.h',
               private=True,
               textual=True),
    ])

    self.assertEqual(
        combine_modulemaps(out=_TESTDATA / 'want/subdir' / 'module.modulemap',
                           modulemaps=[_TESTDATA / 'gen/module.modulemap'],
                           headers=deps,
                           module_name='//system'), _WANT_MODULEMAP)


if __name__ == '__main__':
  unittest.main()
