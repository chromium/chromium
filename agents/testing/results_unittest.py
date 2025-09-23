# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for the results module."""

import pathlib
import sys
import unittest
import unittest.mock

import results

CHROMIUM_SRC = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(CHROMIUM_SRC / 'build' / 'util'))
from lib.results import result_sink
from lib.results import result_types


class ResultsTest(unittest.TestCase):

    def test_report_result(self):
        mock_client = unittest.mock.Mock(spec=result_sink.ResultSinkClient)
        test_result = results.TestResult(
            test_file=CHROMIUM_SRC / 'some_test.yaml',
            success=True,
            duration=1.23,
            test_log='log',
        )
        results.report_result(mock_client, test_result)
        mock_client.Post.assert_called_once_with(
            test_id='some_test.yaml',
            status=result_types.PASS,
            duration=1230,
            test_log='log',
            test_file='//some_test.yaml',
        )


if __name__ == '__main__':
    unittest.main()
