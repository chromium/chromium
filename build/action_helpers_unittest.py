#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import shutil
import sys
import tempfile
import time
import unittest
from unittest import mock

import action_helpers


class ActionHelpersTest(unittest.TestCase):
    def test_replace_file_retries_permission_error(self):
        with (
            mock.patch.object(
                action_helpers.os,
                'replace',
                side_effect=[PermissionError, None],
            ) as replace,
            mock.patch.object(action_helpers.time, 'sleep') as sleep,
        ):
            action_helpers._replace_file('source', 'destination')

        self.assertEqual(replace.call_count, 2)
        sleep.assert_called_once_with(0.01)

    def test_replace_file_reraises_permission_error(self):
        with (
            mock.patch.object(
                action_helpers.os,
                'replace',
                side_effect=PermissionError,
            ) as replace,
            mock.patch.object(action_helpers.time, 'sleep') as sleep,
            self.assertRaises(PermissionError),
        ):
            action_helpers._replace_file('source', 'destination')

        self.assertEqual(replace.call_count, 5)
        self.assertEqual(sleep.call_count, 4)

    def test_replace_file_overwrites_existing_destination(self):
        with tempfile.TemporaryDirectory() as tmp_dir:
            source = os.path.join(tmp_dir, 'source')
            destination = os.path.join(tmp_dir, 'destination')
            pathlib.Path(source).write_text('new')
            pathlib.Path(destination).write_text('old')

            action_helpers._replace_file(source, destination)

            self.assertEqual(pathlib.Path(destination).read_text(), 'new')
            self.assertFalse(os.path.exists(source))

    def test_atomic_output(self):
        tmp_file = pathlib.Path(tempfile.mktemp())
        tmp_file.write_text('test')
        try:
            # Test that same contents does not change mtime.
            orig_mtime = os.path.getmtime(tmp_file)
            with action_helpers.atomic_output(str(tmp_file), 'wt') as af:
                time.sleep(0.01)
                af.write('test')

            self.assertEqual(os.path.getmtime(tmp_file), orig_mtime)

            # Test that contents is written.
            with action_helpers.atomic_output(str(tmp_file), 'wt') as af:
                af.write('test2')
            self.assertEqual(tmp_file.read_text(), 'test2')
            self.assertNotEqual(os.path.getmtime(tmp_file), orig_mtime)
        finally:
            tmp_file.unlink()

    def test_parse_gn_list(self):
        def test(value, expected):
            self.assertEqual(action_helpers.parse_gn_list(value), expected)

        test(None, [])
        test('', [])
        test('asdf', ['asdf'])
        test('["one"]', ['one'])
        test(['["one"]', '["two"]'], ['one', 'two'])
        test(['["one", "two"]', '["three"]'], ['one', 'two', 'three'])

    def test_write_depfile(self):
        tmp_file = pathlib.Path(tempfile.mktemp())
        try:

            def capture_output(inputs):
                action_helpers.write_depfile(str(tmp_file), 'output', inputs)
                return tmp_file.read_text()

            self.assertEqual(capture_output(None), 'output: \n')
            self.assertEqual(capture_output([]), 'output: \n')
            self.assertEqual(capture_output(['a']), 'output: \\\n a\n')
            # Check sorted.
            self.assertEqual(
                capture_output(['b', 'a']), 'output: \\\n a \\\n b\n'
            )
            # Check converts to forward slashes.
            self.assertEqual(
                capture_output(['a', os.path.join('b', 'c')]),
                'output: \\\n a \\\n b/c\n',
            )

            # Arg should be a list.
            with self.assertRaises(AssertionError):
                capture_output('a')

            # Do not use depfile itself as an output.
            with self.assertRaises(AssertionError):
                capture_output([str(tmp_file)])

            # Do not use absolute paths.
            with self.assertRaises(AssertionError):
                capture_output([os.path.sep + 'foo'])

            # Do not use absolute paths (output path).
            with self.assertRaises(AssertionError):
                action_helpers.write_depfile(str(tmp_file), '/output', [])

        finally:
            tmp_file.unlink()


if __name__ == '__main__':
    unittest.main()
