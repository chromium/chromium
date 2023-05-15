#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import dex


class DexTest(unittest.TestCase):
  def testStdErrFilter(self):
    # pylint: disable=line-too-long
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
    # pylint: enable=line-too-long
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


if __name__ == '__main__':
  unittest.main()
