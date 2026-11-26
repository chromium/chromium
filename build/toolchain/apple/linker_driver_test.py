#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for linker_driver.py."""

import os
import sys
import tempfile
import unittest
from unittest import mock

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

import linker_driver  # noqa: E402

_SAMPLE_OTOOL_WITH_ID = b"""\
libfoo.dylib:
Load command 0
      cmd LC_SEGMENT_64
  cmdsize 72
  segname __TEXT
Load command 1
          cmd LC_ID_DYLIB
      cmdsize 56
         name @rpath/libfoo.dylib (offset 24)
   time stamp 2 Thu Jan  1 00:00:02 1970
      current version 0.0.0
compatibility version 0.0.0
Load command 2
     cmd LC_SYMTAB
 cmdsize 24
"""

_SAMPLE_OTOOL_NO_ID = b"""\
foo.so:
Load command 0
      cmd LC_SEGMENT_64
  cmdsize 72
  segname __TEXT
"""


class LinkerDriverTocTest(unittest.TestCase):
    def _make_driver(self, output='libfoo.dylib'):
        driver = linker_driver.LinkerDriver(
            [
                'linker_driver.py',
                '-Wcrl,driver,clang++',
                '-Wcrl,otoolpath,llvm-otool',
                '-Wcrl,nmpath,llvm-nm',
                '-o',
                output,
            ]
        )
        driver.set_otool_path('llvm-otool')
        driver.set_nm_path('llvm-nm')
        return driver

    def test_extract_toc_with_lc_id_dylib(self):
        driver = self._make_driver()
        nm_out = (
            b'\n'
            b'libfoo.dylib:\n'
            b'0000000000001000 T _foo\n'
            b'0000000000002000 D _bar\n'
            b'0000000000000000 I _alias (indirect for _foo)\n'
        )
        with mock.patch(
            'subprocess.check_output',
            side_effect=[_SAMPLE_OTOOL_WITH_ID, nm_out],
        ) as mock_check_output:
            toc = driver._extract_toc()

        self.assertEqual(
            mock_check_output.call_args_list,
            [
                mock.call(['llvm-otool', '-l', 'libfoo.dylib']),
                mock.call(['llvm-nm', '-Ugp', 'libfoo.dylib']),
            ],
        )
        self.assertEqual(
            toc,
            b'          cmd LC_ID_DYLIB\n'
            b'      cmdsize 56\n'
            b'         name @rpath/libfoo.dylib (offset 24)\n'
            b'   time stamp 2 Thu Jan  1 00:00:02 1970\n'
            b'      current version 0.0.0\n'
            b'compatibility version 0.0.0\n'
            b'T _foo\n'
            b'D _bar\n'
            b'I _alias (indirect for _foo)\n',
        )

    def test_extract_toc_without_lc_id_dylib(self):
        driver = self._make_driver('foo.so')
        with mock.patch(
            'subprocess.check_output',
            side_effect=[_SAMPLE_OTOOL_NO_ID, b''],
        ):
            self.assertEqual(driver._extract_toc(), b'')

        with mock.patch(
            'subprocess.check_output',
            side_effect=[_SAMPLE_OTOOL_NO_ID, b'0000000000001000 T _entry\n'],
        ):
            self.assertEqual(driver._extract_toc(), b'T _entry\n')

    def test_extract_toc_too_many_lc_id_dylibs(self):
        driver = self._make_driver()
        dup_otool = _SAMPLE_OTOOL_WITH_ID + _SAMPLE_OTOOL_WITH_ID
        with mock.patch(
            'subprocess.check_output',
            side_effect=[dup_otool, b''],
        ):
            with self.assertRaises(ValueError):
                driver._extract_toc()

    def test_extract_toc_large_symbol_table(self):
        driver = self._make_driver()
        num_symbols = 200_000
        nm_out = (
            b'\n'.join(
                f'{i:016x} T __ZN8chromium4test6symbol{i}Ev'.encode()
                for i in range(num_symbols)
            )
            + b'\n'
        )
        with mock.patch(
            'subprocess.check_output',
            side_effect=[_SAMPLE_OTOOL_WITH_ID, nm_out],
        ):
            toc = driver._extract_toc()

        toc_lines = toc.splitlines()
        self.assertEqual(len(toc_lines), 6 + num_symbols)
        self.assertEqual(toc_lines[0], b'          cmd LC_ID_DYLIB')
        self.assertEqual(toc_lines[6], b'T __ZN8chromium4test6symbol0Ev')
        self.assertEqual(
            toc_lines[-1],
            f'T __ZN8chromium4test6symbol{num_symbols - 1}Ev'.encode(),
        )

    def test_output_toc_updates_only_when_changed(self):
        driver = self._make_driver()
        with tempfile.TemporaryDirectory() as tmpdir:
            orig_cwd = os.getcwd()
            os.chdir(tmpdir)
            try:
                toc_name = 'libfoo.dylib.TOC'
                with mock.patch(
                    'subprocess.check_output',
                    side_effect=[
                        _SAMPLE_OTOOL_WITH_ID,
                        b'0000000000001000 T _foo\n',
                    ],
                ):
                    self.assertEqual(driver.output_toc(toc_name), [toc_name])

                os.utime(toc_name, (1000, 1000))
                with mock.patch(
                    'subprocess.check_output',
                    side_effect=[
                        _SAMPLE_OTOOL_WITH_ID,
                        b'0000000000009000 T _foo\n',
                    ],
                ):
                    driver.output_toc(toc_name)
                self.assertEqual(int(os.path.getmtime(toc_name)), 1000)

                with mock.patch(
                    'subprocess.check_output',
                    side_effect=[
                        _SAMPLE_OTOOL_WITH_ID,
                        b'0000000000009000 T _foo\n000000000000a000 T _bar\n',
                    ],
                ):
                    driver.output_toc(toc_name)
                self.assertNotEqual(int(os.path.getmtime(toc_name)), 1000)
                with open(toc_name, 'rb') as f:
                    content = f.read()
                self.assertTrue(content.endswith(b'T _foo\nT _bar\n'))
            finally:
                os.chdir(orig_cwd)


