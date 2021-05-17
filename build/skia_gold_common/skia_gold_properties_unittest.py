#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#pylint: disable=protected-access

import os
import sys
import unittest

if sys.version_info[0] == 2:
  import mock
else:
  import unittest.mock as mock

from skia_gold_common import skia_gold_properties
from skia_gold_common import unittest_utils

createSkiaGoldArgs = unittest_utils.createSkiaGoldArgs


class SkiaGoldPropertiesInitializationTest(unittest.TestCase):
  """Tests that SkiaGoldProperties initializes (or doesn't) when expected."""

  def verifySkiaGoldProperties(self, instance, expected):
    self.assertEqual(instance._local_pixel_tests,
                     expected.get('local_pixel_tests'))
    self.assertEqual(instance._no_luci_auth, expected.get('no_luci_auth'))
    self.assertEqual(instance._code_review_system,
                     expected.get('code_review_system'))
    self.assertEqual(instance._continuous_integration_system,
                     expected.get('continuous_integration_system'))
    self.assertEqual(instance._git_revision, expected.get('git_revision'))
    self.assertEqual(instance._issue, expected.get('gerrit_issue'))
    self.assertEqual(instance._patchset, expected.get('gerrit_patchset'))
    self.assertEqual(instance._job_id, expected.get('buildbucket_id'))
    self.assertEqual(instance._bypass_skia_gold_functionality,
                     expected.get('bypass_skia_gold_functionality'))

  def test_initializeSkiaGoldAttributes_unsetLocal(self):
    args = createSkiaGoldArgs()
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.verifySkiaGoldProperties(sgp, {})

  def test_initializeSkiaGoldAttributes_explicitLocal(self):
    args = createSkiaGoldArgs(local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.verifySkiaGoldProperties(sgp, {'local_pixel_tests': True})

  def test_initializeSkiaGoldAttributes_explicitNonLocal(self):
    args = createSkiaGoldArgs(local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.verifySkiaGoldProperties(sgp, {'local_pixel_tests': False})

  def test_initializeSkiaGoldAttributes_explicitNoLuciAuth(self):
    args = createSkiaGoldArgs(no_luci_auth=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.verifySkiaGoldProperties(sgp, {'no_luci_auth': True})

  def test_initializeSkiaGoldAttributes_explicitCrs(self):
    args = createSkiaGoldArgs(code_review_system='foo')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.verifySkiaGoldProperties(sgp, {'code_review_system': 'foo'})

  def test_initializeSkiaGoldAttributes_explicitCis(self):
    args = createSkiaGoldArgs(continuous_integration_system='foo')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.verifySkiaGoldProperties(sgp, {'continuous_integration_system': 'foo'})

  def test_initializeSkiaGoldAttributes_bypassExplicitTrue(self):
    args = createSkiaGoldArgs(bypass_skia_gold_functionality=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.verifySkiaGoldProperties(sgp, {'bypass_skia_gold_functionality': True})

  def test_initializeSkiaGoldAttributes_explicitGitRevision(self):
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.verifySkiaGoldProperties(sgp, {'git_revision': 'a'})

  def test_initializeSkiaGoldAttributes_tryjobArgsIgnoredWithoutRevision(self):
    args = createSkiaGoldArgs(gerrit_issue=1,
                              gerrit_patchset=2,
                              buildbucket_id=3)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.verifySkiaGoldProperties(sgp, {})

  def test_initializeSkiaGoldAttributes_tryjobArgs(self):
    args = createSkiaGoldArgs(git_revision='a',
                              gerrit_issue=1,
                              gerrit_patchset=2,
                              buildbucket_id=3)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.verifySkiaGoldProperties(
        sgp, {
            'git_revision': 'a',
            'gerrit_issue': 1,
            'gerrit_patchset': 2,
            'buildbucket_id': 3
        })

  def test_initializeSkiaGoldAttributes_tryjobMissingPatchset(self):
    args = createSkiaGoldArgs(git_revision='a',
                              gerrit_issue=1,
                              buildbucket_id=3)
    with self.assertRaises(RuntimeError):
      skia_gold_properties.SkiaGoldProperties(args)

  def test_initializeSkiaGoldAttributes_tryjobMissingBuildbucket(self):
    args = createSkiaGoldArgs(git_revision='a',
                              gerrit_issue=1,
                              gerrit_patchset=2)
    with self.assertRaises(RuntimeError):
      skia_gold_properties.SkiaGoldProperties(args)


class SkiaGoldPropertiesCalculationTest(unittest.TestCase):
  """Tests that SkiaGoldProperties properly calculates certain properties."""

  def testLocalPixelTests_determineTrue(self):
    args = createSkiaGoldArgs()
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    with mock.patch.dict(os.environ, {}, clear=True):
      self.assertTrue(sgp.local_pixel_tests)

  def testLocalPixelTests_determineFalse(self):
    args = createSkiaGoldArgs()
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    with mock.patch.dict(os.environ, {'SWARMING_SERVER': ''}, clear=True):
      self.assertFalse(sgp.local_pixel_tests)

  def testIsTryjobRun_noIssue(self):
    args = createSkiaGoldArgs()
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.assertFalse(sgp.IsTryjobRun())

  def testIsTryjobRun_issue(self):
    args = createSkiaGoldArgs(git_revision='a',
                              gerrit_issue=1,
                              gerrit_patchset=2,
                              buildbucket_id=3)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.assertTrue(sgp.IsTryjobRun())

  def testGetGitRevision_revisionSet(self):
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self.assertEqual(sgp.git_revision, 'a')

  def testGetGitRevision_findValidRevision(self):
    args = createSkiaGoldArgs(local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    with mock.patch.object(skia_gold_properties.SkiaGoldProperties,
                           '_GetGitOriginMasterHeadSha1') as patched_head:
      expected = 'a' * 40
      patched_head.return_value = expected
      self.assertEqual(sgp.git_revision, expected)
      # Should be cached.
      self.assertEqual(sgp._git_revision, expected)

  def testGetGitRevision_noExplicitOnBot(self):
    args = createSkiaGoldArgs(local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    with self.assertRaises(RuntimeError):
      _ = sgp.git_revision

  def testGetGitRevision_findEmptyRevision(self):
    args = createSkiaGoldArgs(local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    with mock.patch.object(skia_gold_properties.SkiaGoldProperties,
                           '_GetGitOriginMasterHeadSha1') as patched_head:
      patched_head.return_value = ''
      with self.assertRaises(RuntimeError):
        _ = sgp.git_revision

  def testGetGitRevision_findMalformedRevision(self):
    args = createSkiaGoldArgs(local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    with mock.patch.object(skia_gold_properties.SkiaGoldProperties,
                           '_GetGitOriginMasterHeadSha1') as patched_head:
      patched_head.return_value = 'a' * 39
      with self.assertRaises(RuntimeError):
        _ = sgp.git_revision


if __name__ == '__main__':
  unittest.main(verbosity=2)
