#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for gold_utils."""

#pylint: disable=protected-access

import contextlib
import os
import tempfile
import unittest

from pylib.constants import host_paths
from pylib.utils import gold_utils

with host_paths.SysPath(host_paths.BUILD_PATH):
  from skia_gold_common import unittest_utils

import mock  # pylint: disable=import-error
from pyfakefs import fake_filesystem_unittest  # pylint: disable=import-error

createSkiaGoldArgs = unittest_utils.createSkiaGoldArgs


def assertArgWith(test, arg_list, arg, value):
  i = arg_list.index(arg)
  test.assertEqual(arg_list[i + 1], value)


class AndroidSkiaGoldSessionDiffTest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name

  @mock.patch.object(gold_utils.AndroidSkiaGoldSession, '_RunCmdForRcAndOutput')
  def test_commandCommonArgs(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=False)
    sgp = gold_utils.AndroidSkiaGoldProperties(args)
    session = gold_utils.AndroidSkiaGoldSession(self._working_dir,
                                                sgp,
                                                self._json_keys,
                                                'corpus',
                                                instance='instance')
    session.Diff('name', 'png_file', None)
    call_args = cmd_mock.call_args[0][0]
    self.assertIn('diff', call_args)
    assertArgWith(self, call_args, '--corpus', 'corpus')
    # TODO(skbug.com/10610): Remove the -public once we go back to using the
    # non-public instance, or add a second test for testing that the correct
    # instance is chosen if we decide to support both depending on what the
    # user is authenticated for.
    assertArgWith(self, call_args, '--instance', 'instance-public')
    assertArgWith(self, call_args, '--input', 'png_file')
    assertArgWith(self, call_args, '--test', 'name')
    # TODO(skbug.com/10611): Re-add this assert and remove the check for the
    # absence of the directory once we switch back to using the proper working
    # directory.
    # assertArgWith(self, call_args, '--work-dir', self._working_dir)
    self.assertNotIn(self._working_dir, call_args)
    i = call_args.index('--out-dir')
    # The output directory should be a subdirectory of the working directory.
    self.assertIn(self._working_dir, call_args[i + 1])


