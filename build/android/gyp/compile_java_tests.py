#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for compile_java.py"""

import collections
import os
import sys
import tempfile
import types
import unittest
from unittest import mock

import compile_java


def _CreateData(
    class_annotation='', class_prefix='public ', nested_annotation='', suffix=''
):
    return (
        f"""\
package pkg;

import foo;
import foo.Bar;
import foo.Bar.Baz;

{class_annotation}
@SomeThing
{class_prefix}class Foo {{
  {nested_annotation}
  public static class Nested {{ }}
}}
"""
        + suffix
    )


class CompileJavaTests(unittest.TestCase):
    def do_service_loader_test(self, **kwargs):
        data = _CreateData(**kwargs)
        services_map = collections.defaultdict(list)
        compile_java.ParseJavaSource(data, services_map)
        return dict(services_map)

    def do_classes_test(self, **kwargs):
        data = _CreateData(**kwargs)
        services_map = collections.defaultdict(list)
        _, class_names = compile_java.ParseJavaSource(data, services_map)
        return class_names

    def testServiceImpl_NoUses(self):
        services_map = self.do_service_loader_test()
        self.assertEqual({}, services_map)

    def testServiceImpl_LocalType(self):
        services_map = self.do_service_loader_test(
            class_annotation='@ServiceImpl(Local.class)'
        )
        self.assertEqual({'pkg.Local': ['pkg.Foo']}, services_map)

    def testServiceImpl_ImportedTopType(self):
        services_map = self.do_service_loader_test(
            class_annotation='@ServiceImpl(Bar.class)'
        )
        self.assertEqual({'foo.Bar': ['pkg.Foo']}, services_map)

    def testServiceImpl_ImportedNestedType1(self):
        services_map = self.do_service_loader_test(
            class_annotation='@ServiceImpl(Baz.class)'
        )
        self.assertEqual({'foo.Bar$Baz': ['pkg.Foo']}, services_map)

    def testServiceImpl_ImportedNestedType2(self):
        services_map = self.do_service_loader_test(
            class_annotation='@ServiceImpl(Bar.Baz.class)'
        )
        self.assertEqual({'foo.Bar$Baz': ['pkg.Foo']}, services_map)

    def testServiceImpl_NestedImpl(self):
        services_map = self.do_service_loader_test(
            class_annotation='@ServiceImpl(Baz.class)',
            nested_annotation='@ServiceImpl(Baz.class)',
        )
        self.assertEqual(
            {'foo.Bar$Baz': ['pkg.Foo', 'pkg.Foo$Nested']}, services_map
        )

    def testParseClasses(self):
        classes = self.do_classes_test(
            class_prefix='public final ', suffix='\nprivate class Extra {}'
        )
        self.assertEqual(['Foo', 'Extra'], classes)

    def testErrorOnNonPublic(self):

        def inner():
            self.do_classes_test(
                class_annotation='@ServiceImpl(Local.class)', class_prefix=''
            )

        self.assertRaises(Exception, inner)


