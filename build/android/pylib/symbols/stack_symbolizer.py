# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import tempfile
import threading
import time

from devil.utils import cmd_helper
from pylib import constants
from pylib.constants import host_paths
from .expensive_line_transformer import ExpensiveLineTransformer
from .expensive_line_transformer import ExpensiveLineTransformerPool

with host_paths.SysPath(host_paths.ANDROID_PLATFORM_DEVELOPMENT_SCRIPTS_PATH):
    import stack_core  # pylint: disable=import-error

_STACK_TOOL = os.path.join(
    host_paths.ANDROID_PLATFORM_DEVELOPMENT_SCRIPTS_PATH, 'stack'
)
_MINIMUM_TIMEOUT = 10.0
_PER_LINE_TIMEOUT = 0.005  # Should be able to process 200 lines per second.
_PROCESS_START_TIMEOUT = 20.0
_TOMBSTONE_SYMBOLIZE_TIMEOUT = 180.0
_CAS_DOWNLOAD_TIMEOUT = 180.0
_MAX_RESTARTS = 4  # Should be plenty unless tool is crashing on start-up.
_POOL_SIZE = 1
_PASSTHROUH_ON_FAILURE = True
ABI_REG = re.compile('ABI: \'(.+?)\'')
_SYMBOL_CAS_DIGEST_FILE = 'lib.unstripped.cas_digest'

_DOWNLOAD_LOCK = threading.Lock()
_DOWNLOAD_ATTEMPTED_DIRS = set()


def _DeviceAbiToArch(device_abi):
    # The order of this list is significant to find the more specific match
    # (e.g., arm64) before the less specific (e.g., arm).
    arches = ['arm64', 'arm', 'x86_64', 'x86_64', 'x86', 'mips']
    for arch in arches:
        if arch in device_abi:
            return arch
    raise RuntimeError('Unknown device ABI: %s' % device_abi)


def _HasNativeCrashLines(lines):
    """Returns True if |lines| has native frames."""
    return any(stack_core.IsTraceLine(line) for line in lines)


def _GetSymbolCasDigest(out_dir):
    """Returns the RBE-CAS digest for unstripped symbols from file or env."""
    # Build recipes write `<out_dir>/lib.unstripped.cas_digest` before archiving
    # the main test isolate so the digest travels with the isolated inputs.
    # `ANDROID_SYMBOL_CAS_DIGEST` allows supplying or overriding the digest via
    # the Swarming task environment or during local testing.
    digest_file = os.path.join(out_dir, _SYMBOL_CAS_DIGEST_FILE)
    if os.path.isfile(digest_file):
        with open(digest_file, 'r', encoding='utf-8') as f:
            digest = f.read().strip()
        if digest:
            return digest
    return os.environ.get('ANDROID_SYMBOL_CAS_DIGEST')


def _EnsureSymbolsDownloaded(out_dir):
    """Fetches unstripped symbols on demand from RBE-CAS if omitted."""
    with _DOWNLOAD_LOCK:
        unstripped_dir = os.path.join(out_dir, 'lib.unstripped')
        if os.path.isdir(unstripped_dir) and os.listdir(unstripped_dir):
            return
        cas_digest = _GetSymbolCasDigest(out_dir)
        if not cas_digest:
            return
        attempt_key = (os.path.abspath(out_dir), cas_digest)
        if attempt_key in _DOWNLOAD_ATTEMPTED_DIRS:
            return
        _DOWNLOAD_ATTEMPTED_DIRS.add(attempt_key)
        cas_instance = os.environ.get(
            'RBE_CAS_INSTANCE',
            'projects/chromium-swarm/instances/default_instance',
        )
        logging.info(
            'Downloading unstripped symbols on demand from RBE-CAS (%s)...',
            cas_digest,
        )
        try:
            status, output = cmd_helper.GetCmdStatusAndOutputWithTimeout(
                [
                    'cas',
                    'download',
                    '-cas-instance',
                    cas_instance,
                    '-digest',
                    cas_digest,
                    '-dir',
                    out_dir,
                ],
                timeout=_CAS_DOWNLOAD_TIMEOUT,
            )
        except (OSError, cmd_helper.TimeoutError) as e:
            logging.warning(
                'On-demand RBE-CAS symbol download failed with exception: %s',
                e,
            )
            return
        if status != 0:
            logging.warning(
                'On-demand RBE-CAS symbol download failed (status %d): %s',
                status,
                output,
            )


