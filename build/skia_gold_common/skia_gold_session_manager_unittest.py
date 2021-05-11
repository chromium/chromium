#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#pylint: disable=protected-access

import json
import os
import sys
import tempfile
import unittest

if sys.version_info[0] == 2:
  import mock
else:
  import unittest.mock as mock

from pyfakefs import fake_filesystem_unittest

from skia_gold_common import skia_gold_properties
from skia_gold_common import skia_gold_session
from skia_gold_common import skia_gold_session_manager
from skia_gold_common import unittest_utils

createSkiaGoldArgs = unittest_utils.createSkiaGoldArgs


class SkiaGoldSessionManagerGetSessionTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSessionManager.GetSkiaGoldSession."""

  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._patcher = mock.patch.object(
        skia_gold_session_manager.SkiaGoldSessionManager, 'GetSessionClass')
    self._session_class_mock = self._patcher.start()
    self._session_class_mock.return_value = skia_gold_session.SkiaGoldSession
    self.addCleanup(self._patcher.stop)

  def test_ArgsForwardedToSession(self):
    args = createSkiaGoldArgs()
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    sgsm = skia_gold_session_manager.SkiaGoldSessionManager(
        self._working_dir, sgp)
    session = sgsm.GetSkiaGoldSession({}, 'corpus', 'instance')
    self.assertTrue(session._keys_file.startswith(self._working_dir))
    self.assertEqual(session._corpus, 'corpus')
    self.assertEqual(session._instance, 'instance')
    # Make sure the session's working directory is a subdirectory of the
    # manager's working directory.
    self.assertEqual(os.path.dirname(session._working_dir), self._working_dir)

  def test_corpusFromJson(self):
    args = createSkiaGoldArgs()
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    sgsm = skia_gold_session_manager.SkiaGoldSessionManager(
        self._working_dir, sgp)
    session = sgsm.GetSkiaGoldSession({'source_type': 'foobar'}, None,
                                      'instance')
    self.assertTrue(session._keys_file.startswith(self._working_dir))
    self.assertEqual(session._corpus, 'foobar')
    self.assertEqual(session._instance, 'instance')

  def test_corpusDefaultsToInstance(self):
    args = createSkiaGoldArgs()
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    sgsm = skia_gold_session_manager.SkiaGoldSessionManager(
        self._working_dir, sgp)
    session = sgsm.GetSkiaGoldSession({}, None, 'instance')
    self.assertTrue(session._keys_file.startswith(self._working_dir))
    self.assertEqual(session._corpus, 'instance')
    self.assertEqual(session._instance, 'instance')

  @mock.patch.object(skia_gold_session_manager.SkiaGoldSessionManager,
                     '_GetDefaultInstance')
  def test_getDefaultInstance(self, default_instance_mock):
    default_instance_mock.return_value = 'default'
    args = createSkiaGoldArgs()
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    sgsm = skia_gold_session_manager.SkiaGoldSessionManager(
        self._working_dir, sgp)
    session = sgsm.GetSkiaGoldSession({}, None, None)
    self.assertTrue(session._keys_file.startswith(self._working_dir))
    self.assertEqual(session._corpus, 'default')
    self.assertEqual(session._instance, 'default')

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '__init__')
  def test_matchingSessionReused(self, session_mock):
    session_mock.return_value = None
    args = createSkiaGoldArgs()
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    sgsm = skia_gold_session_manager.SkiaGoldSessionManager(
        self._working_dir, sgp)
    session1 = sgsm.GetSkiaGoldSession({}, 'corpus', 'instance')
    session2 = sgsm.GetSkiaGoldSession({}, 'corpus', 'instance')
    self.assertEqual(session1, session2)
    # For some reason, session_mock.assert_called_once() always passes,
    # so check the call count directly.
    self.assertEqual(session_mock.call_count, 1)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '__init__')
  def test_separateSessionsFromKeys(self, session_mock):
    session_mock.return_value = None
    args = createSkiaGoldArgs()
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    sgsm = skia_gold_session_manager.SkiaGoldSessionManager(
        self._working_dir, sgp)
    session1 = sgsm.GetSkiaGoldSession({}, 'corpus', 'instance')
    session2 = sgsm.GetSkiaGoldSession({'something_different': 1}, 'corpus',
                                       'instance')
    self.assertNotEqual(session1, session2)
    self.assertEqual(session_mock.call_count, 2)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '__init__')
  def test_separateSessionsFromCorpus(self, session_mock):
    session_mock.return_value = None
    args = createSkiaGoldArgs()
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    sgsm = skia_gold_session_manager.SkiaGoldSessionManager(
        self._working_dir, sgp)
    session1 = sgsm.GetSkiaGoldSession({}, 'corpus1', 'instance')
    session2 = sgsm.GetSkiaGoldSession({}, 'corpus2', 'instance')
    self.assertNotEqual(session1, session2)
    self.assertEqual(session_mock.call_count, 2)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '__init__')
  def test_separateSessionsFromInstance(self, session_mock):
    session_mock.return_value = None
    args = createSkiaGoldArgs()
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    self._working_dir = tempfile.mkdtemp()
    sgsm = skia_gold_session_manager.SkiaGoldSessionManager(
        self._working_dir, sgp)
    session1 = sgsm.GetSkiaGoldSession({}, 'corpus', 'instance1')
    session2 = sgsm.GetSkiaGoldSession({}, 'corpus', 'instance2')
    self.assertNotEqual(session1, session2)
    self.assertEqual(session_mock.call_count, 2)


class SkiaGoldSessionManagerKeyConversionTest(fake_filesystem_unittest.TestCase
                                              ):
  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()

  def test_getKeysAsDict(self):
    keys_dict = {'foo': 'bar'}
    keys_file_contents = {'bar': 'baz'}
    keys_file = tempfile.NamedTemporaryFile(delete=False).name
    with open(keys_file, 'w') as f:
      json.dump(keys_file_contents, f)

    self.assertEqual(skia_gold_session_manager._GetKeysAsDict(keys_dict),
                     keys_dict)
    self.assertEqual(skia_gold_session_manager._GetKeysAsDict(keys_file),
                     keys_file_contents)
    with self.assertRaises(AssertionError):
      skia_gold_session_manager._GetKeysAsDict(1)

  def test_getKeysAsJson(self):
    keys_dict = {'foo': 'bar'}
    keys_file_contents = {'bar': 'baz'}
    keys_file = tempfile.NamedTemporaryFile(delete=False).name
    with open(keys_file, 'w') as f:
      json.dump(keys_file_contents, f)

    self.assertEqual(skia_gold_session_manager._GetKeysAsJson(keys_file, None),
                     keys_file)
    keys_dict_as_json = skia_gold_session_manager._GetKeysAsJson(
        keys_dict, self._working_dir)
    self.assertTrue(keys_dict_as_json.startswith(self._working_dir))
    with open(keys_dict_as_json) as f:
      self.assertEqual(json.load(f), keys_dict)
    with self.assertRaises(AssertionError):
      skia_gold_session_manager._GetKeysAsJson(1, None)


if __name__ == '__main__':
  unittest.main(verbosity=2)
