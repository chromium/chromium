#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import os
import sys
import unittest

sys.path.insert(
    0, os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
)
from util import build_utils

_NOTICE = 'Picked up _JAVA_OPTIONS: -XX:UseAVX=2\n'


def _WriteStderrCmd(text):
    return [sys.executable, '-c', f'import sys; sys.stderr.write({text!r})']


class FilterJvmOptionsNoticeTest(unittest.TestCase):
    def testRemovesNotices(self):
        output = (
            _NOTICE
            + 'Picked up JAVA_TOOL_OPTIONS: -Dfoo=bar\n'
            + 'NOTE: Picked up JDK_JAVA_OPTIONS: -Xmx1G\n'
        )
        self.assertEqual('', build_utils.FilterJvmOptionsNotice(output))

    def testKeepsOtherLines(self):
        output = 'warning: a\n' + _NOTICE + 'warning: b\n'
        self.assertEqual(
            'warning: a\nwarning: b\n',
            build_utils.FilterJvmOptionsNotice(output),
        )

    def testKeepsLinesThatOnlyMentionTheVariable(self):
        output = 'error: Picked up _JAVA_OPTIONS: is not at line start\n'
        self.assertEqual(output, build_utils.FilterJvmOptionsNotice(output))


class CheckOutputTest(unittest.TestCase):
    def _CheckOutput(self, text, **kwargs):
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            build_utils.CheckOutput(_WriteStderrCmd(text), **kwargs)
        return stderr.getvalue()

    def testNoticeAloneDoesNotFail(self):
        self.assertEqual('', self._CheckOutput(_NOTICE))

    def testNoticeIsRemovedBeforeStderrFilter(self):
        seen = []

        def stderr_filter(stderr):
            seen.append(stderr)
            return stderr

        self._CheckOutput(_NOTICE, stderr_filter=stderr_filter)
        self.assertEqual([''], seen)

    def testOtherStderrStillFails(self):
        with self.assertRaises(build_utils.CalledProcessError):
            self._CheckOutput(_NOTICE + 'warning: real\n')

    def testOtherStderrIsPrintedWithoutNotice(self):
        stderr = self._CheckOutput(
            _NOTICE + 'warning: real\n', fail_on_output=False
        )
        # The trailer that names the command follows the command's stderr.
        self.assertTrue(stderr.startswith('warning: real\n'), stderr)


class HandleCommandOutputTest(unittest.TestCase):
    def testNoticeAloneDoesNotFail(self):
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            build_utils.HandleCommandOutput(['cmd'], '', _NOTICE)
        self.assertEqual('', stderr.getvalue())


if __name__ == '__main__':
    unittest.main()