def _StartAsyncSymbolsDownload(out_dir):
    """Starts _EnsureSymbolsDownloaded in a background thread."""
    threading.Thread(
        target=_EnsureSymbolsDownloaded, args=(out_dir,), daemon=True
    ).start()


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
                'Total time spent symbolizing: %.2fs',
                self._time_spent_symbolizing,
            )

    def ExtractAndResolveNativeStackTraces(
        self, data_to_symbolize, device_abi, include_stack=True
    ):
        """Run the stack tool for given input.

        Args:
          data_to_symbolize: a list of strings to symbolize.
          include_stack: boolean whether to include stack data in output.
          device_abi: the default ABI of the device which generated the tombstone.

        Yields:
          A string for each line of resolved stack output.
        """
        if not os.path.exists(_STACK_TOOL):
            logging.warning(
                '%s missing. Unable to resolve native stack traces.',
                _STACK_TOOL,
            )
            return

        arch = _DeviceAbiToArch(device_abi)
        if not arch:
            logging.warning('No device_abi can be found.')
            return

        out_dir = constants.GetOutDirectory()
        if _HasNativeCrashLines(data_to_symbolize):
            _EnsureSymbolsDownloaded(out_dir)

        cmd = [
            _STACK_TOOL,
            '--arch',
            arch,
            '--output-directory',
            out_dir,
            '--more-info',
        ]
        if self._apk_under_test:
            cmd.extend(
                ['--apks-directory', os.path.dirname(self._apk_under_test)]
            )
        env = dict(os.environ)
        env['PYTHONDONTWRITEBYTECODE'] = '1'
        with tempfile.NamedTemporaryFile(mode='w') as f:
            f.write('\n'.join(data_to_symbolize))
            f.flush()
            start = time.time()
            try:
                _, output = cmd_helper.GetCmdStatusAndOutputWithTimeout(
                    cmd + [f.name],
                    timeout=_TOMBSTONE_SYMBOLIZE_TIMEOUT,
                    env=env,
                )
            except cmd_helper.TimeoutError:
                logging.error(
                    'Timed out after %.1fs symbolizing native stack trace; '
                    'falling back to unsymbolized tombstone output.',
                    _TOMBSTONE_SYMBOLIZE_TIMEOUT,
                )
                yield from data_to_symbolize
                return
            finally:
                self._time_spent_symbolizing += time.time() - start
        for line in output.splitlines():
            if not include_stack and 'Stack Data:' in line:
                break
            yield line


class PassThroughSymbolizer(ExpensiveLineTransformer):
    def __init__(self, device_abi, apks_directory=None):
        self._command = None
        self._out_dir = None
        self._in_stack = False
        self._pending_lines = []
        super().__init__(
            _PROCESS_START_TIMEOUT, _MINIMUM_TIMEOUT, _PER_LINE_TIMEOUT
        )
        if not os.path.exists(_STACK_TOOL):
            logging.warning(
                '%s: %s missing. Unable to resolve native stack traces.',
                PassThroughSymbolizer.name,
                _STACK_TOOL,
            )
            return
        arch = _DeviceAbiToArch(device_abi)
        if not arch:
            logging.warning(
                '%s: No device_abi can be found.', PassThroughSymbolizer.name
            )
            return
        self._out_dir = constants.GetOutDirectory()
        self._command = [
            _STACK_TOOL,
            '--arch',
            arch,
            '--output-directory',
            self._out_dir,
            '--more-info',
            '--pass-through',
            '--flush',
            '--quiet',
        ]
        if apks_directory:
            self._command.extend(['--apks-directory', apks_directory])
        self._command.append('-')
        self.start()

    def TransformLines(self, lines):
        if not lines:
            return super().TransformLines(lines)
        output = []
        passthrough_batch = []
        for line in lines:
            if not self._in_stack:
                if stack_core.IsTraceLine(line):
                    if passthrough_batch:
                        output.extend(super().TransformLines(passthrough_batch))
                        passthrough_batch = []
                    self._in_stack = True
                    _StartAsyncSymbolsDownload(self._out_dir)
                    self._pending_lines.append(line)
                    output.append(line)
                else:
                    passthrough_batch.append(line)
            else:
                if stack_core.IsStackContinuationLine(line):
                    self._pending_lines.append(line)
                    output.append(line)
                else:
                    self._in_stack = False
                    pending = self._pending_lines
                    self._pending_lines = []
                    _EnsureSymbolsDownloaded(self._out_dir)
                    transformed = super().TransformLines(pending + [line])
                    output.extend(transformed[len(pending) :])
        if passthrough_batch:
            output.extend(super().TransformLines(passthrough_batch))
        return output

    @property
    def name(self):
        return "symbolizer"

    @property
    def command(self):
        return self._command


class PassThroughSymbolizerPool(ExpensiveLineTransformerPool):
    def __init__(self, device_abi, apks_directory=None):
        self._device_abi = device_abi
        self._apks_directory = apks_directory
        super().__init__(_MAX_RESTARTS, _POOL_SIZE, _PASSTHROUH_ON_FAILURE)

    def CreateTransformer(self):
        return PassThroughSymbolizer(self._device_abi, self._apks_directory)

    @property
    def name(self):
        return "symbolizer-pool"
