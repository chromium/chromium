#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import unittest
import unittest.mock
import zipfile
_RealZipFile = zipfile.ZipFile

import dex

# pylint: disable=protected-access


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
    filters = (dex.DEFAULT_IGNORE_WARNINGS +
               ('CONTAINING_TYPE_', 'libcore', 'PublicStopClientEvent'))
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

    zip_data_1 = create_zip_data({
        'META-INF/services/foo.Bar': b'impl.Bar1\n'
    })
    zip_data_2 = create_zip_data({
        'META-INF/services/foo.Bar': b'impl.Bar2\n'
    })
    zip_data_3 = create_zip_data({
        'META-INF/services/foo.Bar': b'impl.Bar1\n'
    })
    zip_data_non_conflicting = create_zip_data({
        'META-INF/services/foo.Baz': b'impl.Baz1\n'
    })
    zip_data_cleanup_1 = create_zip_data({
        'META-INF/services/org.chromium.base.test.BaseJUnit4ClassRunner$ClassCleanupHook': b'impl.Hook1\n'
    })
    zip_data_cleanup_2 = create_zip_data({
        'META-INF/services/org.chromium.base.test.BaseJUnit4ClassRunner$ClassCleanupHook': b'impl.Hook2\n'
    })

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
    self.assertIn('Conflicting contents for: META-INF/services/foo.Bar', str(cm.exception))

    # Case 3: Conflicting content, but in merge list -> should merge
    orig_merge = dex._MERGE_SERVICE_ENTRIES
    dex._MERGE_SERVICE_ENTRIES = ('META-INF/services/foo.Bar',)
    try:
      res = dex._CreateServicesMap(['jar1.jar', 'jar2.jar'])
      self.assertEqual(res, {'META-INF/services/foo.Bar': 'impl.Bar1\nimpl.Bar2\n'})
    finally:
      dex._MERGE_SERVICE_ENTRIES = orig_merge

    # Case 4: Different services (no conflict)
    res = dex._CreateServicesMap(['jar1.jar', 'jar_baz.jar'])
    self.assertEqual(res, {
        'META-INF/services/foo.Bar': 'impl.Bar1\n',
        'META-INF/services/foo.Baz': 'impl.Baz1\n'
    })

    # Case 5: Real whitelisted service (ClassCleanupHook) -> should merge
    res = dex._CreateServicesMap(['cleanup1.jar', 'cleanup2.jar'])
    self.assertEqual(res, {
        'META-INF/services/org.chromium.base.test.BaseJUnit4ClassRunner$ClassCleanupHook':
            'impl.Hook1\nimpl.Hook2\n'
    })


if __name__ == '__main__':
  unittest.main()
