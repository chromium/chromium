#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import pathlib
import textwrap
import unittest

import modulemap_config

from generate_system_modulemap import (
    AllowList,
    Header,
    calculate_transitive_headers,
    combine_modulemaps,
    parse_depfile,
    parse_modulemap,
    split_modulemap,
    modify_modulemap,
)

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

module "never_referenced.h" {
  textual header "../../sysroot/usr/include/never_referenced.h"
  export *
}
module "public_root.h" {
  header "../../sysroot/usr/include/public_root.h"
  export *
}
module "sysroot.h" {
  header "../../sysroot/usr/include/sysroot.h"
  export *
}
module "public_subdir.h" {
  textual header \
"../../sysroot/usr/include/x86_64-linux-gnu/bits/public_subdir.h"
  export *
}
}
"""
_MODULES = """
module foo {
   module submodule {
     header "bar.h"
   }
}
explicit framework module "bar" [system] {
   header "bar.h"
}
"""
_WANT_SPLIT_MODULEMAP = {
    "foo": """module foo {
   module submodule {
     header "bar.h"
   }
}""",
    "bar": """explicit framework module "bar" [system] {
   header "bar.h"
}"""
}


class GenerateSysrootModulemapTest(unittest.TestCase):
  maxDiff = None

  def test_allowlist(self):
    # Test successful allowlist extraction
    hdrs = [
        modulemap_config.Header('normal.h'),
        modulemap_config.Header('only_on_win.h',
                                module_name='only_on_win',
                                exports=['*', 'other']),
        modulemap_config.Header('textual.h', textual=True),
        modulemap_config.Header('win_lazy.h', lazy=True),
        modulemap_config.Header('win_textual.h', textual=True),
    ]
    al = AllowList(hdrs)
    self.assertEqual(
        al.includes(),
        ['normal.h', 'only_on_win.h', 'textual.h', 'win_textual.h'])
    self.assertIsNone(al.textual('normal.h'))
    self.assertTrue(al.textual('textual.h'))
    self.assertTrue(al.textual('win_textual.h'))
    self.assertTrue(al.textual('bits/some.h'))
    self.assertFalse(al.private('normal.h'))
    self.assertTrue(al.private('unknown.h'))
    self.assertEqual(al.module_name('only_on_win.h'), 'only_on_win')
    self.assertIsNone(al.module_name('normal.h'))
    self.assertEqual(al.exports('only_on_win.h'), ['*', 'other'])
    self.assertEqual(al.exports('normal.h'), ['*'])

  def test_generate_system_modulemap(self):
    common_dir, headers = parse_modulemap(_TESTDATA / 'gen/module.modulemap',
                                          {}, set())
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
        clang_args=[
            str(_CLANG), '-isystem',
            str(_LIBCXX), f'--sysroot={_SYSROOT}', '--target=x86_64-linux-gnu'
        ],
        include_dirs=[(common_dir, headers)],
        sysroot_dirs=[
            _SYSROOT / 'usr/include', _SYSROOT / 'usr/include/x86_64-linux-gnu'
        ],
        allowlist=AllowList([
            modulemap_config.Header('bits/public_subdir.h'),
            modulemap_config.Header('public_root.h'),
            modulemap_config.AllowedHeader('sysroot.h'),
            modulemap_config.AllowedHeader('never_referenced.h'),
        ]),
        target_os='linux',
        target_cpu='x64',
    )

    self.assertEqual(sorted(deps), [
        Header(
            path=_LIBCXX / 'subdir/bits/missing.h', private=True, textual=True),
        Header(path=_LIBCXX / 'subdir/missing.h', private=True, textual=True),
        Header(path=_SYSROOT / 'usr/include/never_referenced.h',
               private=False,
               textual=True),
        Header(path=_SYSROOT / 'usr/include/public_root.h',
               private=False,
               textual=False),
        Header(path=_SYSROOT / 'usr/include/sysroot.h',
               private=False,
               textual=False),
        Header(
            path=_SYSROOT / 'usr/include/x86_64-linux-gnu/bits/public_subdir.h',
            private=False,
            textual=True),
        Header(path=_SYSROOT / 'usr/include/x86_64-linux-gnu/bits/sysroot.h',
               private=True,
               textual=True),
    ])

    combined = combine_modulemaps(
        out=_TESTDATA / 'want/subdir' / 'module.modulemap',
        modulemaps=[_TESTDATA / 'gen/module.modulemap'],
        headers=deps,
        module_name='//system',
        extra_modules=[],
        modify_modules={},
        modified_modules=set())
    self.assertEqual(combined, _WANT_MODULEMAP)

  def test_find_modules(self):
    modules = split_modulemap(_MODULES)
    # We can just do assertEqual on the dict, but the diff isn't very nice
    self.assertCountEqual(modules.keys(), _WANT_SPLIT_MODULEMAP.keys())
    for k, v in _WANT_SPLIT_MODULEMAP.items():
      self.assertEqual(modules[k], v)

  def test_parse_depfile(self):
    self.assertEqual(
        parse_depfile(r"""dummy.o: \
  /path/to/header1.h \
  /path/to/header2.h \
  /path/to/header\ with\ spaces.h \
  /path/to/header3.h
"""), [
            pathlib.Path('/path/to/header1.h'),
            pathlib.Path('/path/to/header2.h'),
            pathlib.Path('/path/to/header with spaces.h'),
            pathlib.Path('/path/to/header3.h'),
        ])

  def test_modify_modulemap(self):
    modulemap = textwrap.dedent("""\
        module foo {
          module submodule {
            header "bar.h"
          }
        }""")
    modified_modules = set()
    content = modify_modulemap(modulemap, {
        'foo': lambda s: '',
        'unused': lambda s: '',
    }, modified_modules)
    self.assertEqual(content, '')
    self.assertEqual(modified_modules, {'foo'})

    modified_modules = set()
    content = modify_modulemap(
        modulemap, {'foo.submodule': lambda s: s[:-1] + '  export *\n  }'},
        modified_modules)
    self.assertEqual(
        content,
        textwrap.dedent("""\
            module foo {
              module submodule {
                header "bar.h"
                export *
              }
            }"""))
    self.assertIn('foo.submodule', modified_modules)

    modified_modules = set()
    content = modify_modulemap(
        modulemap, {
            'foo.submodule': lambda s: s[:-1] + '  export submodule\n  }',
            'foo': lambda s: s[:-1] + '  export foo\n}',
            'other.submodule': lambda s: '',
        }, modified_modules)
    self.assertEqual(
        content,
        textwrap.dedent("""\
            module foo {
              module submodule {
                header "bar.h"
                export submodule
              }
              export foo
            }"""))
    self.assertIn('foo.submodule', modified_modules)
    self.assertIn('foo', modified_modules)
    self.assertNotIn('other.submodule', modified_modules)


if __name__ == '__main__':
  unittest.main()
