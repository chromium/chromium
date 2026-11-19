#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import os
import sys
import tempfile
import unittest

sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..'))
from util import zipbuilder


class ZipBuilderTest(unittest.TestCase):
    def _round_trip_spec(self, zb):
        with tempfile.NamedTemporaryFile(
            'w+', encoding='utf8', suffix='.zipspec'
        ) as f:
            zb.write_spec(f)
            f.flush()
            return zipbuilder.parse_spec(f.name)

    def testWriteAndParseSpecRoundTrip(self):
        with zipbuilder.ZipBuilder() as zb:
            zb.add_file(
                'lib/arm64-v8a/libfoo.so',
                'obj/libfoo.so',
                compress_level=0,
                alignment=4096,
            )
            zb.add_from_zip(
                'manifest/AndroidManifest.xml',
                'obj/res.proto.ap_',
                'AndroidManifest.xml',
                compress_level=1,
                alignment=4,
            )
            zb.add_all_from_zip(
                'obj/res.proto.ap_',
                dst_prefix='res/',
                compress_level=zipbuilder.KEEP_COMPRESSION,
                alignment=4,
            )
            entries = self._round_trip_spec(zb)

        self.assertEqual(
            entries,
            [
                zipbuilder.SpecEntry(
                    dest_name='lib/arm64-v8a/libfoo.so',
                    compress_level=0,
                    alignment=4096,
                    src_path='obj/libfoo.so',
                    src_entry_name=None,
                ),
                zipbuilder.SpecEntry(
                    dest_name='manifest/AndroidManifest.xml',
                    compress_level=1,
                    alignment=4,
                    src_path='obj/res.proto.ap_',
                    src_entry_name='AndroidManifest.xml',
                ),
                zipbuilder.SpecEntry(
                    dest_name='res/',
                    compress_level=zipbuilder.KEEP_COMPRESSION,
                    alignment=4,
                    src_path='obj/res.proto.ap_',
                    src_entry_name='',
                ),
            ],
        )

    def testParseSpecInvalidLine(self):
        for bad_line in ('a\t0\t0\n', 'a\t0\t0\tb\tc\textra\n'):
            with tempfile.NamedTemporaryFile(
                'w', encoding='utf8', suffix='.zipspec'
            ) as f:
                f.write(bad_line)
                f.flush()
                with self.assertRaises(ValueError):
                    zipbuilder.parse_spec(f.name)

    def testInlineDataZipPath(self):
        with zipbuilder.ZipBuilder(
            inline_data_zip_path='base.skeleton.zip'
        ) as zb:
            zb.add_data(
                'root/META-INF/services/foo',
                'impl.Foo\n',
                compress_level=0,
                alignment=4,
            )
            zb.add_data(
                'lib/arm64-v8a/libplaceholder.so',
                b'',
                compress_level=0,
                alignment=4096,
            )
            self.assertEqual(
                zb.names,
                {
                    'root/META-INF/services/foo',
                    'lib/arm64-v8a/libplaceholder.so',
                },
            )
            self.assertEqual(
                zb.inline_data,
                {
                    'root/META-INF/services/foo': b'impl.Foo\n',
                    'lib/arm64-v8a/libplaceholder.so': b'',
                },
            )
            entries = self._round_trip_spec(zb)

        self.assertEqual(
            entries,
            [
                zipbuilder.SpecEntry(
                    dest_name='root/META-INF/services/foo',
                    compress_level=0,
                    alignment=4,
                    src_path='base.skeleton.zip',
                    src_entry_name='root/META-INF/services/foo',
                ),
                zipbuilder.SpecEntry(
                    dest_name='lib/arm64-v8a/libplaceholder.so',
                    compress_level=0,
                    alignment=4096,
                    src_path='base.skeleton.zip',
                    src_entry_name='lib/arm64-v8a/libplaceholder.so',
                ),
            ],
        )

    def testAddSpecEntriesOverrides(self):
        source_entries = [
            zipbuilder.SpecEntry('assets/a.txt', 1, 0, 'gen/a.txt', None),
            zipbuilder.SpecEntry('lib/arm64-v8a/liba.so', 0, 4096, 'liba.so'),
            zipbuilder.SpecEntry(
                'dex/classes.dex', 1, 4, 'foo.dex.jar', 'classes.dex'
            ),
            zipbuilder.SpecEntry(
                '', zipbuilder.KEEP_COMPRESSION, 4, 'res.ap_', ''
            ),
        ]

        # KEEP_COMPRESSION clamps loose files to 0, keeps -1 for zip entries.
        with zipbuilder.ZipBuilder() as zb:
            zb.add_spec_entries(
                source_entries,
                dst_prefix='base/',
                compress_level=zipbuilder.KEEP_COMPRESSION,
            )
            uncompressed = self._round_trip_spec(zb)
        self.assertEqual(
            uncompressed,
            [
                zipbuilder.SpecEntry(
                    'base/assets/a.txt', 0, 0, 'gen/a.txt', None
                ),
                zipbuilder.SpecEntry(
                    'base/lib/arm64-v8a/liba.so', 0, 0, 'liba.so', None
                ),
                zipbuilder.SpecEntry(
                    'base/dex/classes.dex',
                    zipbuilder.KEEP_COMPRESSION,
                    0,
                    'foo.dex.jar',
                    'classes.dex',
                ),
                zipbuilder.SpecEntry(
                    'base/', zipbuilder.KEEP_COMPRESSION, 0, 'res.ap_', ''
                ),
            ],
        )

        # Official builds override all entries to compress_level=6.
        with zipbuilder.ZipBuilder() as zb:
            zb.add_spec_entries(
                source_entries,
                dst_prefix='base/',
                compress_level=6,
            )
            official = self._round_trip_spec(zb)
        self.assertEqual(
            official,
            [
                zipbuilder.SpecEntry(
                    'base/assets/a.txt', 6, 0, 'gen/a.txt', None
                ),
                zipbuilder.SpecEntry(
                    'base/lib/arm64-v8a/liba.so', 6, 0, 'liba.so', None
                ),
                zipbuilder.SpecEntry(
                    'base/dex/classes.dex', 6, 0, 'foo.dex.jar', 'classes.dex'
                ),
                zipbuilder.SpecEntry('base/', 6, 0, 'res.ap_', ''),
            ],
        )

    def testLifecycleGuards(self):
        with zipbuilder.ZipBuilder(
            inline_data_zip_path='base.skeleton.zip'
        ) as zb:
            with self.assertRaises(AssertionError):
                zb.write('unused.zip')

        with zipbuilder.ZipBuilder() as zb:
            zb.add_data('a.txt', b'data', compress_level=0)
            with self.assertRaises(AssertionError):
                zb.write_spec(io.StringIO())

        with zipbuilder.ZipBuilder() as zb:
            with self.assertRaises(AssertionError):
                zb.add_spec_entries([], compress_level=0, dst_prefix='base')


if __name__ == '__main__':
    unittest.main()
