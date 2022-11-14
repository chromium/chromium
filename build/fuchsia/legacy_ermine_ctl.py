# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Adds python interface to erminectl tools on workstation products."""

import os
import subprocess
import sys
from typing import Any, List

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))
import base_ermine_ctl


class LegacyErmineCtl(base_ermine_ctl.BaseErmineCtl):
  def __init__(self, target: Any):
    super().__init__()
    self._target = target

  def execute_command_async(self, args: List[str]) -> subprocess.Popen:
    return self._target.RunCommandPiped(args,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT,
                                        encoding='utf-8')
