# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implementation of skia_gold_session.py without output managers.

Diff output is instead stored in a directory and pointed to with file:// URLs.
"""

import os
import subprocess
import time
from typing import List, Tuple

import six

from skia_gold_common import skia_gold_session


class OutputManagerlessSkiaGoldSession(skia_gold_session.SkiaGoldSession):
  def RunComparison(self, *args, **kwargs) -> skia_gold_session.StepRetVal:
    # Passing True for the output manager is a bit of a hack, as we don't
    # actually need an output manager and just need to get past the truthy
    # check.
    assert 'output_manager' not in kwargs, 'Cannot specify output_manager'
    kwargs['output_manager'] = True
    return super().RunComparison(*args, **kwargs)

  def _CreateDiffOutputDir(self, name: str) -> str:
    # Do this instead of just making a temporary directory so that it's easier
    # for users to look through multiple results. We intentionally do not clean
    # this directory up since the user might need to look at it later.
    timestamp = int(time.time())
    name = '%s_%d' % (name, timestamp)
    filepath = os.path.join(self._local_png_directory, name)
    os.makedirs(filepath)
    return filepath

  def _StoreDiffLinks(self, image_name: str, _, output_dir: str) -> None:
    results = self._comparison_results.setdefault(image_name,
                                                  self.ComparisonResults())
    # The directory should contain "input-<hash>.png", "closest-<hash>.png",
    # and "diff.png".
    for f in os.listdir(output_dir):
      file_url = 'file://%s' % os.path.join(output_dir, f)
      if f.startswith('input-'):
        results.local_diff_given_image = file_url
      elif f.startswith('closest-'):
        results.local_diff_closest_image = file_url
      elif f == 'diff.png':
        results.local_diff_diff_image = file_url

  @staticmethod
  def _RunCmdForRcAndOutput(cmd: List[str]) -> Tuple[int, str]:
    try:
      output = subprocess.check_output(cmd,
                                       stderr=subprocess.STDOUT).decode('utf-8')
      return 0, output
    except subprocess.CalledProcessError as e:
      output = e.output
      if not isinstance(output, six.string_types):
        output = output.decode('utf-8')
      return e.returncode, output