class ShardingTests(unittest.TestCase):
    def setUp(self):
        self._temp_dir = tempfile.TemporaryDirectory()
        self._num_files = 0

    def tearDown(self):
        self._temp_dir.cleanup()

    def _CreateFiles(self, sizes):
        paths = []
        for size in sizes:
            path = os.path.join(
                self._temp_dir.name, f'F{self._num_files:02}.java'
            )
            self._num_files += 1
            with open(path, 'w') as f:
                f.write('x' * size)
            paths.append(path)
        return paths

    def testComputeNumShards(self):
        target = compile_java._JAVAC_SHARD_TARGET_SIZE
        self.assertEqual(1, compile_java._ComputeNumShards([]))
        with mock.patch.object(compile_java, '_MIN_FILES_TO_SHARD', 0):
            files = self._CreateFiles([target // 2])
            self.assertEqual(1, compile_java._ComputeNumShards(files))
            files = self._CreateFiles([target] * 3)
            self.assertEqual(3, compile_java._ComputeNumShards(files))

    def testComputeNumShards_TooFewFiles(self):
        num_files = compile_java._MIN_FILES_TO_SHARD
        size = compile_java._JAVAC_SHARD_TARGET_SIZE * 4 // num_files
        files = self._CreateFiles([size] * num_files)
        self.assertEqual(1, compile_java._ComputeNumShards(files[1:]))
        self.assertEqual(4, compile_java._ComputeNumShards(files))

    def testComputeNumShards_Max(self):
        files = self._CreateFiles([10] * 100)
        with mock.patch.object(compile_java, '_JAVAC_SHARD_TARGET_SIZE', 10):
            self.assertEqual(
                compile_java._MAX_JAVAC_SHARDS,
                compile_java._ComputeNumShards(files),
            )

    def testShardJavaFiles(self):
        files = self._CreateFiles([10, 10, 10, 10, 40, 10, 10])
        shards = compile_java._ShardJavaFiles(list(reversed(files)), 3)
        # Files stay in sorted order, and are balanced by size.
        self.assertEqual(
            [files[0:3], files[3:5], files[5:7]],
            shards,
        )
        self.assertEqual(sorted(files), sorted(sum(shards, [])))

    def testShardJavaFiles_OmitsEmptyShards(self):
        files = self._CreateFiles([100, 1])
        shards = compile_java._ShardJavaFiles(files, 4)
        self.assertEqual([[files[0]], [files[1]]], shards)

    def _RunShardedJavac(
        self, sizes, chromium_code=True, warnings_as_errors=False
    ):
        files = self._CreateFiles(sizes)
        rsp_path = os.path.join(self._temp_dir.name, 'files.txt')
        # Prints the files it is given, and fails if F02.java is one of them.
        script = (
            'import sys; files = open(sys.argv[1][1:]).read().split(); '
            'names = [f.rsplit("/", 1)[1] for f in files]; '
            'print("out", *names); print("err", *names, file=sys.stderr); '
            'sys.exit("F02.java" in names)'
        )
        options = types.SimpleNamespace(
            chromium_code=chromium_code, warnings_as_errors=warnings_as_errors
        )
        callback = mock.Mock()
        output_filter = mock.Mock(side_effect=lambda x: x)
        try:
            compile_java._RunShardedJavac(
                [sys.executable, '-c', script, '@' + rsp_path],
                files,
                len(sizes),
                rsp_path,
                options,
                output_filter,
                callback,
            )
        finally:
            callback.assert_called_once_with()
            # Called once for stdout and once for stderr (not per-shard).
            self.assertEqual(2, output_filter.call_count)

    def testRunShardedJavac(self):
        with mock.patch('sys.stdout.write') as stdout:
            with mock.patch('sys.stderr.write') as stderr:
                self._RunShardedJavac([10, 10], chromium_code=True)
        # Output is combined in shard order.
        self.assertEqual(
            mock.call('out F00.java\nout F01.java\n'), stdout.call_args_list[0]
        )
        self.assertEqual(
            mock.call('err F00.java\nerr F01.java\n'), stderr.call_args_list[0]
        )

    def testRunShardedJavac_Failure(self):
        with self.assertRaises(
            compile_java.build_utils.CalledProcessError
        ) as cm:
            self._RunShardedJavac([10, 10, 10])
        # Contains output from all shards.
        self.assertIn(
            'err F00.java\nerr F01.java\nerr F02.java', str(cm.exception)
        )

    def testRunShardedJavac_WarningsAsErrors(self):
        with mock.patch('sys.stdout.write'), mock.patch('sys.stderr.write'):
            with self.assertRaises(compile_java.build_utils.CalledProcessError):
                self._RunShardedJavac([10, 10], warnings_as_errors=True)

    def testRunShardedJavac_NotChromiumCode(self):
        with mock.patch('sys.stdout.write') as stdout:
            with mock.patch('sys.stderr.write'):
                self._RunShardedJavac([10, 10], chromium_code=False)
        stdout.assert_not_called()


if __name__ == '__main__':
    unittest.main()