class LinkerDriverNoDsymTest(unittest.TestCase):
    def test_nodsym_skips_object_path_lto_and_writes_dsym_stubs(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            out_bin = os.path.join(tmpdir, 'gen_tool')
            stale_file = os.path.join(
                tmpdir, 'gen_tool.dSYM', 'Contents', 'stale.txt'
            )
            os.makedirs(os.path.dirname(stale_file), exist_ok=True)
            with open(stale_file, 'wb') as f:
                f.write(b'stale')

            driver = linker_driver.LinkerDriver(
                [
                    'linker_driver.py',
                    '-Wcrl,driver,clang++',
                    '-Wcrl,object_path_lto',
                    f'-Wcrl,dsym,{tmpdir}',
                    '-Wcrl,dsymutilpath,/nonexistent/dsymutil',
                    '-Wcrl,nodsym,aarch64',
                    'main.o',
                    '-o',
                    out_bin,
                ]
            )
            with mock.patch('subprocess.check_call') as mock_check_call:
                driver.run()

            self.assertEqual(mock_check_call.call_count, 1)
            link_cmd = mock_check_call.call_args[0][0]
            self.assertEqual(link_cmd, ['clang++', 'main.o', '-o', out_bin])
            self.assertIsNone(driver._object_path_lto)

            dsym_dir = os.path.join(tmpdir, 'gen_tool.dSYM')
            self.assertFalse(os.path.exists(stale_file))
            self.assertTrue(
                os.path.isfile(os.path.join(dsym_dir, 'Contents', 'Info.plist'))
            )
            self.assertTrue(
                os.path.isfile(
                    os.path.join(
                        dsym_dir, 'Contents', 'Resources', 'DWARF', 'gen_tool'
                    )
                )
            )
            self.assertTrue(
                os.path.isfile(
                    os.path.join(
                        dsym_dir,
                        'Contents',
                        'Resources',
                        'Relocations',
                        'aarch64',
                        'gen_tool.yml',
                    )
                )
            )

    def test_nodsym_without_dsym_flag_creates_no_dsym_dir(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            out_bin = os.path.join(tmpdir, 'gen_tool')
            driver = linker_driver.LinkerDriver(
                [
                    'linker_driver.py',
                    '-Wcrl,driver,clang++',
                    '-Wcrl,nodsym,x86_64',
                    'main.o',
                    '-o',
                    out_bin,
                ]
            )
            with mock.patch('subprocess.check_call') as mock_check_call:
                driver.run()

            self.assertEqual(mock_check_call.call_count, 1)
            self.assertFalse(
                os.path.exists(os.path.join(tmpdir, 'gen_tool.dSYM'))
            )

    def test_nodsym_requires_arch(self):
        driver = linker_driver.LinkerDriver(
            [
                'linker_driver.py',
                '-Wcrl,driver,clang++',
                '-Wcrl,nodsym,',
                'main.o',
                '-o',
                'gen_tool',
            ]
        )
        with mock.patch('subprocess.check_call'):
            with self.assertRaises(ValueError):
                driver.run()


if __name__ == '__main__':
    unittest.main()
