#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for the resultdb module."""

import base64
import pathlib
import sys
import unittest
from unittest import mock

import eval_config
import resultdb
import results

CHROMIUM_SRC = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(CHROMIUM_SRC / 'build' / 'util'))
from lib.results import result_sink
from lib.results import result_types


class ResultDBReporterTest(unittest.TestCase):

    def setUp(self):
        self.mock_client = mock.Mock(spec=result_sink.ResultSinkClient)
        self.try_init_client_patcher = mock.patch(
            'lib.results.result_sink.TryInitClient',
            return_value=self.mock_client)
        self.mock_try_init_client = self.try_init_client_patcher.start()
        self.addCleanup(self.try_init_client_patcher.stop)

    def test_report_result(self):
        reporter = resultdb.ResultDBReporter()
        config = eval_config.TestConfig(test_file=CHROMIUM_SRC /
                                        'some_test.yaml')
        test_result = results.TestResult(
            config=config,
            success=True,
            iteration_results=[
                results.IterationResult(
                    success=True,
                    duration=1.23,
                    test_log='log',
                    metrics={'Foo': {
                        'bar': 3.21
                    }},
                    prompt='foo?',
                    response='bar!',
                )
            ])
        reporter.report_result(test_result)
        self.mock_client.Post.assert_called_once_with(
            test_id='some_test.yaml',
            status=result_types.PASS,
            duration=1230,
            test_log='log',
            test_file='//some_test.yaml',
            test_id_structured={
                'coarseName': '',
                'fineName': '',
                'caseNameComponents': ['some_test.yaml']
            },
            tags=[('foo_bar', '3.21')],
            artifacts={
                'Prompt': {
                    'contents': base64.b64encode('foo?'.encode()).decode(),
                    'content_type': 'text/plain',
                },
                'Response': {
                    'contents': base64.b64encode('bar!'.encode()).decode(),
                    'content_type': 'text/plain',
                },
            },
        )

    def test_report_result_with_owner(self):
        reporter = resultdb.ResultDBReporter()
        config = eval_config.TestConfig(test_file=CHROMIUM_SRC /
                                        'some_test.yaml',
                                        owner='foo')
        test_result = results.TestResult(
            config=config,
            success=True,
            iteration_results=[
                results.IterationResult(
                    success=True,
                    duration=1.23,
                    test_log='log',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ],
        )
        reporter.report_result(test_result)
        self.mock_client.Post.assert_called_once_with(
            test_id='some_test.yaml',
            status=result_types.PASS,
            duration=1230,
            test_log='log',
            test_file='//some_test.yaml',
            test_id_structured={
                'coarseName': '',
                'fineName': '',
                'caseNameComponents': ['some_test.yaml']
            },
            tags=[('owner', 'foo')],
            artifacts={},
        )

    def test_report_result_failure(self):
        reporter = resultdb.ResultDBReporter()
        config = eval_config.TestConfig(test_file=CHROMIUM_SRC /
                                        'some_test.yaml')
        test_result = results.TestResult(
            config=config,
            success=False,
            iteration_results=[
                results.IterationResult(
                    success=False,
                    duration=1.23,
                    test_log='log',
                    metrics={},
                    prompt=None,
                    response=None,
                )
            ],
        )
        reporter.report_result(test_result)
        self.mock_client.Post.assert_called_once_with(
            test_id='some_test.yaml',
            status=result_types.FAIL,
            duration=1230,
            test_log='log',
            test_file='//some_test.yaml',
            test_id_structured={
                'coarseName': '',
                'fineName': '',
                'caseNameComponents': ['some_test.yaml']
            },
            tags=[],
            artifacts={},
        )

    def test_client_none(self):
        self.mock_try_init_client.return_value = None
        reporter = resultdb.ResultDBReporter()
        reporter.report_result(mock.Mock())
        self.mock_client.Post.assert_not_called()


if __name__ == '__main__':
    unittest.main()
