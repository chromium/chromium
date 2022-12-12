#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#pylint: disable=protected-access

import json
import os
import sys
import tempfile
from typing import Any
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


def assertArgWith(test: unittest.TestCase, arg_list: list, arg: Any,
                  value: Any) -> None:
  i = arg_list.index(arg)
  test.assertEqual(arg_list[i + 1], value)


class SkiaGoldSessionRunComparisonTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSession.RunComparison."""

  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name
    with open(self._json_keys, 'w') as f:
      json.dump({}, f)

    self.auth_patcher = mock.patch.object(skia_gold_session.SkiaGoldSession,
                                          'Authenticate')
    self.init_patcher = mock.patch.object(skia_gold_session.SkiaGoldSession,
                                          'Initialize')
    self.compare_patcher = mock.patch.object(skia_gold_session.SkiaGoldSession,
                                             'Compare')
    self.diff_patcher = mock.patch.object(skia_gold_session.SkiaGoldSession,
                                          'Diff')

    self.auth_mock = self.auth_patcher.start()
    self.init_mock = self.init_patcher.start()
    self.compare_mock = self.compare_patcher.start()
    self.diff_mock = self.diff_patcher.start()

    self.addCleanup(self.auth_patcher.stop)
    self.addCleanup(self.init_patcher.stop)
    self.addCleanup(self.compare_patcher.stop)
    self.addCleanup(self.diff_patcher.stop)

  def test_comparisonSuccess(self) -> None:
    self.auth_mock.return_value = (0, None)
    self.init_mock.return_value = (0, None)
    self.compare_mock.return_value = (0, None)
    sgp = skia_gold_properties.SkiaGoldProperties(createSkiaGoldArgs())
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    status, _ = session.RunComparison('', '', None)
    self.assertEqual(status,
                     skia_gold_session.SkiaGoldSession.StatusCodes.SUCCESS)
    self.assertEqual(self.auth_mock.call_count, 1)
    self.assertEqual(self.init_mock.call_count, 1)
    self.assertEqual(self.compare_mock.call_count, 1)
    self.assertEqual(self.diff_mock.call_count, 0)

  def test_authFailure(self) -> None:
    self.auth_mock.return_value = (1, 'Auth failed')
    sgp = skia_gold_properties.SkiaGoldProperties(createSkiaGoldArgs())
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    status, error = session.RunComparison('', '', None)
    self.assertEqual(status,
                     skia_gold_session.SkiaGoldSession.StatusCodes.AUTH_FAILURE)
    self.assertEqual(error, 'Auth failed')
    self.assertEqual(self.auth_mock.call_count, 1)
    self.assertEqual(self.init_mock.call_count, 0)
    self.assertEqual(self.compare_mock.call_count, 0)
    self.assertEqual(self.diff_mock.call_count, 0)

  def test_initFailure(self) -> None:
    self.auth_mock.return_value = (0, None)
    self.init_mock.return_value = (1, 'Init failed')
    sgp = skia_gold_properties.SkiaGoldProperties(createSkiaGoldArgs())
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    status, error = session.RunComparison('', '', None)
    self.assertEqual(status,
                     skia_gold_session.SkiaGoldSession.StatusCodes.INIT_FAILURE)
    self.assertEqual(error, 'Init failed')
    self.assertEqual(self.auth_mock.call_count, 1)
    self.assertEqual(self.init_mock.call_count, 1)
    self.assertEqual(self.compare_mock.call_count, 0)
    self.assertEqual(self.diff_mock.call_count, 0)

  def test_compareFailureRemote(self) -> None:
    self.auth_mock.return_value = (0, None)
    self.init_mock.return_value = (0, None)
    self.compare_mock.return_value = (1, 'Compare failed')
    args = createSkiaGoldArgs(local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    status, error = session.RunComparison('', '', None)
    self.assertEqual(
        status,
        skia_gold_session.SkiaGoldSession.StatusCodes.COMPARISON_FAILURE_REMOTE)
    self.assertEqual(error, 'Compare failed')
    self.assertEqual(self.auth_mock.call_count, 1)
    self.assertEqual(self.init_mock.call_count, 1)
    self.assertEqual(self.compare_mock.call_count, 1)
    self.assertEqual(self.diff_mock.call_count, 0)

  def test_compareFailureLocal(self) -> None:
    self.auth_mock.return_value = (0, None)
    self.init_mock.return_value = (0, None)
    self.compare_mock.return_value = (1, 'Compare failed')
    self.diff_mock.return_value = (0, None)
    args = createSkiaGoldArgs(local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    status, error = session.RunComparison('', '',
                                          'Definitely an output manager')
    self.assertEqual(
        status,
        skia_gold_session.SkiaGoldSession.StatusCodes.COMPARISON_FAILURE_LOCAL)
    self.assertEqual(error, 'Compare failed')
    self.assertEqual(self.auth_mock.call_count, 1)
    self.assertEqual(self.init_mock.call_count, 1)
    self.assertEqual(self.compare_mock.call_count, 1)
    self.assertEqual(self.diff_mock.call_count, 1)

  def test_compareInexactMatching(self) -> None:
    self.auth_mock.return_value = (0, None)
    self.init_mock.return_value = (0, None)
    self.compare_mock.return_value = (0, None)
    self.diff_mock.return_value = (0, None)
    args = createSkiaGoldArgs(local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    status, _ = session.RunComparison('',
                                      '',
                                      None,
                                      inexact_matching_args=['--inexact'])
    self.assertEqual(status,
                     skia_gold_session.SkiaGoldSession.StatusCodes.SUCCESS)
    self.assertEqual(self.auth_mock.call_count, 1)
    self.assertEqual(self.init_mock.call_count, 1)
    self.assertEqual(self.compare_mock.call_count, 1)
    self.assertEqual(self.diff_mock.call_count, 0)
    self.compare_mock.assert_called_with(name='',
                                         png_file=mock.ANY,
                                         inexact_matching_args=['--inexact'],
                                         optional_keys=None,
                                         force_dryrun=False)

  def test_compareOptionalKeys(self) -> None:
    self.auth_mock.return_value = (0, None)
    self.init_mock.return_value = (0, None)
    self.compare_mock.return_value = (0, None)
    self.diff_mock.return_value = (0, None)
    args = createSkiaGoldArgs(local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    status, _ = session.RunComparison('',
                                      '',
                                      None,
                                      optional_keys={'foo': 'bar'})
    self.assertEqual(status,
                     skia_gold_session.SkiaGoldSession.StatusCodes.SUCCESS)
    self.assertEqual(self.auth_mock.call_count, 1)
    self.assertEqual(self.init_mock.call_count, 1)
    self.assertEqual(self.compare_mock.call_count, 1)
    self.assertEqual(self.diff_mock.call_count, 0)
    self.compare_mock.assert_called_with(name='',
                                         png_file=mock.ANY,
                                         inexact_matching_args=None,
                                         optional_keys={'foo': 'bar'},
                                         force_dryrun=False)

  def test_compareForceDryrun(self) -> None:
    self.auth_mock.return_value = (0, None)
    self.init_mock.return_value = (0, None)
    self.compare_mock.return_value = (0, None)
    self.diff_mock.return_value = (0, None)
    args = createSkiaGoldArgs(local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    status, _ = session.RunComparison('', '', None, force_dryrun=True)
    self.assertEqual(status,
                     skia_gold_session.SkiaGoldSession.StatusCodes.SUCCESS)
    self.assertEqual(self.auth_mock.call_count, 1)
    self.assertEqual(self.init_mock.call_count, 1)
    self.assertEqual(self.compare_mock.call_count, 1)
    self.assertEqual(self.diff_mock.call_count, 0)
    self.compare_mock.assert_called_with(name='',
                                         png_file=mock.ANY,
                                         inexact_matching_args=None,
                                         optional_keys=None,
                                         force_dryrun=True)

  def test_diffFailure(self) -> None:
    self.auth_mock.return_value = (0, None)
    self.init_mock.return_value = (0, None)
    self.compare_mock.return_value = (1, 'Compare failed')
    self.diff_mock.return_value = (1, 'Diff failed')
    args = createSkiaGoldArgs(local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    status, error = session.RunComparison('', '',
                                          'Definitely an output manager')
    self.assertEqual(
        status,
        skia_gold_session.SkiaGoldSession.StatusCodes.LOCAL_DIFF_FAILURE)
    self.assertEqual(error, 'Diff failed')
    self.assertEqual(self.auth_mock.call_count, 1)
    self.assertEqual(self.init_mock.call_count, 1)
    self.assertEqual(self.compare_mock.call_count, 1)
    self.assertEqual(self.diff_mock.call_count, 1)

  def test_noOutputManagerLocal(self) -> None:
    self.auth_mock.return_value = (0, None)
    self.init_mock.return_value = (0, None)
    self.compare_mock.return_value = (1, 'Compare failed')
    self.diff_mock.return_value = (0, None)
    args = createSkiaGoldArgs(local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    status, error = session.RunComparison('', '', None)
    self.assertEqual(
        status, skia_gold_session.SkiaGoldSession.StatusCodes.NO_OUTPUT_MANAGER)
    self.assertEqual(error, 'No output manager for local diff images')
    self.assertEqual(self.auth_mock.call_count, 1)
    self.assertEqual(self.compare_mock.call_count, 1)
    self.assertEqual(self.diff_mock.call_count, 0)


class SkiaGoldSessionAuthenticateTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSession.Authenticate."""

  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name

    self.cmd_patcher = mock.patch.object(skia_gold_session.SkiaGoldSession,
                                         '_RunCmdForRcAndOutput')
    self.cmd_mock = self.cmd_patcher.start()
    self.addCleanup(self.cmd_patcher.stop)

  def test_commandOutputReturned(self) -> None:
    self.cmd_mock.return_value = (1, 'Something bad :(')
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    rc, stdout = session.Authenticate()
    self.assertEqual(self.cmd_mock.call_count, 1)
    self.assertEqual(rc, 1)
    self.assertEqual(stdout, 'Something bad :(')

  def test_bypassSkiaGoldFunctionality(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a',
                              bypass_skia_gold_functionality=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    rc, _ = session.Authenticate()
    self.assertEqual(rc, 0)
    self.cmd_mock.assert_not_called()

  def test_shortCircuitAlreadyAuthenticated(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session._authenticated = True
    rc, _ = session.Authenticate()
    self.assertEqual(rc, 0)
    self.cmd_mock.assert_not_called()

  def test_successSetsShortCircuit(self) -> None:
    self.cmd_mock.return_value = (0, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    self.assertFalse(session._authenticated)
    rc, _ = session.Authenticate()
    self.assertEqual(rc, 0)
    self.assertTrue(session._authenticated)
    self.cmd_mock.assert_called_once()

  def test_failureDoesNotSetShortCircuit(self) -> None:
    self.cmd_mock.return_value = (1, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    self.assertFalse(session._authenticated)
    rc, _ = session.Authenticate()
    self.assertEqual(rc, 1)
    self.assertFalse(session._authenticated)
    self.cmd_mock.assert_called_once()

  def test_commandWithUseLuciTrue(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session.Authenticate(use_luci=True)
    self.assertIn('--luci', self.cmd_mock.call_args[0][0])

  def test_commandWithUseLuciFalse(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session.Authenticate(use_luci=False)
    self.assertNotIn('--luci', self.cmd_mock.call_args[0][0])

  def test_commandWithUseLuciFalseNotLocal(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    with self.assertRaises(RuntimeError):
      session.Authenticate(use_luci=False)

  def test_commandWithUseLuciAndServiceAccount(self) -> None:
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    with self.assertRaises(AssertionError):
      session.Authenticate(use_luci=True, service_account='a')

  def test_commandWithServiceAccount(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session.Authenticate(use_luci=False, service_account='service_account')
    call_args = self.cmd_mock.call_args[0][0]
    self.assertNotIn('--luci', call_args)
    assertArgWith(self, call_args, '--service-account', 'service_account')

  def test_commandCommonArgs(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session.Authenticate()
    call_args = self.cmd_mock.call_args[0][0]
    self.assertIn('auth', call_args)
    assertArgWith(self, call_args, '--work-dir', self._working_dir)


class SkiaGoldSessionInitializeTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSession.Initialize."""

  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name

    self.cmd_patcher = mock.patch.object(skia_gold_session.SkiaGoldSession,
                                         '_RunCmdForRcAndOutput')
    self.cmd_mock = self.cmd_patcher.start()
    self.addCleanup(self.cmd_patcher.stop)

  def test_bypassSkiaGoldFunctionality(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a',
                              bypass_skia_gold_functionality=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    rc, _ = session.Initialize()
    self.assertEqual(rc, 0)
    self.cmd_mock.assert_not_called()

  def test_shortCircuitAlreadyInitialized(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session._initialized = True
    rc, _ = session.Initialize()
    self.assertEqual(rc, 0)
    self.cmd_mock.assert_not_called()

  def test_successSetsShortCircuit(self) -> None:
    self.cmd_mock.return_value = (0, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    self.assertFalse(session._initialized)
    rc, _ = session.Initialize()
    self.assertEqual(rc, 0)
    self.assertTrue(session._initialized)
    self.cmd_mock.assert_called_once()

  def test_failureDoesNotSetShortCircuit(self) -> None:
    self.cmd_mock.return_value = (1, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    self.assertFalse(session._initialized)
    rc, _ = session.Initialize()
    self.assertEqual(rc, 1)
    self.assertFalse(session._initialized)
    self.cmd_mock.assert_called_once()

  def test_commandCommonArgs(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir,
                                                sgp,
                                                self._json_keys,
                                                'corpus',
                                                instance='instance',
                                                bucket='bucket')
    session.Initialize()
    call_args = self.cmd_mock.call_args[0][0]
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

  def test_commandTryjobArgs(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a',
                              gerrit_issue=1,
                              gerrit_patchset=2,
                              buildbucket_id=3)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session.Initialize()
    call_args = self.cmd_mock.call_args[0][0]
    assertArgWith(self, call_args, '--issue', '1')
    assertArgWith(self, call_args, '--patchset', '2')
    assertArgWith(self, call_args, '--jobid', '3')
    assertArgWith(self, call_args, '--crs', 'gerrit')
    assertArgWith(self, call_args, '--cis', 'buildbucket')

  def test_commandTryjobArgsNonDefaultCrs(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(code_review_system='foo',
                              git_revision='a',
                              gerrit_issue=1,
                              gerrit_patchset=2,
                              buildbucket_id=3)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session.Initialize()
    call_args = self.cmd_mock.call_args[0][0]
    assertArgWith(self, call_args, '--issue', '1')
    assertArgWith(self, call_args, '--patchset', '2')
    assertArgWith(self, call_args, '--jobid', '3')
    assertArgWith(self, call_args, '--crs', 'foo')
    assertArgWith(self, call_args, '--cis', 'buildbucket')

  def test_commandTryjobArgsMissing(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session.Initialize()
    call_args = self.cmd_mock.call_args[0][0]
    self.assertNotIn('--issue', call_args)
    self.assertNotIn('--patchset', call_args)
    self.assertNotIn('--jobid', call_args)
    self.assertNotIn('--crs', call_args)
    self.assertNotIn('--cis', call_args)


class SkiaGoldSessionCompareTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSession.Compare."""

  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name

    self.cmd_patcher = mock.patch.object(skia_gold_session.SkiaGoldSession,
                                         '_RunCmdForRcAndOutput')
    self.cmd_mock = self.cmd_patcher.start()
    self.addCleanup(self.cmd_patcher.stop)

  def test_commandOutputReturned(self) -> None:
    self.cmd_mock.return_value = (1, 'Something bad :(')
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    rc, stdout = session.Compare('', '')
    self.assertEqual(self.cmd_mock.call_count, 1)
    self.assertEqual(rc, 1)
    self.assertEqual(stdout, 'Something bad :(')

  def test_bypassSkiaGoldFunctionality(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a',
                              bypass_skia_gold_functionality=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    rc, _ = session.Compare('', '')
    self.assertEqual(rc, 0)
    self.cmd_mock.assert_not_called()

  def test_commandWithLocalPixelTestsTrue(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session.Compare('', '')
    self.assertIn('--dryrun', self.cmd_mock.call_args[0][0])

  def test_commandWithForceDryrunTrue(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session.Compare('', '', force_dryrun=True)
    self.assertIn('--dryrun', self.cmd_mock.call_args[0][0])

  def test_commandWithLocalPixelTestsFalse(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session.Compare('', '')
    self.assertNotIn('--dryrun', self.cmd_mock.call_args[0][0])

  def test_commandWithInexactArgs(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session.Compare('', '', inexact_matching_args=['--inexact', 'foobar'])
    self.assertIn('--inexact', self.cmd_mock.call_args[0][0])
    self.assertIn('foobar', self.cmd_mock.call_args[0][0])

  def test_commandCommonArgs(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir,
                                                sgp,
                                                self._json_keys,
                                                'corpus',
                                                instance='instance')
    session.Compare('name', 'png_file')
    call_args = self.cmd_mock.call_args[0][0]
    self.assertIn('imgtest', call_args)
    self.assertIn('add', call_args)
    assertArgWith(self, call_args, '--test-name', 'name')
    assertArgWith(self, call_args, '--png-file', 'png_file')
    assertArgWith(self, call_args, '--work-dir', self._working_dir)

  def test_noLinkOnSuccess(self) -> None:
    self.cmd_mock.return_value = (0, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    rc, _ = session.Compare('name', 'png_file')
    self.assertEqual(rc, 0)
    comparison_result = session._comparison_results['name']
    self.assertEqual(comparison_result.public_triage_link, None)
    self.assertEqual(comparison_result.internal_triage_link, None)
    self.assertNotEqual(comparison_result.triage_link_omission_reason, None)

  def test_clLinkOnTrybot(self) -> None:
    self.cmd_mock.return_value = (1, None)
    args = createSkiaGoldArgs(git_revision='a',
                              gerrit_issue=1,
                              gerrit_patchset=2,
                              buildbucket_id=3)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir,
                                                sgp,
                                                self._json_keys,
                                                '',
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

  def test_individualLinkOnCi(self) -> None:
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir,
                                                sgp,
                                                self._json_keys,
                                                '',
                                                instance='foobar')

    internal_link = 'foobar-gold.skia.org'
    public_link = 'foobar-public-gold.skia.org'

    def WriteTriageLinkFile(_):
      with open(session._triage_link_file, 'w') as f:
        f.write(internal_link)
      return (1, None)

    self.cmd_mock.side_effect = WriteTriageLinkFile
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

  def test_validOmissionOnMissingLink(self) -> None:
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')

    def WriteTriageLinkFile(_):
      with open(session._triage_link_file, 'w'):
        pass
      return (1, None)

    self.cmd_mock.side_effect = WriteTriageLinkFile
    rc, _ = session.Compare('name', 'png_file')
    self.assertEqual(rc, 1)
    comparison_result = session._comparison_results['name']
    self.assertEqual(comparison_result.public_triage_link, None)
    self.assertEqual(comparison_result.internal_triage_link, None)
    self.assertIn('Gold did not provide a triage link',
                  comparison_result.triage_link_omission_reason)

  def test_validOmissionOnIoError(self) -> None:
    self.cmd_mock.return_value = (1, None)
    args = createSkiaGoldArgs(git_revision='a')
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')

    def DeleteTriageLinkFile(_):
      os.remove(session._triage_link_file)
      return (1, None)

    self.cmd_mock.side_effect = DeleteTriageLinkFile
    rc, _ = session.Compare('name', 'png_file')
    self.assertEqual(rc, 1)
    comparison_result = session._comparison_results['name']
    self.assertEqual(comparison_result.public_triage_link, None)
    self.assertEqual(comparison_result.internal_triage_link, None)
    self.assertNotEqual(comparison_result.triage_link_omission_reason, None)
    self.assertIn('Failed to read',
                  comparison_result.triage_link_omission_reason)

  def test_optionalKeysPassedToGoldctl(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    session.Compare('', '', optional_keys={'foo': 'bar'})
    assertArgWith(self, self.cmd_mock.call_args[0][0],
                  '--add-test-optional-key', 'foo:bar')


class SkiaGoldSessionDiffTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSession.Diff."""

  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name

    self.cmd_patcher = mock.patch.object(skia_gold_session.SkiaGoldSession,
                                         '_RunCmdForRcAndOutput')
    self.cmd_mock = self.cmd_patcher.start()
    self.addCleanup(self.cmd_patcher.stop)

  @mock.patch.object(skia_gold_session.SkiaGoldSession, '_StoreDiffLinks')
  def test_commandOutputReturned(self, _) -> None:
    self.cmd_mock.return_value = (1, 'Something bad :(')
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    rc, stdout = session.Diff('', '', None)
    self.assertEqual(self.cmd_mock.call_count, 1)
    self.assertEqual(rc, 1)
    self.assertEqual(stdout, 'Something bad :(')

  def test_bypassSkiaGoldFunctionality(self) -> None:
    self.cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a',
                              bypass_skia_gold_functionality=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, '', '')
    with self.assertRaises(RuntimeError):
      session.Diff('', '', None)


class SkiaGoldSessionTriageLinkOmissionTest(fake_filesystem_unittest.TestCase):
  """Tests the functionality of SkiaGoldSession.GetTriageLinkOmissionReason."""

  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()

  def _CreateSession(self) -> skia_gold_session.SkiaGoldSession:
    sgp = skia_gold_properties.SkiaGoldProperties(createSkiaGoldArgs())
    json_keys = tempfile.NamedTemporaryFile(delete=False).name
    session = skia_gold_session.SkiaGoldSession(self._working_dir, sgp,
                                                json_keys, '', '')
    session._comparison_results = {
        'foo': skia_gold_session.SkiaGoldSession.ComparisonResults(),
    }
    return session

  def test_noComparison(self) -> None:
    session = self._CreateSession()
    session._comparison_results = {}
    reason = session.GetTriageLinkOmissionReason('foo')
    self.assertEqual(reason, 'No image comparison performed for foo')

  def test_validReason(self) -> None:
    session = self._CreateSession()
    session._comparison_results['foo'].triage_link_omission_reason = 'bar'
    reason = session.GetTriageLinkOmissionReason('foo')
    self.assertEqual(reason, 'bar')

  def test_onlyLocal(self) -> None:
    session = self._CreateSession()
    session._comparison_results['foo'].local_diff_given_image = 'bar'
    reason = session.GetTriageLinkOmissionReason('foo')
    self.assertEqual(reason, 'Gold only used to do a local image diff')

  def test_onlyWithoutTriageLink(self) -> None:
    session = self._CreateSession()
    comparison_result = session._comparison_results['foo']
    comparison_result.public_triage_link = 'bar'
    with self.assertRaises(AssertionError):
      session.GetTriageLinkOmissionReason('foo')
    comparison_result.public_triage_link = None
    comparison_result.internal_triage_link = 'bar'
    with self.assertRaises(AssertionError):
      session.GetTriageLinkOmissionReason('foo')

  def test_resultsShouldNotExist(self) -> None:
    session = self._CreateSession()
    with self.assertRaises(RuntimeError):
      session.GetTriageLinkOmissionReason('foo')


if __name__ == '__main__':
  unittest.main(verbosity=2)
