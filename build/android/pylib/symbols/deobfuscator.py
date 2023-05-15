# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from pylib import constants
from .expensive_line_transformer import ExpensiveLineTransformer
from .expensive_line_transformer import ExpensiveLineTransformerPool

_MINIMUM_TIMEOUT = 10.0
_PER_LINE_TIMEOUT = .005  # Should be able to process 200 lines per second.
_PROCESS_START_TIMEOUT = 20.0
_MAX_RESTARTS = 4  # Should be plenty unless tool is crashing on start-up.
_POOL_SIZE = 4
_PASSTHROUH_ON_FAILURE = False


class Deobfuscator(ExpensiveLineTransformer):
  def __init__(self, mapping_path):
    super().__init__(_PROCESS_START_TIMEOUT, _MINIMUM_TIMEOUT,
                     _PER_LINE_TIMEOUT)
    script_path = os.path.join(constants.DIR_SOURCE_ROOT, 'build', 'android',
                               'stacktrace', 'java_deobfuscate.py')
    self._command = [script_path, mapping_path]
    self.start()

  @property
  def name(self):
    return "deobfuscator"

  @property
  def command(self):
    return self._command


class DeobfuscatorPool(ExpensiveLineTransformerPool):
  def __init__(self, mapping_path):
    # As of Sep 2017, each instance requires about 500MB of RAM, as measured by:
    # /usr/bin/time -v build/android/stacktrace/java_deobfuscate.py \
    #     out/Release/apks/ChromePublic.apk.mapping
    self.mapping_path = mapping_path
    super().__init__(_MAX_RESTARTS, _POOL_SIZE, _PASSTHROUH_ON_FAILURE)

  @property
  def name(self):
    return "deobfuscator-pool"

  def CreateTransformer(self):
    return Deobfuscator(self.mapping_path)
