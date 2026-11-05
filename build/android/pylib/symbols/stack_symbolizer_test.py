#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for stack_symbolizer."""

import os
import sys
import tempfile
import threading
import unittest
from unittest import mock

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
)
import devil_chromium  # pylint: disable=import-error  # noqa: E402

devil_chromium.Initialize()

from devil.utils import cmd_helper  # pylint: disable=wrong-import-position  # noqa: E402
from pylib.symbols import expensive_line_transformer  # pylint: disable=wrong-import-position  # noqa: E402
from pylib.symbols import stack_symbolizer  # pylint: disable=wrong-import-position  # noqa: E402


class StackSymbolizerTest(unittest.TestCase):
    def setUp(self):
        super().setUp()
        stack_symbolizer._DOWNLOAD_ATTEMPTED_DIRS.clear()

    @mock.patch('devil.utils.cmd_helper.GetCmdStatusAndOutputWithTimeout')
    def testEnsureSymbolsDownloaded_skipsWhenLocalSymbolsPresent(
        self, mock_cmd
    ):
        with tempfile.TemporaryDirectory() as out_dir:
            unstripped = os.path.join(out_dir, 'lib.unstripped')
            os.makedirs(unstripped)
            with open(os.path.join(unstripped, 'libchrome.so'), 'w') as f:
                f.write('elf')
            with mock.patch.dict(
                os.environ, {'ANDROID_SYMBOL_CAS_DIGEST': 'abc123/456'}
            ):
                stack_symbolizer._EnsureSymbolsDownloaded(out_dir)
            mock_cmd.assert_not_called()

    @mock.patch('devil.utils.cmd_helper.GetCmdStatusAndOutputWithTimeout')
    def testEnsureSymbolsDownloaded_skipsWithoutDigest(self, mock_cmd):
        with tempfile.TemporaryDirectory() as out_dir:
            env = dict(os.environ)
            env.pop('ANDROID_SYMBOL_CAS_DIGEST', None)
            with mock.patch.dict(os.environ, env, clear=True):
                stack_symbolizer._EnsureSymbolsDownloaded(out_dir)
            mock_cmd.assert_not_called()

    @mock.patch(
        'devil.utils.cmd_helper.GetCmdStatusAndOutputWithTimeout',
        return_value=(0, 'downloaded'),
    )
    def testEnsureSymbolsDownloaded_downloadsFromSidecarDigestFile(
        self, mock_cmd
    ):
        with tempfile.TemporaryDirectory() as out_dir:
            digest_path = os.path.join(
                out_dir, stack_symbolizer._SYMBOL_CAS_DIGEST_FILE
            )
            with open(digest_path, 'w') as f:
                f.write('feedface/2048\n')
            env = dict(os.environ)
            env.pop('ANDROID_SYMBOL_CAS_DIGEST', None)
            env.pop('RBE_CAS_INSTANCE', None)
            with mock.patch.dict(os.environ, env, clear=True):
                stack_symbolizer._EnsureSymbolsDownloaded(out_dir)
            mock_cmd.assert_called_once_with(
                [
                    'cas',
                    'download',
                    '-cas-instance',
                    'projects/chromium-swarm/instances/default_instance',
                    '-digest',
                    'feedface/2048',
                    '-dir',
                    out_dir,
                ],
                timeout=stack_symbolizer._CAS_DOWNLOAD_TIMEOUT,
            )

    @mock.patch(
        'devil.utils.cmd_helper.GetCmdStatusAndOutputWithTimeout',
        return_value=(0, 'downloaded'),
    )
    def testEnsureSymbolsDownloaded_downloadsFromCasEnvWhenMissing(
        self, mock_cmd
    ):
        with tempfile.TemporaryDirectory() as out_dir:
            with mock.patch.dict(
                os.environ,
                {
                    'ANDROID_SYMBOL_CAS_DIGEST': 'deadbeef/1024',
                    'RBE_CAS_INSTANCE': 'projects/test/instances/default',
                },
            ):
                stack_symbolizer._EnsureSymbolsDownloaded(out_dir)
            mock_cmd.assert_called_once_with(
                [
                    'cas',
                    'download',
                    '-cas-instance',
                    'projects/test/instances/default',
                    '-digest',
                    'deadbeef/1024',
                    '-dir',
                    out_dir,
                ],
                timeout=stack_symbolizer._CAS_DOWNLOAD_TIMEOUT,
            )

    @mock.patch(
        'devil.utils.cmd_helper.GetCmdStatusAndOutputWithTimeout',
        return_value=(1, 'network error'),
    )
    def testEnsureSymbolsDownloaded_doesNotRetryAfterFailure(self, mock_cmd):
        with tempfile.TemporaryDirectory() as out_dir:
            with mock.patch.dict(
                os.environ, {'ANDROID_SYMBOL_CAS_DIGEST': 'deadbeef/1024'}
            ):
                stack_symbolizer._EnsureSymbolsDownloaded(out_dir)
                stack_symbolizer._EnsureSymbolsDownloaded(out_dir)
            self.assertEqual(mock_cmd.call_count, 1)

    @mock.patch(
        'devil.utils.cmd_helper.GetCmdStatusAndOutputWithTimeout',
        side_effect=OSError('cas binary not found'),
    )
    def testEnsureSymbolsDownloaded_handlesOsErrorGracefully(self, mock_cmd):
        with tempfile.TemporaryDirectory() as out_dir:
            with mock.patch.dict(
                os.environ, {'ANDROID_SYMBOL_CAS_DIGEST': 'deadbeef/1024'}
            ):
                stack_symbolizer._EnsureSymbolsDownloaded(out_dir)
            mock_cmd.assert_called_once()

    @mock.patch(
        'devil.utils.cmd_helper.GetCmdStatusAndOutputWithTimeout',
        side_effect=cmd_helper.TimeoutError('timed out'),
    )
    def testEnsureSymbolsDownloaded_handlesTimeoutGracefully(self, mock_cmd):
        with tempfile.TemporaryDirectory() as out_dir:
            with mock.patch.dict(
                os.environ, {'ANDROID_SYMBOL_CAS_DIGEST': 'deadbeef/1024'}
            ):
                stack_symbolizer._EnsureSymbolsDownloaded(out_dir)
            mock_cmd.assert_called_once()

    def testStartAsyncSymbolsDownload_synchronizesWithEnsureSymbolsDownloaded(
        self,
    ):
        entered = threading.Event()
        release = threading.Event()

        def fake_cas_download(_cmd, timeout=None):
            del timeout
            entered.set()
            release.wait(timeout=5.0)
            return (0, 'downloaded')

        with tempfile.TemporaryDirectory() as out_dir:
            with mock.patch.dict(
                os.environ, {'ANDROID_SYMBOL_CAS_DIGEST': 'deadbeef/1024'}
            ):
                with mock.patch(
                    'devil.utils.cmd_helper.GetCmdStatusAndOutputWithTimeout',
                    side_effect=fake_cas_download,
                ) as mock_cmd:
                    stack_symbolizer._StartAsyncSymbolsDownload(out_dir)
                    self.assertTrue(entered.wait(timeout=5.0))

                    fg_done = threading.Event()

                    def run_foreground():
                        stack_symbolizer._EnsureSymbolsDownloaded(out_dir)
                        fg_done.set()

                    fg_thread = threading.Thread(target=run_foreground)
                    fg_thread.start()
                    self.assertFalse(fg_done.wait(timeout=0.05))

                    release.set()
                    fg_thread.join(timeout=5.0)
                    self.assertTrue(fg_done.is_set())
                    self.assertEqual(mock_cmd.call_count, 1)

    @mock.patch.object(
        expensive_line_transformer.ExpensiveLineTransformer,
        'TransformLines',
        side_effect=lambda lines: (
            lines + ['  SYMBOLIZED_STACK'] if len(lines) > 1 else lines
        ),
    )
    @mock.patch.object(stack_symbolizer.PassThroughSymbolizer, 'start')
    @mock.patch(
        'pylib.constants.GetOutDirectory', return_value='/tmp/out/Release'
    )
    @mock.patch('os.path.exists', return_value=True)
    @mock.patch.object(stack_symbolizer, '_EnsureSymbolsDownloaded')
    @mock.patch.object(stack_symbolizer, '_StartAsyncSymbolsDownload')
    def testPassThroughSymbolizer_buffersInStackAndFlushesAfterDownload(
        self,
        mock_async,
        mock_ensure,
        _mock_exists,
        _mock_out_dir,
        _mock_start,
        mock_super_transform,
    ):
        symbolizer = stack_symbolizer.PassThroughSymbolizer('arm64-v8a')

        out0 = symbolizer.TransformLines(
            ['03-25 00:51:05.510 I/ActivityManager: Start proc']
        )
        self.assertEqual(
            out0, ['03-25 00:51:05.510 I/ActivityManager: Start proc']
        )
        mock_async.assert_not_called()
        mock_ensure.assert_not_called()
        self.assertEqual(mock_super_transform.call_count, 1)

        frame0 = (
            '03-25 00:51:05.520 F/DEBUG:       #00 pc 001cf42e  '
            '/data/app/base.apk (offset 0x1000)'
        )
        out1 = symbolizer.TransformLines([frame0])
        self.assertEqual(out1, [frame0])
        mock_async.assert_called_once_with('/tmp/out/Release')
        mock_ensure.assert_not_called()
        self.assertEqual(mock_super_transform.call_count, 1)

        frame1 = (
            '03-25 00:51:05.521 F/DEBUG:       #01 pc 001cf480  '
            '/data/app/libchrome.so'
        )
        out2 = symbolizer.TransformLines([frame1])
        self.assertEqual(out2, [frame1])
        mock_async.assert_called_once()
        mock_ensure.assert_not_called()
        self.assertEqual(mock_super_transform.call_count, 1)

        code_line = (
            '03-25 00:51:05.530 F/DEBUG:     '
            '801cf40c ffffc4cc 00b2f2c5 00b2f1c7 00c1e1a8'
        )
        out3 = symbolizer.TransformLines([code_line])
        mock_ensure.assert_called_once_with('/tmp/out/Release')
        self.assertEqual(mock_super_transform.call_count, 2)
        mock_super_transform.assert_called_with([frame0, frame1, code_line])
        self.assertEqual(out3, [code_line, '  SYMBOLIZED_STACK'])

    @mock.patch(
        'devil.utils.cmd_helper.GetCmdStatusAndOutputWithTimeout',
        return_value=(0, 'resolved line\n'),
    )
    @mock.patch('pylib.constants.GetOutDirectory', return_value='/tmp/out')
    @mock.patch('os.path.exists', return_value=True)
    @mock.patch.object(stack_symbolizer, '_EnsureSymbolsDownloaded')
    def testSymbolizer_skipsDownloadOnJavaOnlyStack(
        self, mock_ensure, _mock_exists, _mock_out_dir, _mock_cmd
    ):
        sym = stack_symbolizer.Symbolizer()
        list(
            sym.ExtractAndResolveNativeStackTraces(
                [
                    'org.junit.AssumptionViolatedException: skipped',
                    '\tat org.junit.Assume.assumeTrue(Assume.java:10)',
                ],
                'arm64-v8a',
            )
        )
        mock_ensure.assert_not_called()

        list(
            sym.ExtractAndResolveNativeStackTraces(
                [
                    '03-25 00:51:05.520 F/DEBUG: #00 pc 001cf42e '
                    '/data/app/libchrome.so'
                ],
                'arm64-v8a',
            )
        )
        mock_ensure.assert_called_once_with('/tmp/out')


if __name__ == '__main__':
    unittest.main()
