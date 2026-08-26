#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

sys.path.append(os.path.dirname(__file__))
from filter_clang_args import filter_clang_args

NEUTRALIZING_FLAGS = [
    '-w',
    '-Wno-unknown-argument',
    '-Wno-unknown-warning-option',
    '-Wno-unused-command-line-argument',
]


class FilterClangArgsTest(unittest.TestCase):
    def test_preserves_standard_flags(self):
        args = [
            '-D__STDC_CONSTANT_MACROS',
            '-D_FORTIFY_SOURCE=2',
            '-I../..',
            '-Ix64/gen',
            '-isystem../../third_party/libc++/src/include',
            '--sysroot=../../build/linux/debian_bullseye_amd64-sysroot',
            '-std=gnu++20',
            '-fPIC',
            '-fno-exceptions',
            '-fno-rtti',
            '-O2',
            '-m64',
        ]
        self.assertEqual(filter_clang_args(args), args + NEUTRALIZING_FLAGS)

    def test_filters_gcc_specific_warnings(self):
        args = [
            '-Wall',
            '-Werror',
            '-Wno-maybe-uninitialized',
            '-Wno-packed-not-aligned',
            '-Wno-class-memaccess',
            '-Wno-psabi',
            '-Wno-stringop-overread',
            '-Wno-unused-parameter',
            '-I../../base',
        ]
        expected = ['-I../../base'] + NEUTRALIZING_FLAGS
        self.assertEqual(filter_clang_args(args), expected)

    def test_filters_plugin_and_time_trace_args(self):
        args = [
            '-I../../base',
            '-ftime-trace',
            '-Xclang',
            '-add-plugin',
            '-Xclang',
            'find-bad-constructs',
            '-Xclang',
            '-plugin-arg-find-bad-constructs',
            '-Xclang',
            'check-ipc',
            '-Xclang',
            '-plugin-arg-find-bad-constructs-flag',
            '-O2',
        ]
        expected = ['-I../../base', '-O2'] + NEUTRALIZING_FLAGS
        self.assertEqual(filter_clang_args(args), expected)

    def test_filters_clang_cl_warnings_and_wx(self):
        args = [
            '-I../../base',
            '/W4',
            '/WX',
            '/wd4244',
            '/wd4117',
            '/clang:-Wall',
            '/clang:-Wno-unused-variable',
            '-O2',
        ]
        expected = ['-I../../base', '-O2'] + NEUTRALIZING_FLAGS
        self.assertEqual(filter_clang_args(args), expected)

    def test_preserves_non_plugin_xclang_args(self):
        args = [
            '-I../../base',
            '-Xclang',
            '-fmodule-file-home-is-cwd',
            '-Xclang',
            '-fmodules-cache-path=/dummy_dir',
            '-Xclang',
            '--warning-suppression-mappings=warning_suppression.txt',
            '-O2',
        ]
        self.assertEqual(filter_clang_args(args), args + NEUTRALIZING_FLAGS)


if __name__ == '__main__':
    unittest.main()
