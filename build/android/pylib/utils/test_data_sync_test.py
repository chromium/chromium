#! /usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for test_data_sync.py."""

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.append(str(Path(__file__).parents[2]))

from pylib.utils import device_dependencies  # noqa: E402
from pylib.utils import test_data_sync  # noqa: E402
from devil.android import device_errors  # noqa: E402  # Path set by pylib.
from devil.android import device_utils  # noqa: E402
from devil.android import devil_util  # noqa: E402

_ROOT = '/data/media/10/chromium_tests_root'
_MARKER = _ROOT + '/.chromium_test_data_stamp'


class _FakeDevice:
    """In-memory device that records (method, path, kwargs) for each call."""

    def __init__(self):
        self.files = {}
        self.calls = []

    def RunShellCommand(self, cmd, **kwargs):
        assert cmd[0] == 'cat' and len(cmd) == 2, cmd
        path = cmd[1]
        self.calls.append(('cat', path, kwargs))
        if path not in self.files:
            if kwargs.get('check_return'):
                raise device_errors.AdbCommandFailedError(cmd, 'No such file')
            return 'cat: %s: No such file or directory\n' % path
        return self.files[path]

    def WriteFile(self, path, content, **kwargs):
        self.calls.append(('WriteFile', path, kwargs))
        self.files[path] = content

    def RemovePath(self, path, **kwargs):
        self.calls.append(('RemovePath', path, kwargs))
        self.files.pop(path, None)


class TestDataSyncTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self._data_dir = os.path.join(self._tmp.name, 'data')
        self._file = self._Write('data/a.txt', 'ab', mtime_ns=10**18)
        self._single = self._Write('single.pak', '')
        self._device = _FakeDevice()
        self._tuples = [
            (self._data_dir, _ROOT + '/data'),
            (self._single, _ROOT),
        ]

    def _Write(self, rel_path, content='x', mtime_ns=None):
        path = os.path.join(self._tmp.name, rel_path)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(content)
        if mtime_ns is not None:
            os.utime(path, ns=(mtime_ns, mtime_ns))
        return path

    def _Sync(self, enabled=True, push=lambda: None):
        commit = test_data_sync.PreparePush(
            self._device, _ROOT, self._tuples, enabled=enabled, as_root=True
        )
        if not commit:
            return False
        self._device.calls.append(('push', None, {}))
        push()
        commit()
        return True

    def _CallNames(self):
        return [c[0] for c in self._device.calls]

    def testStampChangesWithEachInput(self):
        stamp = test_data_sync.ComputeStamp(self._tuples)
        self.assertEqual(stamp, test_data_sync.ComputeStamp(self._tuples))
        # Each change alters exactly one input relative to the previous state.
        changes = {
            'size': lambda: self._Write('data/a.txt', 'abc', mtime_ns=10**18),
            'mtime': lambda: os.utime(self._file, ns=(1, 1)),
            'added file': lambda: self._Write('data/sub/new.txt'),
            'removed file': lambda: os.remove(self._file),
            'missing top-level file': lambda: os.remove(self._single),
        }
        for name, change in changes.items():
            change()
            new_stamp = test_data_sync.ComputeStamp(self._tuples)
            self.assertNotEqual(stamp, new_stamp, name)
            stamp = new_stamp
        other_dest = [(self._data_dir, _ROOT + '/other'), (self._single, _ROOT)]
        self.assertNotEqual(stamp, test_data_sync.ComputeStamp(other_dest))

    def testStampIgnoresContentWithSameMtimeAndSize(self):
        # Known limitation: file contents are never read.
        stamp = test_data_sync.ComputeStamp(self._tuples)
        self._Write('data/a.txt', 'cd', mtime_ns=10**18)
        self.assertEqual(stamp, test_data_sync.ComputeStamp(self._tuples))

    def testFirstSyncPushesAndWritesMarker(self):
        self.assertTrue(self._Sync())
        self.assertEqual(
            self._CallNames(), ['cat', 'RemovePath', 'push', 'WriteFile']
        )
        for name, path, kwargs in self._device.calls:
            if name != 'push':
                self.assertEqual(path, _MARKER, name)
                self.assertIs(kwargs.get('as_root'), True, name)
        # A missing marker is expected: devil logs CRITICAL stack dumps for
        # failing commands.
        self.assertIs(self._device.calls[0][2].get('check_return'), False)
        self.assertEqual(
            json.loads(self._device.files[_MARKER]),
            {'stamp': test_data_sync.ComputeStamp(self._tuples)},
        )

    def testSkipsWhenMarkerMatches(self):
        self._Sync()
        self._device.calls.clear()
        self.assertFalse(self._Sync())
        self.assertEqual(self._CallNames(), ['cat'])

    def testPushesWhenMarkerDoesNotMatch(self):
        for marker in [
            json.dumps({'stamp': 'other'}),
            'not json',
            '[]',
            'null',
        ]:
            self._device.files[_MARKER] = marker
            self.assertTrue(self._Sync(), marker)
        self._Write('data/a.txt', 'changed')
        self.assertTrue(self._Sync())

    def testDisabledInvalidatesMarkerWithoutWritingOne(self):
        self._Sync()
        self._device.calls.clear()
        self.assertTrue(self._Sync(enabled=False))
        self.assertEqual(
            self._device.calls,
            [
                ('RemovePath', _MARKER, {'force': True, 'as_root': True}),
                ('push', None, {}),
            ],
        )
        self.assertNotIn(_MARKER, self._device.files)

    def testFailedPushLeavesNoMarker(self):
        self._Sync()
        self._Write('data/a.txt', 'changed')

        def fail():
            raise device_errors.CommandFailedError('push failed')

        with self.assertRaises(device_errors.CommandFailedError):
            self._Sync(push=fail)
        self.assertNotIn(_MARKER, self._device.files)

    def testRecordsStampFromBeforePush(self):
        stamp = test_data_sync.ComputeStamp(self._tuples)
        self._Sync(push=lambda: self._Write('data/a.txt', 'edited during push'))
        self.assertEqual(
            json.loads(self._device.files[_MARKER])['stamp'], stamp
        )

    def testDeviceErrorsAreNotFatal(self):
        device = mock.Mock()
        error = device_errors.CommandTimeoutError('timeout')
        device.RunShellCommand.side_effect = error
        device.RemovePath.side_effect = error
        device.WriteFile.side_effect = error
        commit = test_data_sync.PreparePush(
            device, _ROOT, self._tuples, enabled=True
        )
        self.assertIsNotNone(commit)
        commit()
        test_data_sync.InvalidateMarker(device, _ROOT)

    def testCoversSameFilesAsPushChangedFiles(self):
        tmp = self._tmp.name
        self._Write('data/.hidden_file')
        self._Write('data/.hidden_dir/b.txt')
        self._Write('data/sub/.hidden_dir/c.txt')
        os.symlink(self._file, os.path.join(self._data_dir, 'file_link'))
        os.symlink(tmp, os.path.join(self._data_dir, 'dir_link'))
        dot_parent = os.path.dirname(self._Write('.dot/data/d.txt'))
        single = self._Write('.dot/single.txt')
        os.symlink(self._data_dir, os.path.join(tmp, 'data_link'))
        tuples = device_dependencies.SubstituteDeviceRoot(
            [
                (self._data_dir, 'data'),
                (os.path.join(tmp, 'data_link'), 'linked'),
                (dot_parent, 'dot'),
                (single, 'single.txt'),
            ],
            _ROOT,
        )

        device = mock.Mock(_enable_device_files_cache=False)
        device._GetDeviceNodes.return_value = set()
        with mock.patch.object(
            devil_util, 'CalculateHostHashes', return_value={}
        ):
            # pylint: disable=protected-access
            pushed, _ = device_utils.DeviceUtils._GetChangedFiles(
                device, tuples
            )
            covered = [
                f for h, _ in tuples for f in test_data_sync._IterHostFiles(h)
            ]
        self.assertTrue(pushed)
        self.assertEqual(sorted(covered), sorted(h for h, _ in pushed))


if __name__ == '__main__':
    unittest.main()
