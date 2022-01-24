#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#pylint: disable=protected-access

import os
import re
import sys
import tempfile
import unittest

import six

if six.PY2:
  import mock
else:
  import unittest.mock as mock

from pyfakefs import fake_filesystem_unittest

from skia_gold_common import output_managerless_skia_gold_session as omsgs
from skia_gold_common import skia_gold_properties
from skia_gold_common import unittest_utils

createSkiaGoldArgs = unittest_utils.createSkiaGoldArgs


def assertArgWith(test, arg_list, arg, value):
  i = arg_list.index(arg)
  test.assertEqual(arg_list[i + 1], value)


class GpuSkiaGoldSessionDiffTest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name

  @mock.patch.object(omsgs.OutputManagerlessSkiaGoldSession,
                     '_RunCmdForRcAndOutput')
  def test_commandCommonArgs(self, cmd_mock):
    cmd_mock.return_value = (None, None)
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=False)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = omsgs.OutputManagerlessSkiaGoldSession(self._working_dir,
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
    # The output directory should not be a subdirectory of the working
    # directory.
    self.assertNotIn(self._working_dir, call_args[i + 1])

  @mock.patch.object(omsgs.OutputManagerlessSkiaGoldSession, '_StoreDiffLinks')
  @mock.patch.object(omsgs.OutputManagerlessSkiaGoldSession,
                     '_RunCmdForRcAndOutput')
  def test_explicitLocalPngDirectory(self, cmd_mock, _):
    cmd_mock.return_value = (0, '')
    if sys.platform == 'win32':
      local_png_dir = 'c:\\tmp\\foo'
    else:
      local_png_dir = '/tmp/foo'
    args = createSkiaGoldArgs(git_revision='a',
                              skia_gold_local_png_write_directory=local_png_dir)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = omsgs.OutputManagerlessSkiaGoldSession(self._working_dir, sgp,
                                                     self._json_keys, None,
                                                     None)
    _, _ = session.Diff('name', None, None)
    self.assertEqual(cmd_mock.call_count, 1)
    if six.PY3:
      call_args = cmd_mock.call_args.args[0]
    else:
      call_args = cmd_mock.call_args[0][0]
    self.assertIn('--out-dir', call_args)
    output_dir = call_args[call_args.index('--out-dir') + 1]
    # Directory should be a subdirectory of the directory we gave and be made
    # up of the image name and a timestamp.
    parent_dir, sub_dir = output_dir.rsplit(os.sep, 1)
    self.assertEqual(parent_dir, local_png_dir)
    sub_dir = os.path.normpath(sub_dir)
    self.assertIsNotNone(re.match(r'^name_\d+$', sub_dir))


class OutputManagerlessSkiaGoldSessionStoreDiffLinksTest(
    fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self._working_dir = tempfile.mkdtemp()
    self._json_keys = tempfile.NamedTemporaryFile(delete=False).name

  def test_outputManagerNotNeeded(self):
    args = createSkiaGoldArgs(git_revision='a', local_pixel_tests=True)
    sgp = skia_gold_properties.SkiaGoldProperties(args)
    session = omsgs.OutputManagerlessSkiaGoldSession(self._working_dir, sgp,
                                                     self._json_keys, None,
                                                     None)
    input_filepath = os.path.join(self._working_dir, 'input-inputhash.png')
    with open(input_filepath, 'w') as f:
      f.write('')
    closest_filepath = os.path.join(self._working_dir,
                                    'closest-closesthash.png')
    with open(closest_filepath, 'w') as f:
      f.write('')
    diff_filepath = os.path.join(self._working_dir, 'diff.png')
    with open(diff_filepath, 'w') as f:
      f.write('')

    session._StoreDiffLinks('foo', None, self._working_dir)
    self.assertEqual(session.GetGivenImageLink('foo'),
                     'file://' + input_filepath)
    self.assertEqual(session.GetClosestImageLink('foo'),
                     'file://' + closest_filepath)
    self.assertEqual(session.GetDiffImageLink('foo'), 'file://' + diff_filepath)


if __name__ == '__main__':
  unittest.main(verbosity=2)
