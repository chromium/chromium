# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import tempfile
import time

from devil.utils import cmd_helper
from pylib import constants
from pylib.constants import host_paths
from .expensive_line_transformer import ExpensiveLineTransformer
from .expensive_line_transformer import ExpensiveLineTransformerPool

_STACK_TOOL = os.path.join(host_paths.ANDROID_PLATFORM_DEVELOPMENT_SCRIPTS_PATH,
                           'stack')
_MINIMUM_TIMEOUT = 10.0
_PER_LINE_TIMEOUT = .005  # Should be able to process 200 lines per second.
_PROCESS_START_TIMEOUT = 20.0
_MAX_RESTARTS = 4  # Should be plenty unless tool is crashing on start-up.
_POOL_SIZE = 1
_PASSTHROUH_ON_FAILURE = True
ABI_REG = re.compile('ABI: \'(.+?)\'')


def _DeviceAbiToArch(device_abi):
  # The order of this list is significant to find the more specific match
  # (e.g., arm64) before the less specific (e.g., arm).
  arches = ['arm64', 'arm', 'x86_64', 'x86_64', 'x86', 'mips']
  for arch in arches:
    if arch in device_abi:
      return arch
  raise RuntimeError('Unknown device ABI: %s' % device_abi)


class Symbolizer:
  """A helper class to symbolize stack."""

  def __init__(self, apk_under_test=None):
    self._apk_under_test = apk_under_test
    self._time_spent_symbolizing = 0


  def __del__(self):
    self.CleanUp()


  def CleanUp(self):
    """Clean up the temporary directory of apk libs."""
    if self._time_spent_symbolizing > 0:
      logging.info(
          'Total time spent symbolizing: %.2fs', self._time_spent_symbolizing)


  def ExtractAndResolveNativeStackTraces(self, data_to_symbolize,
                                         device_abi, include_stack=True):
    """Run the stack tool for given input.

    Args:
      data_to_symbolize: a list of strings to symbolize.
      include_stack: boolean whether to include stack data in output.
      device_abi: the default ABI of the device which generated the tombstone.

    Yields:
      A string for each line of resolved stack output.
    """
    if not os.path.exists(_STACK_TOOL):
      logging.warning('%s missing. Unable to resolve native stack traces.',
                      _STACK_TOOL)
      return

    arch = _DeviceAbiToArch(device_abi)
    if not arch:
      logging.warning('No device_abi can be found.')
      return

    cmd = [_STACK_TOOL, '--arch', arch, '--output-directory',
           constants.GetOutDirectory(), '--more-info']
    env = dict(os.environ)
    env['PYTHONDONTWRITEBYTECODE'] = '1'
    with tempfile.NamedTemporaryFile(mode='w') as f:
      f.write('\n'.join(data_to_symbolize))
      f.flush()
      start = time.time()
      try:
        _, output = cmd_helper.GetCmdStatusAndOutput(cmd + [f.name], env=env)
      finally:
        self._time_spent_symbolizing += time.time() - start
    for line in output.splitlines():
      if not include_stack and 'Stack Data:' in line:
        break
      yield line


class PassThroughSymbolizer(ExpensiveLineTransformer):
  def __init__(self, device_abi):
    self._command = None
    super().__init__(_PROCESS_START_TIMEOUT, _MINIMUM_TIMEOUT,
                     _PER_LINE_TIMEOUT)
    if not os.path.exists(_STACK_TOOL):
      logging.warning('%s: %s missing. Unable to resolve native stack traces.',
                      PassThroughSymbolizer.name, _STACK_TOOL)
      return
    arch = _DeviceAbiToArch(device_abi)
    if not arch:
      logging.warning('%s: No device_abi can be found.',
                      PassThroughSymbolizer.name)
      return
    self._command = [
        _STACK_TOOL, '--arch', arch, '--output-directory',
        constants.GetOutDirectory(), '--more-info', '--pass-through', '--flush',
        '--quiet', '-'
    ]
    self.start()

  @property
  def name(self):
    return "symbolizer"

  @property
  def command(self):
    return self._command


class PassThroughSymbolizerPool(ExpensiveLineTransformerPool):
  def __init__(self, device_abi):
    self._device_abi = device_abi
    super().__init__(_MAX_RESTARTS, _POOL_SIZE, _PASSTHROUH_ON_FAILURE)

  def CreateTransformer(self):
    return PassThroughSymbolizer(self._device_abi)

  @property
  def name(self):
    return "symbolizer-pool"
