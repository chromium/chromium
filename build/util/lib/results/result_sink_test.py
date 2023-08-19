#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from unittest import mock

_BUILD_UTIL_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
if _BUILD_UTIL_PATH not in sys.path:
  sys.path.insert(0, _BUILD_UTIL_PATH)

from lib.results import result_sink
from lib.results import result_types

_FAKE_CONTEXT = {
    'address': 'some-ip-address',
    'auth_token': 'some-auth-token',
}


class InitClientTest(unittest.TestCase):
  @mock.patch.dict(os.environ, {}, clear=True)
  def testEmptyClient(self):
    # No LUCI_CONTEXT env var should prevent a client from being created.
    client = result_sink.TryInitClient()
    self.assertIsNone(client)

  @mock.patch.dict(os.environ, {'LUCI_CONTEXT': 'some-file.json'})
  def testBasicClient(self):
    luci_context_json = {
        'result_sink': _FAKE_CONTEXT,
    }
    with mock.patch('builtins.open',
                    mock.mock_open(read_data=json.dumps(luci_context_json))):
      client = result_sink.TryInitClient()
    self.assertEqual(
        client.test_results_url,
        'http://some-ip-address/prpc/luci.resultsink.v1.Sink/ReportTestResults')
    self.assertEqual(client.session.headers['Authorization'],
                     'ResultSink some-auth-token')

  @mock.patch('requests.Session')
  def testReuseSession(self, mock_session):
    client = result_sink.ResultSinkClient(_FAKE_CONTEXT)
    client.Post('some-test', result_types.PASS, 0, 'some-test-log', None)
    client.Post('some-test', result_types.PASS, 0, 'some-test-log', None)
    self.assertEqual(mock_session.call_count, 1)
    self.assertEqual(client.session.post.call_count, 2)

  @mock.patch('requests.Session.close')
  def testCloseClient(self, mock_close):
    client = result_sink.ResultSinkClient(_FAKE_CONTEXT)
    client.close()
    mock_close.assert_called_once()

  @mock.patch('requests.Session.close')
  def testClientAsContextManager(self, mock_close):
    with result_sink.ResultSinkClient(_FAKE_CONTEXT) as client:
      mock_close.assert_not_called()
    mock_close.assert_called_once()


class ClientTest(unittest.TestCase):
  def setUp(self):
    self.client = result_sink.ResultSinkClient(_FAKE_CONTEXT)

  @mock.patch('requests.Session.post')
  def testPostPassingTest(self, mock_post):
    self.client.Post('some-test', result_types.PASS, 0, 'some-test-log', None)
    self.assertEqual(
        mock_post.call_args[1]['url'],
        'http://some-ip-address/prpc/luci.resultsink.v1.Sink/ReportTestResults')
    data = json.loads(mock_post.call_args[1]['data'])
    self.assertEqual(data['testResults'][0]['testId'], 'some-test')
    self.assertEqual(data['testResults'][0]['status'], 'PASS')

  @mock.patch('requests.Session.post')
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

  @mock.patch('requests.Session.post')
  def testPostWithTestLogAndHTMLSummary(self, mock_post):
    # This is under max length, but will be over when test log
    # artifact is included.
    test_artifact = '<text-artifact artifact-id="%s" />' % 'b' * (
        result_sink.HTML_SUMMARY_MAX - 35)
    self.client.Post('some-test',
                     result_types.PASS,
                     0,
                     'some-test-log',
                     '//some/test.cc',
                     html_artifact=test_artifact)
    data = json.loads(mock_post.call_args[1]['data'])
    self.assertIsNotNone(data['testResults'][0]['summaryHtml'])
    self.assertTrue(
        len(data['testResults'][0]['summaryHtml']) <
        result_sink.HTML_SUMMARY_MAX)
    self.assertTrue(result_sink._HTML_SUMMARY_ARTIFACT in data['testResults'][0]
                    ['summaryHtml'])
    self.assertTrue(
        result_sink._TEST_LOG_ARTIFACT in data['testResults'][0]['summaryHtml'])

  @mock.patch('requests.Session.post')
  def testPostWithTooLongSummary(self, mock_post):
    # This will be over max length.
    test_artifact = ('<text-artifact artifact-id="%s" />' % 'b' *
                     result_sink.HTML_SUMMARY_MAX)
    self.client.Post('some-test',
                     result_types.PASS,
                     0,
                     'some-test-log',
                     '//some/test.cc',
                     html_artifact=test_artifact)
    data = json.loads(mock_post.call_args[1]['data'])
    self.assertIsNotNone(data['testResults'][0]['summaryHtml'])
    self.assertTrue(
        len(data['testResults'][0]['summaryHtml']) <
        result_sink.HTML_SUMMARY_MAX)
    self.assertTrue(result_sink._HTML_SUMMARY_ARTIFACT in data['testResults'][0]
                    ['summaryHtml'])

  @mock.patch('requests.Session.post')
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

  @mock.patch('requests.Session.post')
  def testPostWithVariant(self, mock_post):
    self.client.Post('some-test',
                     result_types.PASS,
                     0,
                     'some-test-log',
                     None,
                     variant={
                         'key1': 'value1',
                         'key2': 'value2'
                     })
    data = json.loads(mock_post.call_args[1]['data'])
    self.assertEqual(data['testResults'][0]['variant'],
                     {'def': {
                         'key1': 'value1',
                         'key2': 'value2'
                     }})

  @mock.patch('requests.Session.post')
  def testPostWithTags(self, mock_post):
    self.client.Post('some-test',
                     result_types.PASS,
                     0,
                     'some-test-log',
                     None,
                     tags=[('key1', 'value1'), ('key2', 'value2')])
    data = json.loads(mock_post.call_args[1]['data'])
    self.assertIn({
        'key': 'key1',
        'value': 'value1'
    }, data['testResults'][0]['tags'])
    self.assertIn({
        'key': 'key2',
        'value': 'value2'
    }, data['testResults'][0]['tags'])


if __name__ == '__main__':
  unittest.main()
