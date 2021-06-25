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
from skia_gold_common import unittest_utils

createSkiaGoldArgs = unittest_utils.createSkiaGoldArgs


def assertArgWith(test, arg_list, arg, value):
  i = arg_list.index(arg)
  test.assertEqual(arg_list[i + 1], value)


class SkiaGoldSessionRunComparisonTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSession.RunComparison."""

  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name
    with open(self._json_keys, 'w') as f:
      json.dump({}, f)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Diff')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Compare')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Initialize')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Authenticate')
  def test_comparisonSuccess(self, auth_mock, init_mock, compare_mock,
                             diff_mock):
    auth_mock.return_value = (0, None)
    init_mock.return_value = (0, None)
    compare_mock.return_value = (0, None)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, None,
                                                self._json_keys, None, None)
    status, _ = session.RunComparison(None, None, None)
    self.assertEqual(status,
                     skia_gold_session.SkiaGoldSession.StatusCodes.SUCCESS)
    self.assertEqual(auth_mock.call_count, 1)
    self.assertEqual(init_mock.call_count, 1)
    self.assertEqual(compare_mock.call_count, 1)
    self.assertEqual(diff_mock.call_count, 0)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Diff')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Compare')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Initialize')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Authenticate')
  def test_authFailure(self, auth_mock, init_mock, compare_mock, diff_mock):
    auth_mock.return_value = (1, 'Auth failed')
    session = skia_gold_session.SkiaGoldSession(self._working_dir, None,
                                                self._json_keys, None, None)
    status, error = session.RunComparison(None, None, None)
    self.assertEqual(status,
                     skia_gold_session.SkiaGoldSession.StatusCodes.AUTH_FAILURE)
    self.assertEqual(error, 'Auth failed')
    self.assertEqual(auth_mock.call_count, 1)
    self.assertEqual(init_mock.call_count, 0)
    self.assertEqual(compare_mock.call_count, 0)
    self.assertEqual(diff_mock.call_count, 0)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Diff')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Compare')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Initialize')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Authenticate')
  def test_initFailure(self, auth_mock, init_mock, compare_mock, diff_mock):
    auth_mock.return_value = (0, None)
    init_mock.return_value = (1, 'Init failed')
    session = skia_gold_session.SkiaGoldSession(self._working_dir, None,
                                                self._json_keys, None, None)
    status, error = session.RunComparison(None, None, None)
    self.assertEqual(status,
                     skia_gold_session.SkiaGoldSession.StatusCodes.INIT_FAILURE)
    self.assertEqual(error, 'Init failed')
    self.assertEqual(auth_mock.call_count, 1)
    self.assertEqual(init_mock.call_count, 1)
    self.assertEqual(compare_mock.call_count, 0)
    self.assertEqual(diff_mock.call_count, 0)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Diff')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Compare')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Initialize')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Authenticate')
  def test_compareFailureRemote(self, auth_mock, init_mock, compare_mock,
                                diff_mock):
    auth_mock.return_value = (0, None)
    init_mock.return_value = (0, None)
    compare_mock.return_value = (1, 'Compare failed')
    args = createSkiaGoldArgs(local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    status, error = session.RunComparison(None, None, None)
    self.assertEqual(
        status,
        skia_gold_session.SkiaGoldSession.StatusCodes.COMPARISON_FAILURE_REMOTE)
    self.assertEqual(error, 'Compare failed')
    self.assertEqual(auth_mock.call_count, 1)
    self.assertEqual(init_mock.call_count, 1)
    self.assertEqual(compare_mock.call_count, 1)
    self.assertEqual(diff_mock.call_count, 0)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Diff')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Compare')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Initialize')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Authenticate')
  def test_compareFailureLocal(self, auth_mock, init_mock, compare_mock,
                               diff_mock):
    auth_mock.return_value = (0, None)
    init_mock.return_value = (0, None)
    compare_mock.return_value = (1, 'Compare failed')
    diff_mock.return_value = (0, None)
    args = createSkiaGoldArgs(local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    status, error = session.RunComparison(None, None,
                                          'Definitely an output manager')
    self.assertEqual(
        status,
        skia_gold_session.SkiaGoldSession.StatusCodes.COMPARISON_FAILURE_LOCAL)
    self.assertEqual(error, 'Compare failed')
    self.assertEqual(auth_mock.call_count, 1)
    self.assertEqual(init_mock.call_count, 1)
    self.assertEqual(compare_mock.call_count, 1)
    self.assertEqual(diff_mock.call_count, 1)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Diff')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Compare')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Initialize')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Authenticate')
  def test_compareInexactMatching(self, auth_mock, init_mock, compare_mock,
                                  diff_mock):
    auth_mock.return_value = (0, None)
    init_mock.return_value = (0, None)
    compare_mock.return_value = (0, None)
    diff_mock.return_value = (0, None)
    args = createSkiaGoldArgs(local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    status, _ = session.RunComparison(None,
                                      None,
                                      None,
                                      inexact_matching_args=['--inexact'])
    self.assertEqual(status,
                     skia_gold_session.SkiaGoldSession.StatusCodes.SUCCESS)
    self.assertEqual(auth_mock.call_count, 1)
    self.assertEqual(init_mock.call_count, 1)
    self.assertEqual(compare_mock.call_count, 1)
    self.assertEqual(diff_mock.call_count, 0)
    compare_mock.assert_called_with(name=None,
                                    png_file=mock.ANY,
                                    inexact_matching_args=['--inexact'],
                                    optional_keys=None,
                                    force_dryrun=False)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Diff')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Compare')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Initialize')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Authenticate')
  def test_compareOptionalKeys(self, auth_mock, init_mock, compare_mock,
                               diff_mock):
    auth_mock.return_value = (0, None)
    init_mock.return_value = (0, None)
    compare_mock.return_value = (0, None)
    diff_mock.return_value = (0, None)
    args = createSkiaGoldArgs(local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    status, _ = session.RunComparison(None,
                                      None,
                                      None,
                                      optional_keys={'foo': 'bar'})
    self.assertEqual(status,
                     skia_gold_session.SkiaGoldSession.StatusCodes.SUCCESS)
    self.assertEqual(auth_mock.call_count, 1)
    self.assertEqual(init_mock.call_count, 1)
    self.assertEqual(compare_mock.call_count, 1)
    self.assertEqual(diff_mock.call_count, 0)
    compare_mock.assert_called_with(name=None,
                                    png_file=mock.ANY,
                                    inexact_matching_args=None,
                                    optional_keys={'foo': 'bar'},
                                    force_dryrun=False)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Diff')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Compare')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Initialize')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Authenticate')
  def test_compareForceDryrun(self, auth_mock, init_mock, compare_mock,
                              diff_mock):
    auth_mock.return_value = (0, None)
    init_mock.return_value = (0, None)
    compare_mock.return_value = (0, None)
    diff_mock.return_value = (0, None)
    args = createSkiaGoldArgs(local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    status, _ = session.RunComparison(None, None, None, force_dryrun=True)
    self.assertEqual(status,
                     skia_gold_session.SkiaGoldSession.StatusCodes.SUCCESS)
    self.assertEqual(auth_mock.call_count, 1)
    self.assertEqual(init_mock.call_count, 1)
    self.assertEqual(compare_mock.call_count, 1)
    self.assertEqual(diff_mock.call_count, 0)
    compare_mock.assert_called_with(name=None,
                                    png_file=mock.ANY,
                                    inexact_matching_args=None,
                                    optional_keys=None,
                                    force_dryrun=True)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Diff')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Compare')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Initialize')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Authenticate')
  def test_diffFailure(self, auth_mock, init_mock, compare_mock, diff_mock):
    auth_mock.return_value = (0, None)
    init_mock.return_value = (0, None)
    compare_mock.return_value = (1, 'Compare failed')
    diff_mock.return_value = (1, 'Diff failed')
    args = createSkiaGoldArgs(local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    status, error = session.RunComparison(None, None,
                                          'Definitely an output manager')
    self.assertEqual(
        status,
        skia_gold_session.SkiaGoldSession.StatusCodes.LOCAL_DIFF_FAILURE)
    self.assertEqual(error, 'Diff failed')
    self.assertEqual(auth_mock.call_count, 1)
    self.assertEqual(init_mock.call_count, 1)
    self.assertEqual(compare_mock.call_count, 1)
    self.assertEqual(diff_mock.call_count, 1)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Diff')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Compare')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Initialize')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, 'Authenticate')
  def test_noOutputManagerLocal(self, auth_mock, init_mock, compare_mock,
                                diff_mock):
    auth_mock.return_value = (0, None)
    init_mock.return_value = (0, None)
    compare_mock.return_value = (1, 'Compare failed')
    diff_mock.return_value = (0, None)
    args = createSkiaGoldArgs(local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    status, error = session.RunComparison(None, None, None)
    self.assertEqual(
        status, skia_gold_session.SkiaGoldSession.StatusCodes.NO_OUTPUT_MANAGER)
    self.assertEqual(error, 'No output manager for local diff images')
    self.assertEqual(auth_mock.call_count, 1)
    self.assertEqual(compare_mock.call_count, 1)
    self.assertEqual(diff_mock.call_count, 0)


class SkiaGoldSessionAuthenticateTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSession.Authenticate."""

  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandOutputReturned(self, cmd_mock):
    cmd_mock.return_value = (1, 'Something bad :(')
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    rc, stdout = session.Authenticate()
    self.assertEqual(cmd_mock.call_count, 1)
    self.assertEqual(rc, 1)
    self.assertEqual(stdout, 'Something bad :(')

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_bypassSkiaGoldFunctionality(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a',
                              bypass_skia_gold_functionality=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    rc, _ = session.Authenticate()
    self.assertEqual(rc, 0)
    cmd_mock.assert_not_called()

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_shortCircuitAlreadyAuthenticated(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session._authenticated = True
    rc, _ = session.Authenticate()
    self.assertEqual(rc, 0)
    cmd_mock.assert_not_called()

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_successSetsShortCircuit(self, cmd_mock):
    cmd_mock.return_value = (0, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    self.assertFalse(session._authenticated)
    rc, _ = session.Authenticate()
    self.assertEqual(rc, 0)
    self.assertTrue(session._authenticated)
    cmd_mock.assert_called_once()

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_failureDoesNotSetShortCircuit(self, cmd_mock):
    cmd_mock.return_value = (1, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    self.assertFalse(session._authenticated)
    rc, _ = session.Authenticate()
    self.assertEqual(rc, 1)
    self.assertFalse(session._authenticated)
    cmd_mock.assert_called_once()

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandWithUseLuciTrue(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session.Authenticate(use_luci=True)
    self.assertIn('--luci', cmd_mock.call_args[0][0])

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandWithUseLuciFalse(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session.Authenticate(use_luci=False)
    self.assertNotIn('--luci', cmd_mock.call_args[0][0])

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandWithUseLuciFalseNotLocal(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    with self.assertRaises(RuntimeError):
      session.Authenticate(use_luci=False)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandCommonArgs(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session.Authenticate()
    call_args = cmd_mock.call_args[0][0]
    self.assertIn('auth', call_args)
    assertArgWith(self, call_args, '--work-dir', self._working_dir)


class SkiaGoldSessionInitializeTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSession.Initialize."""

  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_bypassSkiaGoldFunctionality(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a',
                              bypass_skia_gold_functionality=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    rc, _ = session.Initialize()
    self.assertEqual(rc, 0)
    cmd_mock.assert_not_called()

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_shortCircuitAlreadyInitialized(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session._initialized = True
    rc, _ = session.Initialize()
    self.assertEqual(rc, 0)
    cmd_mock.assert_not_called()

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_successSetsShortCircuit(self, cmd_mock):
    cmd_mock.return_value = (0, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    self.assertFalse(session._initialized)
    rc, _ = session.Initialize()
    self.assertEqual(rc, 0)
    self.assertTrue(session._initialized)
    cmd_mock.assert_called_once()

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_failureDoesNotSetShortCircuit(self, cmd_mock):
    cmd_mock.return_value = (1, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    self.assertFalse(session._initialized)
    rc, _ = session.Initialize()
    self.assertEqual(rc, 1)
    self.assertFalse(session._initialized)
    cmd_mock.assert_called_once()

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandCommonArgs(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir,
                                                sgp,
                                                self._json_keys,
                                                'corpus',
                                                instance='instance',
                                                bucket='bucket')
    session.Initialize()
    call_args = cmd_mock.call_args[0][0]
    self.assertIn('imgtest', call_args)
    self.assertIn('init', call_args)
    self.assertIn('--passfail', call_args)
    assertArgWith(self, call_args, '--instance', 'instance')
    assertArgWith(self, call_args, '--bucket', 'bucket')
    assertArgWith(self, call_args, '--corpus', 'corpus')
    # The keys file should have been copied to the working directory.
    assertArgWith(self, call_args, '--keys-file',
                  os.path.join(self._working_dir, 'gold_keys.json'))
    assertArgWith(self, call_args, '--work-dir', self._working_dir)
    assertArgWith(self, call_args, '--failure-file', session._triage_link_file)
    assertArgWith(self, call_args, '--commit', 'a')

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandTryjobArgs(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a',
                              gerrit_issue=1,
                              gerrit_patchset=2,
                              buildbucket_id=3)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session.Initialize()
    call_args = cmd_mock.call_args[0][0]
    assertArgWith(self, call_args, '--issue', '1')
    assertArgWith(self, call_args, '--patchset', '2')
    assertArgWith(self, call_args, '--jobid', '3')
    assertArgWith(self, call_args, '--crs', 'gerrit')
    assertArgWith(self, call_args, '--cis', 'buildbucket')

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandTryjobArgsNonDefaultCrs(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(code_review_system='foo',
                              git_revision='a',
                              gerrit_issue=1,
                              gerrit_patchset=2,
                              buildbucket_id=3)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session.Initialize()
    call_args = cmd_mock.call_args[0][0]
    assertArgWith(self, call_args, '--issue', '1')
    assertArgWith(self, call_args, '--patchset', '2')
    assertArgWith(self, call_args, '--jobid', '3')
    assertArgWith(self, call_args, '--crs', 'foo')
    assertArgWith(self, call_args, '--cis', 'buildbucket')

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandTryjobArgsMissing(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session.Initialize()
    call_args = cmd_mock.call_args[0][0]
    self.assertNotIn('--issue', call_args)
    self.assertNotIn('--patchset', call_args)
    self.assertNotIn('--jobid', call_args)
    self.assertNotIn('--crs', call_args)
    self.assertNotIn('--cis', call_args)


class SkiaGoldSessionCompareTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSession.Compare."""

  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandOutputReturned(self, cmd_mock):
    cmd_mock.return_value = (1, 'Something bad :(')
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    rc, stdout = session.Compare(None, None)
    self.assertEqual(cmd_mock.call_count, 1)
    self.assertEqual(rc, 1)
    self.assertEqual(stdout, 'Something bad :(')

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_bypassSkiaGoldFunctionality(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a',
                              bypass_skia_gold_functionality=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    rc, _ = session.Compare(None, None)
    self.assertEqual(rc, 0)
    cmd_mock.assert_not_called()

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandWithLocalPixelTestsTrue(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session.Compare(None, None)
    self.assertIn('--dryrun', cmd_mock.call_args[0][0])

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandWithForceDryrunTrue(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session.Compare(None, None, force_dryrun=True)
    self.assertIn('--dryrun', cmd_mock.call_args[0][0])

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandWithLocalPixelTestsFalse(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session.Compare(None, None)
    self.assertNotIn('--dryrun', cmd_mock.call_args[0][0])

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandWithInexactArgs(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session.Compare(None, None, inexact_matching_args=['--inexact', 'foobar'])
    self.assertIn('--inexact', cmd_mock.call_args[0][0])
    self.assertIn('foobar', cmd_mock.call_args[0][0])

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandCommonArgs(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir,
                                                sgp,
                                                self._json_keys,
                                                'corpus',
                                                instance='instance')
    session.Compare('name', 'png_file')
    call_args = cmd_mock.call_args[0][0]
    self.assertIn('imgtest', call_args)
    self.assertIn('add', call_args)
    assertArgWith(self, call_args, '--test-name', 'name')
    assertArgWith(self, call_args, '--png-file', 'png_file')
    assertArgWith(self, call_args, '--work-dir', self._working_dir)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_noLinkOnSuccess(self, cmd_mock):
    cmd_mock.return_value = (0, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    rc, _ = session.Compare('name', 'png_file')
    self.assertEqual(rc, 0)
    comparison_result = session._comparison_results['name']
    self.assertEqual(comparison_result.public_triage_link, None)
    self.assertEqual(comparison_result.internal_triage_link, None)
    self.assertNotEqual(comparison_result.triage_link_omission_reason, None)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_clLinkOnTrybot(self, cmd_mock):
    cmd_mock.return_value = (1, None)
    args = createSkiaGoldArgs(git_revision='a',
                              gerrit_issue=1,
                              gerrit_patchset=2,
                              buildbucket_id=3)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir,
                                                sgp,
                                                self._json_keys,
                                                None,
                                                instance='instance')
    rc, _ = session.Compare('name', 'png_file')
    self.assertEqual(rc, 1)
    comparison_result = session._comparison_results['name']
    self.assertNotEqual(comparison_result.public_triage_link, None)
    self.assertNotEqual(comparison_result.internal_triage_link, None)
    internal_link = 'https://instance-gold.skia.org/cl/gerrit/1'
    public_link = 'https://instance-public-gold.skia.org/cl/gerrit/1'
    self.assertEqual(comparison_result.internal_triage_link, internal_link)
    self.assertEqual(comparison_result.public_triage_link, public_link)
    self.assertEqual(comparison_result.triage_link_omission_reason, None)
    self.assertEqual(session.GetTriageLinks('name'),
                     (public_link, internal_link))

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_individualLinkOnCi(self, cmd_mock):
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir,
                                                sgp,
                                                self._json_keys,
                                                None,
                                                instance='foobar')

    internal_link = 'foobar-gold.skia.org'
    public_link = 'foobar-public-gold.skia.org'

    def WriteTriageLinkFile(_):
      with open(session._triage_link_file, 'w') as f:
        f.write(internal_link)
      return (1, None)

    cmd_mock.side_effect = WriteTriageLinkFile
    rc, _ = session.Compare('name', 'png_file')
    self.assertEqual(rc, 1)
    comparison_result = session._comparison_results['name']
    self.assertNotEqual(comparison_result.public_triage_link, None)
    self.assertNotEqual(comparison_result.internal_triage_link, None)
    self.assertEqual(comparison_result.internal_triage_link, internal_link)
    self.assertEqual(comparison_result.public_triage_link, public_link)
    self.assertEqual(comparison_result.triage_link_omission_reason, None)
    self.assertEqual(session.GetTriageLinks('name'),
                     (public_link, internal_link))

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_validOmissionOnMissingLink(self, cmd_mock):
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)

    def WriteTriageLinkFile(_):
      with open(session._triage_link_file, 'w'):
        pass
      return (1, None)

    cmd_mock.side_effect = WriteTriageLinkFile
    rc, _ = session.Compare('name', 'png_file')
    self.assertEqual(rc, 1)
    comparison_result = session._comparison_results['name']
    self.assertEqual(comparison_result.public_triage_link, None)
    self.assertEqual(comparison_result.internal_triage_link, None)
    self.assertIn('Gold did not provide a triage link',
                  comparison_result.triage_link_omission_reason)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_validOmissionOnIoError(self, cmd_mock):
    cmd_mock.return_value = (1, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)

    def DeleteTriageLinkFile(_):
      os.remove(session._triage_link_file)
      return (1, None)

    cmd_mock.side_effect = DeleteTriageLinkFile
    rc, _ = session.Compare('name', 'png_file')
    self.assertEqual(rc, 1)
    comparison_result = session._comparison_results['name']
    self.assertEqual(comparison_result.public_triage_link, None)
    self.assertEqual(comparison_result.internal_triage_link, None)
    self.assertNotEqual(comparison_result.triage_link_omission_reason, None)
    self.assertIn('Failed to read',
                  comparison_result.triage_link_omission_reason)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_optionalKeysPassedToGoldctl(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    session.Compare(None, None, optional_keys={'foo': 'bar'})
    assertArgWith(self, cmd_mock.call_args[0][0], '--add-test-optional-key',
                  'foo:bar')


class SkiaGoldSessionDiffTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSession.Diff."""

  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_StoreDiffLinks')
  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandOutputReturned(self, cmd_mock, _):
    cmd_mock.return_value = (1, 'Something bad :(')
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    rc, stdout = session.Diff(None, None, None)
    self.assertEqual(cmd_mock.call_count, 1)
    self.assertEqual(rc, 1)
    self.assertEqual(stdout, 'Something bad :(')

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_bypassSkiaGoldFunctionality(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a',
                              bypass_skia_gold_functionality=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    with self.assertRaises(RuntimeError):
      session.Diff(None, None, None)


class SkiaGoldSessionTriageLinkOmissionTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSession.GetTriageLinkOmissionReason."""

  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()

  def _CreateSession(self):
    json_keys = tempfile.NamedTemporaryFile(delete=False).name
    session = skia_gold_session.SkiaGoldSession(self._working_dir, None,
                                                json_keys, None, None)
    session._comparison_results = {
        'foo': skia_gold_session.SkiaGoldSession.ComparisonResults(),
    }
    return session

  def test_noComparison(self):
    session = self._CreateSession()
    session._comparison_results = {}
    reason = session.GetTriageLinkOmissionReason('foo')
    self.assertEqual(reason, 'No image comparison performed for foo')

  def test_validReason(self):
    session = self._CreateSession()
    session._comparison_results['foo'].triage_link_omission_reason = 'bar'
    reason = session.GetTriageLinkOmissionReason('foo')
    self.assertEqual(reason, 'bar')

  def test_onlyLocal(self):
    session = self._CreateSession()
    session._comparison_results['foo'].local_diff_given_image = 'bar'
    reason = session.GetTriageLinkOmissionReason('foo')
    self.assertEqual(reason, 'Gold only used to do a local image diff')

  def test_onlyWithoutTriageLink(self):
    session = self._CreateSession()
    comparison_result = session._comparison_results['foo']
    comparison_result.public_triage_link = 'bar'
    with self.assertRaises(AssertionError):
      session.GetTriageLinkOmissionReason('foo')
    comparison_result.public_triage_link = None
    comparison_result.internal_triage_link = 'bar'
    with self.assertRaises(AssertionError):
      session.GetTriageLinkOmissionReason('foo')

  def test_resultsShouldNotExist(self):
    session = self._CreateSession()
    with self.assertRaises(RuntimeError):
      session.GetTriageLinkOmissionReason('foo')


if __name__ == '__main__':
  unittest.main(verbosity=2)
