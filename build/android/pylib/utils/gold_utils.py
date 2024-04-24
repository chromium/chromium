# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""//build/android implementations of //testing/skia_gold_common.

Used for interacting with the Skia Gold image diffing service.
"""

import os
import shutil
import time

from devil.utils import cmd_helper
from pylib.base.output_manager import Datatype
from pylib.constants import host_paths
from pylib.utils import repo_utils

with host_paths.SysPath(host_paths.BUILD_PATH):
  from skia_gold_common import skia_gold_session
  from skia_gold_common import skia_gold_session_manager
  from skia_gold_common import skia_gold_properties


class AndroidSkiaGoldSession(skia_gold_session.SkiaGoldSession):
  def _StoreDiffLinks(self, image_name, output_manager, output_dir):
    """See SkiaGoldSession._StoreDiffLinks for general documentation.

    |output_manager| must be a build.android.pylib.base.OutputManager instance.
    """
    given_path = closest_path = diff_path = None
    # The directory should contain "input-<hash>.png", "closest-<hash>.png",
    # and "diff.png".
    for f in os.listdir(output_dir):
      filepath = os.path.join(output_dir, f)
      if f.startswith('input-'):
        given_path = filepath
      elif f.startswith('closest-'):
        closest_path = filepath
      elif f == 'diff.png':
        diff_path = filepath
    results = self._comparison_results.setdefault(image_name,
                                                  self.ComparisonResults())
    # We include the timestamp in the PNG filename so that multiple tries from
    # the same run do not clobber each other.
    timestamp = _GetTimestamp()
    image_name = f'{image_name}_{timestamp}'
    if given_path:
      with output_manager.ArchivedTempfile('given_%s.png' % image_name,
                                           'gold_local_diffs',
                                           Datatype.PNG) as given_file:
        shutil.move(given_path, given_file.name)
      results.local_diff_given_image = given_file.Link()
    if closest_path:
      with output_manager.ArchivedTempfile('closest_%s.png' % image_name,
                                           'gold_local_diffs',
                                           Datatype.PNG) as closest_file:
        shutil.move(closest_path, closest_file.name)
      results.local_diff_closest_image = closest_file.Link()
    if diff_path:
      with output_manager.ArchivedTempfile('diff_%s.png' % image_name,
                                           'gold_local_diffs',
                                           Datatype.PNG) as diff_file:
        shutil.move(diff_path, diff_file.name)
      results.local_diff_diff_image = diff_file.Link()

  @staticmethod
  def _RunCmdForRcAndOutput(cmd):
    rc, stdout, _ = cmd_helper.GetCmdStatusOutputAndError(cmd,
                                                          merge_stderr=True)
    return rc, stdout


def _GetTimestamp():
  return time.strftime('%Y%m%dT%H%M%S-UTC', time.gmtime())


class AndroidSkiaGoldSessionManager(
    skia_gold_session_manager.SkiaGoldSessionManager):
  @staticmethod
  def GetSessionClass():
    return AndroidSkiaGoldSession


class AndroidSkiaGoldProperties(skia_gold_properties.SkiaGoldProperties):
  @staticmethod
  def _GetGitOriginMainHeadSha1():
    return repo_utils.GetGitOriginMainHeadSHA1(host_paths.DIR_SOURCE_ROOT)
