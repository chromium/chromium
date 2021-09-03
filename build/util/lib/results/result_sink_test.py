#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

# The following non-std imports are fetched via vpython. See the list at
# //.vpython3
import mock  # pylint: disable=import-error
import six

_BUILD_UTIL_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
if _BUILD_UTIL_PATH not in sys.path:
  sys.path.insert(0, _BUILD_UTIL_PATH)

from lib.results import result_sink
from lib.results import result_types


class InitClientTest(unittest.TestCase):
  @mock.patch.dict(os.environ, {}, clear=True)
  def testEmptyClient(self):
    # No LUCI_CONTEXT env var should prevent a client from being created.
    client = result_sink.TryInitClient()
    self.assertIsNone(client)

  @mock.patch.dict(os.environ, {'LUCI_CONTEXT': 'some-file.json'})
  def testBasicClient(self):
    luci_context_json = {
        'result_sink': {
            'address': 'some-ip-address',
            'auth_token': 'some-auth-token',
        },
    }
    if six.PY2:
      open_builtin_path = '__builtin__.open'
    else:
      open_builtin_path = 'builtins.open'
    with mock.patch(open_builtin_path,
                    mock.mock_open(read_data=json.dumps(luci_context_json))):
      client = result_sink.TryInitClient()
    self.assertEqual(
        client.test_results_url,
        'http://some-ip-address/prpc/luci.resultsink.v1.Sink/ReportTestResults')
    self.assertEqual(client.headers['Authorization'],
                     'ResultSink some-auth-token')


class ClientTest(unittest.TestCase):
  def setUp(self):
    context = {
        'address': 'some-ip-address',
        'auth_token': 'some-auth-token',
    }
    self.client = result_sink.ResultSinkClient(context)

  @mock.patch('requests.post')
  def testPostPassingTest(self, mock_post):
    self.client.Post('some-test', result_types.PASS, 0, 'some-test-log', None)
    self.assertEqual(
        mock_post.call_args[1]['url'],
        'http://some-ip-address/prpc/luci.resultsink.v1.Sink/ReportTestResults')
    data = json.loads(mock_post.call_args[1]['data'])
    self.assertEqual(data['testResults'][0]['testId'], 'some-test')
    self.assertEqual(data['testResults'][0]['status'], 'PASS')

  @mock.patch('requests.post')
  def testPostFailingTest(self, mock_post):
    self.client.Post('some-test',
                     result_types.FAIL,
                     0,
                     'some-test-log',
                     None,
                     failure_reason='omg test failure')
    data = json.loads(mock_post.call_args[1]['data'])
    self.assertEqual(data['testResults'][0]['status'], 'FAIL')
    self.assertEqual(data['testResults'][0]['testMetadata']['name'],
                     'some-test')
    self.assertEqual(
        data['testResults'][0]['failureReason']['primaryErrorMessage'],
        'omg test failure')

  @mock.patch('requests.post')
  def testPostWithTestFile(self, mock_post):
    self.client.Post('some-test', result_types.PASS, 0, 'some-test-log',
                     '//some/test.cc')
    data = json.loads(mock_post.call_args[1]['data'])
    self.assertEqual(
        data['testResults'][0]['testMetadata']['location']['file_name'],
        '//some/test.cc')
    self.assertEqual(data['testResults'][0]['testMetadata']['name'],
                     'some-test')
    self.assertIsNotNone(data['testResults'][0]['summaryHtml'])


if __name__ == '__main__':
  unittest.main()
