#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import os
import tempfile
import unittest
import unittest.mock
import zipfile

_RealZipFile = zipfile.ZipFile

import dex  # noqa: E402


class DexTest(unittest.TestCase):
    def testStdErrFilter(self):
        output = """\
some initial message
Warning in ../../clank/third_party/google3/pg_confs/java_com_google_protobuf_lite_proguard.pgcfg:
Rule matches the static final field `java.lang.String com.google.protobuf.BaseGeneratedExtensionRegistryLite.CONTAINING_TYPE_0`, which may have been inlined: -identifiernamestring class com.google.protobuf.*GeneratedExtensionRegistryLite {
  static java.lang.String CONTAINING_TYPE_*;
}
Warning: some message
Warning in gen/.../Foo.jar:Bar.class:
  Type `libcore.io.Memory` was not found, it is required for default or static interface methods desugaring of `void Bar.a(long, byte)`
Warning: Missing class com.google.android.apps.gsa.search.shared.service.proto.PublicStopClientEvent (referenced from: com.google.protobuf.GeneratedMessageLite$GeneratedExtension com.google.protobuf.BaseGeneratedExtensionRegistryLite.findLiteExtensionByNumber(com.google.protobuf.MessageLite, int))
Missing class com.google.android.gms.feedback.ApplicationProperties (referenced from: com.google.protobuf.GeneratedMessageLite$GeneratedExtension com.google.protobuf.BaseGeneratedExtensionRegistryLite.findLiteExtensionByNumber(com.google.protobuf.MessageLite, int))
"""
        expected = """\
some initial message
Warning: some message
Missing class com.google.android.gms.feedback.ApplicationProperties (referenced from: com.google.protobuf.GeneratedMessageLite$GeneratedExtension com.google.protobuf.BaseGeneratedExtensionRegistryLite.findLiteExtensionByNumber(com.google.protobuf.MessageLite, int))
"""
        filters = dex.DEFAULT_IGNORE_WARNINGS + (
            'CONTAINING_TYPE_',
            'libcore',
            'PublicStopClientEvent',
        )
        filter_func = dex.CreateStderrFilter(filters)
        self.assertEqual(filter_func(output), expected)

        # Test no preamble, not filtered.
        output = """Warning: hi"""
        expected = output
        self.assertEqual(filter_func(output), expected)

        # Test no preamble, filtered
        output = """\
Warning: PublicStopClientEvent is hungry.
"""
        expected = ''
        self.assertEqual(filter_func(output), expected)

        # Test uppercase WARNING line (e.g. JVM Unsafe warning) does not mask preceding warning
        output = """\
Warning: Missing class com.example.Foo
WARNING: A terminally deprecated method in sun.misc.Unsafe has been called
"""
        expected = """\
Warning: Missing class com.example.Foo
"""
        self.assertEqual(filter_func(output), expected)

    def testClassFileNestPrefix(self):
        cases = {
            'pkg/Top.class': 'pkg/Top',
            'pkg/Outer$Inner.class': 'pkg/Outer',
            'pkg/Outer$Inner$Deep.class': 'pkg/Outer',
            'Outer$1.class': 'Outer',
            # '$' inside the package path must be ignored.
            'weird$pkg/Top.class': 'weird$pkg/Top',
        }
        for path, expected in cases.items():
            self.assertEqual(dex._ClassFileNestPrefix(path), expected, msg=path)

    @unittest.mock.patch('zipfile.ZipFile')
    def testCreateServicesMap(self, mock_zipfile):

        def create_zip_data(files):
            bio = io.BytesIO()
            with _RealZipFile(bio, 'w') as z:
                for name, content in files.items():
                    z.writestr(name, content)
            bio.seek(0)
            return bio

        zip_data_1 = create_zip_data(
            {'META-INF/services/foo.Bar': b'impl.Bar1\n'}
        )
        zip_data_2 = create_zip_data(
            {'META-INF/services/foo.Bar': b'impl.Bar2\n'}
        )
        zip_data_3 = create_zip_data(
            {'META-INF/services/foo.Bar': b'impl.Bar1\n'}
        )
        zip_data_non_conflicting = create_zip_data(
            {'META-INF/services/foo.Baz': b'impl.Baz1\n'}
        )
        zip_data_cleanup_1 = create_zip_data(
            {
                'META-INF/services/org.chromium.base.test.BaseJUnit4ClassRunner$ClassCleanupHook': b'impl.Hook1\n'
            }
        )
        zip_data_cleanup_2 = create_zip_data(
            {
                'META-INF/services/org.chromium.base.test.BaseJUnit4ClassRunner$ClassCleanupHook': b'impl.Hook2\n'
            }
        )

        def side_effect(path, mode='r'):
            del mode
            if path == 'jar1.jar':
                return _RealZipFile(zip_data_1, 'r')
            if path == 'jar2.jar':
                return _RealZipFile(zip_data_2, 'r')
            if path == 'jar3.jar':
                return _RealZipFile(zip_data_3, 'r')
            if path == 'jar_baz.jar':
                return _RealZipFile(zip_data_non_conflicting, 'r')
            if path == 'cleanup1.jar':
                return _RealZipFile(zip_data_cleanup_1, 'r')
            if path == 'cleanup2.jar':
                return _RealZipFile(zip_data_cleanup_2, 'r')
            raise FileNotFoundError(path)

        mock_zipfile.side_effect = side_effect

        # Case 1: Same content (no conflict)
        res = dex._CreateServicesMap(['jar1.jar', 'jar3.jar'])
        self.assertEqual(res, {'META-INF/services/foo.Bar': 'impl.Bar1\n'})

        # Case 2: Conflicting content, not in merge list -> should raise Exception
        with self.assertRaises(Exception) as cm:
            dex._CreateServicesMap(['jar1.jar', 'jar2.jar'])
        self.assertIn(
            'Conflicting contents for: META-INF/services/foo.Bar',
            str(cm.exception),
        )

        # Case 3: Conflicting content, but in merge list -> should merge
        orig_merge = dex._MERGE_SERVICE_ENTRIES
        dex._MERGE_SERVICE_ENTRIES = ('META-INF/services/foo.Bar',)
        try:
            res = dex._CreateServicesMap(['jar1.jar', 'jar2.jar'])
            self.assertEqual(
                res, {'META-INF/services/foo.Bar': 'impl.Bar1\nimpl.Bar2\n'}
            )
        finally:
            dex._MERGE_SERVICE_ENTRIES = orig_merge

        # Case 4: Different services (no conflict)
        res = dex._CreateServicesMap(['jar1.jar', 'jar_baz.jar'])
        self.assertEqual(
            res,
            {
                'META-INF/services/foo.Bar': 'impl.Bar1\n',
                'META-INF/services/foo.Baz': 'impl.Baz1\n',
            },
        )

        # Case 5: Real whitelisted service (ClassCleanupHook) -> should merge
        res = dex._CreateServicesMap(['cleanup1.jar', 'cleanup2.jar'])
        self.assertEqual(
            res,
            {
                'META-INF/services/org.chromium.base.test.BaseJUnit4ClassRunner$ClassCleanupHook': 'impl.Hook1\nimpl.Hook2\n'
            },
        )

    def testMergeDexAndServices(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create input jars
            jar1_path = os.path.join(tmpdir, 'jar1.jar')
            jar2_path = os.path.join(tmpdir, 'jar2.jar')

            # Use an allowlisted service name to avoid exception
            service_name = 'META-INF/services/org.chromium.base.test.BaseJUnit4ClassRunner$ClassCleanupHook'

            with _RealZipFile(jar1_path, 'w') as z1:
                z1.writestr(
                    'classes.dex',
                    b'dex1_data_is_long_enough_to_avoid_compression_bypass',
                )
                z1.writestr(service_name, b'impl.Hook1\n')

            with _RealZipFile(jar2_path, 'w') as z2:
                z2.writestr(
                    'classes.dex',
                    b'dex2_data_is_long_enough_to_avoid_compression_bypass',
                )
                z2.writestr(
                    'classes2.dex',
                    b'dex3_data_is_long_enough_to_avoid_compression_bypass',
                )
                z2.writestr(service_name, b'impl.Hook2\n')

            # Merge with uncompress_dex = True
            zip_builder = unittest.mock.MagicMock()
            dex.MergeDexAndServices(
                [jar1_path, jar2_path],
                zip_builder,
                apk_root_dir='root/',
                apk_dex_dir='dex/',
                uncompress_dex=True,
            )

            zip_builder.add_data.assert_called_once_with(
                'root/' + service_name,
                'impl.Hook1\nimpl.Hook2\n',
                compress_level=0,
                alignment=4,
            )
            self.assertEqual(
                zip_builder.add_from_zip.call_args_list,
                [
                    unittest.mock.call(
                        'dex/classes.dex',
                        jar1_path,
                        'classes.dex',
                        compress_level=0,
                        alignment=4,
                    ),
                    unittest.mock.call(
                        'dex/classes2.dex',
                        jar2_path,
                        'classes.dex',
                        compress_level=0,
                        alignment=4,
                    ),
                    unittest.mock.call(
                        'dex/classes3.dex',
                        jar2_path,
                        'classes2.dex',
                        compress_level=0,
                        alignment=4,
                    ),
                ],
            )

    def testMergeDexAndServices_SingleJar(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            jar_path = os.path.join(tmpdir, 'jar.jar')
            service_name = 'META-INF/services/org.chromium.base.test.BaseJUnit4ClassRunner$ClassCleanupHook'

            with _RealZipFile(jar_path, 'w') as z:
                z.writestr(
                    'classes.dex',
                    b'dex2_data_is_long_enough_to_avoid_compression_bypass',
                )
                z.writestr(
                    'classes2.dex',
                    b'dex3_data_is_long_enough_to_avoid_compression_bypass',
                )
                z.writestr(service_name, b'impl.Hook2\n')

            zip_builder = unittest.mock.MagicMock()
            dex.MergeDexAndServices(
                [jar_path],
                zip_builder,
                apk_root_dir='root/',
                apk_dex_dir='dex/',
                uncompress_dex=False,
            )

            zip_builder.add_data.assert_called_once_with(
                'root/' + service_name,
                'impl.Hook2\n',
                compress_level=0,
                alignment=4,
            )
            self.assertEqual(
                zip_builder.add_from_zip.call_args_list,
                [
                    unittest.mock.call(
                        'dex/classes.dex',
                        jar_path,
                        'classes.dex',
                        compress_level=1,
                        alignment=None,
                    ),
                    unittest.mock.call(
                        'dex/classes2.dex',
                        jar_path,
                        'classes2.dex',
                        compress_level=1,
                        alignment=None,
                    ),
                ],
            )

    def testScanClassInputJars(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            jar1 = os.path.join(tmpdir, 'one.jar')
            jar2 = os.path.join(tmpdir, 'two.jar')
            with _RealZipFile(jar1, 'w') as z:
                z.writestr('pkg/Foo.class', b'a' * 100)
                z.writestr('module-info.class', b'skip')
                z.writestr('META-INF/MANIFEST.MF', b'manifest')
            with _RealZipFile(jar2, 'w') as z:
                z.writestr('pkg/Bar.class', b'b' * 250)
                z.writestr('META-INF/services/', b'')
                z.writestr('META-INF/services/pkg.Svc', b'pkg.Bar\n')

            entries, service_jars = dex._ScanClassInputJars([jar1, jar2])
            self.assertEqual(
                entries,
                {
                    jar1: [('pkg/Foo.class', 100)],
                    jar2: [('pkg/Bar.class', 250)],
                },
            )
            self.assertEqual(service_jars, [jar2])

    def testPartitionClassEntriesIntoShards_KeepsNestsTogether(self):
        jar_entries = {
            'a.jar': [
                ('pkg/Big.class', 900),
                ('pkg/Big$1.class', 300),
                ('pkg/Big$Inner.class', 300),
                ('pkg/SmallA.class', 700),
                ('pkg/SmallB.class', 700),
            ],
        }
        shards = dex._PartitionClassEntriesIntoShards(
            jar_entries, target_shard_size=1000
        )
        self.assertGreaterEqual(len(shards), 2)
        all_seen = []
        big_shards = []
        for idx, shard in enumerate(shards):
            names = shard.get('a.jar', [])
            all_seen.extend(names)
            if any(n.startswith('pkg/Big') for n in names):
                big_shards.append(idx)
        self.assertEqual(len(big_shards), 1)
        self.assertCountEqual(
            all_seen, [name for name, _ in jar_entries['a.jar']]
        )

    def testBuildJarInputArgs(self):
        jar_entries = {
            'full.jar': [('a/A.class', 10), ('a/B.class', 20)],
            'part.jar': [('b/C.class', 10), ('b/D.class', 20)],
        }
        shard = {
            'full.jar': ['a/A.class', 'a/B.class'],
            'part.jar': ['b/D.class'],
            'empty.jar': [],
        }
        args = dex._BuildJarInputArgs(shard, jar_entries)
        self.assertEqual(
            args,
            [
                '--jar-input',
                'full.jar',
                '--jar-input',
                'part.jar',
                '--jar-class',
                'b/D.class',
            ],
        )

    @unittest.mock.patch('dex.zipbuilder.ZipBuilder')
    def testMergeShardJars(self, mock_zip_builder):
        zb = mock_zip_builder.return_value.__enter__.return_value
        services = {
            'META-INF/services/pkg.SvcB': 'pkg.ImplB\n',
            'META-INF/services/pkg.SvcA': 'pkg.ImplA\n',
        }
        dex._MergeShardJars(
            ['s0.dex.jar', 's1.dex.jar'],
            'merged.dex.jar',
            services,
            warnings_as_errors=True,
        )

        self.assertEqual(
            zb.add_all_from_zip.call_args_list,
            [
                unittest.mock.call(
                    's0.dex.jar',
                    compress_level=dex.zipbuilder.KEEP_COMPRESSION,
                ),
                unittest.mock.call(
                    's1.dex.jar',
                    compress_level=dex.zipbuilder.KEEP_COMPRESSION,
                ),
            ],
        )
        self.assertEqual(
            zb.add_data.call_args_list,
            [
                unittest.mock.call(
                    'META-INF/services/pkg.SvcA',
                    'pkg.ImplA\n',
                    compress_level=0,
                ),
                unittest.mock.call(
                    'META-INF/services/pkg.SvcB',
                    'pkg.ImplB\n',
                    compress_level=0,
                ),
            ],
        )
        zb.write.assert_called_once_with(
            'merged.dex.jar', warnings_as_errors=True
        )


if __name__ == '__main__':
    unittest.main()