class AndroidSkiaGoldSessionDiffLinksTest(fake_filesystem_unittest.TestCase):
  class FakeArchivedFile:
    def __init__(self, path):
      self.name = path

    def Link(self):
      return 'file://' + self.name

  class FakeOutputManager:
    def __init__(self):
      self.output_dir = tempfile.mkdtemp()

    @contextlib.contextmanager
    def ArchivedTempfile(self, image_name, _, __):
      filepath = os.path.join(self.output_dir, image_name)
      yield AndroidSkiaGoldSessionDiffLinksTest.FakeArchivedFile(filepath)

  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name
    self._timestamp_patcher = mock.patch.object(gold_utils, '_GetTimestamp')
    self._timestamp_mock = self._timestamp_patcher.start()
    self.addCleanup(self._timestamp_patcher.stop)

  def test_outputManagerUsed(self):
    self._timestamp_mock.return_value = 'ts0'
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=True)
    sgp = gold_utils.AndroidSkiaGoldProperties(args)
    session = gold_utils.AndroidSkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    with open(os.path.join(self._working_dir, 'input-inputhash.png'), 'w') as f:
      f.write('input')
    with open(os.path.join(self._working_dir, 'closest-closesthash.png'),
              'w') as f:
      f.write('closest')
    with open(os.path.join(self._working_dir, 'diff.png'), 'w') as f:
      f.write('diff')

    output_manager = AndroidSkiaGoldSessionDiffLinksTest.FakeOutputManager()
    session._StoreDiffLinks('foo', output_manager, self._working_dir)

    copied_input = os.path.join(output_manager.output_dir, 'given_foo_ts0.png')
    copied_closest = os.path.join(output_manager.output_dir,
                                  'closest_foo_ts0.png')
    copied_diff = os.path.join(output_manager.output_dir, 'diff_foo_ts0.png')
    with open(copied_input) as f:
      self.assertEqual(f.read(), 'input')
    with open(copied_closest) as f:
      self.assertEqual(f.read(), 'closest')
    with open(copied_diff) as f:
      self.assertEqual(f.read(), 'diff')

    self.assertEqual(session.GetGivenImageLink('foo'), 'file://' + copied_input)
    self.assertEqual(session.GetClosestImageLink('foo'),
                     'file://' + copied_closest)
    self.assertEqual(session.GetDiffImageLink('foo'), 'file://' + copied_diff)

  def test_diffLinksDoNotClobber(self):
    """Tests that multiple calls to store links does not clobber files."""

    def side_effect():
      side_effect.count += 1
      return f'ts{side_effect.count}'

    side_effect.count = -1
    self._timestamp_mock.side_effect = side_effect

    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=True)
    sgp = gold_utils.AndroidSkiaGoldProperties(args)
    session = gold_utils.AndroidSkiaGoldSession(self._working_dir, sgp,
                                                self._json_keys, None, None)
    with open(os.path.join(self._working_dir, 'input-inputhash.png'), 'w') as f:
      f.write('first input')
    with open(os.path.join(self._working_dir, 'closest-closesthash.png'),
              'w') as f:
      f.write('first closest')
    with open(os.path.join(self._working_dir, 'diff.png'), 'w') as f:
      f.write('first diff')

    output_manager = AndroidSkiaGoldSessionDiffLinksTest.FakeOutputManager()
    session._StoreDiffLinks('foo', output_manager, self._working_dir)

    # Store links normally once.
    first_copied_input = os.path.join(output_manager.output_dir,
                                      'given_foo_ts0.png')
    first_copied_closest = os.path.join(output_manager.output_dir,
                                        'closest_foo_ts0.png')
    first_copied_diff = os.path.join(output_manager.output_dir,
                                     'diff_foo_ts0.png')
    with open(first_copied_input) as f:
      self.assertEqual(f.read(), 'first input')
    with open(first_copied_closest) as f:
      self.assertEqual(f.read(), 'first closest')
    with open(first_copied_diff) as f:
      self.assertEqual(f.read(), 'first diff')

    with open(os.path.join(self._working_dir, 'input-inputhash.png'), 'w') as f:
      f.write('second input')
    with open(os.path.join(self._working_dir, 'closest-closesthash.png'),
              'w') as f:
      f.write('second closest')
    with open(os.path.join(self._working_dir, 'diff.png'), 'w') as f:
      f.write('second diff')

    self.assertEqual(session.GetGivenImageLink('foo'),
                     'file://' + first_copied_input)
    self.assertEqual(session.GetClosestImageLink('foo'),
                     'file://' + first_copied_closest)
    self.assertEqual(session.GetDiffImageLink('foo'),
                     'file://' + first_copied_diff)

    # Store links again and check that the new data is surfaced.
    session._StoreDiffLinks('foo', output_manager, self._working_dir)

    second_copied_input = os.path.join(output_manager.output_dir,
                                       'given_foo_ts1.png')
    second_copied_closest = os.path.join(output_manager.output_dir,
                                         'closest_foo_ts1.png')
    second_copied_diff = os.path.join(output_manager.output_dir,
                                      'diff_foo_ts1.png')
    with open(second_copied_input) as f:
      self.assertEqual(f.read(), 'second input')
    with open(second_copied_closest) as f:
      self.assertEqual(f.read(), 'second closest')
    with open(second_copied_diff) as f:
      self.assertEqual(f.read(), 'second diff')

    self.assertEqual(session.GetGivenImageLink('foo'),
                     'file://' + second_copied_input)
    self.assertEqual(session.GetClosestImageLink('foo'),
                     'file://' + second_copied_closest)
    self.assertEqual(session.GetDiffImageLink('foo'),
                     'file://' + second_copied_diff)

    # Check to make sure the first images still exist on disk and are unchanged.
    with open(first_copied_input) as f:
      self.assertEqual(f.read(), 'first input')
    with open(first_copied_closest) as f:
      self.assertEqual(f.read(), 'first closest')
    with open(first_copied_diff) as f:
      self.assertEqual(f.read(), 'first diff')


if __name__ == '__main__':
  unittest.main(verbosity=2)
