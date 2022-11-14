# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Defines erminctl interface compatible with modern scripts."""

import subprocess
from typing import List

from compatible_utils import get_ssh_prefix
from common import get_ssh_address
import base_ermine_ctl


class ErmineCtl(base_ermine_ctl.BaseErmineCtl):
    """ErmineCtl adaptation for modern scripts."""

    def __init__(self, target_id: str):
        super().__init__()
        self._ssh_prefix = get_ssh_prefix(get_ssh_address(target_id))

    def execute_command_async(self, args: List[str]) -> subprocess.Popen:
        return subprocess.Popen(self._ssh_prefix + args,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT,
                                encoding='utf-8')
