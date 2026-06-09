#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for common_types.py."""

import datetime
import unittest

from common_types import CommonArgs, PreviousRunInfo


class CommonArgsTest(unittest.TestCase):

    def test_clobber_true_when_no_previous_run(self):
        args = CommonArgs(
            project='proj',
            repo='repo',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime.now(tz=datetime.timezone.utc),
            dryrun=False,
            previous_run=None,
        )
        self.assertTrue(args.clobber)

    def test_clobber_false_when_previous_run_exists(self):
        previous_run = PreviousRunInfo(
            revision='deadbeef',
            start_time=datetime.datetime.now(tz=datetime.timezone.utc),
        )
        args = CommonArgs(
            project='proj',
            repo='repo',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime.now(tz=datetime.timezone.utc),
            dryrun=False,
            previous_run=previous_run,
        )
        self.assertFalse(args.clobber)


if __name__ == '__main__':
    unittest.main()
