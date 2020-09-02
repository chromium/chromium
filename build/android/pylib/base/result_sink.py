# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import base64
import cgi
import json
import os

from pylib.base import base_test_result
import requests  # pylint: disable=import-error

# Comes from luci/resultdb/pbutil/test_result.go
# Slightly smaller to allow prepending <pre></pres>
MAX_REPORT_LEN = 4 * 1024

# Maps base_test_results to the luci test-result.proto.
# https://godoc.org/go.chromium.org/luci/resultdb/proto/v1#TestStatus
RESULT_MAP = {
    base_test_result.ResultType.UNKNOWN: 'STATUS_UNSPECIFIED',
    base_test_result.ResultType.PASS: 'PASS',
    base_test_result.ResultType.FAIL: 'FAIL',
    base_test_result.ResultType.CRASH: 'CRASH',
    base_test_result.ResultType.TIMEOUT: 'ABORT',
    base_test_result.ResultType.SKIP: 'SKIP',
    base_test_result.ResultType.NOTRUN: 'SKIP',
}


def TryInitClient():
  """Tries to initialize a result_sink_client object.

  Assumes that rdb stream is already running.

  Returns:
    A ResultSinkClient for the result_sink server else returns None.
  """
  try:
    with open(os.environ['LUCI_CONTEXT']) as f:
      sink = json.load(f)['result_sink']
      return ResultSinkClient(sink)
  except KeyError:
    return None


class ResultSinkClient(object):
  """A class to store the sink's post configurations and make post requests.

  This assumes that the rdb stream has been called already and that the
  server is listening.
  """
  def __init__(self, context):
    self.url = ('http://%s/prpc/luci.resultsink.v1.Sink/ReportTestResults' %
                context['address'])
    self.headers = {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
        'Authorization': 'ResultSink %s' % context['auth_token'],
    }

  def Post(self, test_id, status, test_log):
    """Uploads the test result to the ResultSink server.

    This assumes that the rdb stream has been called already and that
    server is ready listening.

    Args:
      test_id: A string representing the test's name.
      status: A string representing if the test passed, failed, etc...
      test_log: A string representing the test's output.

    Returns:
      N/A
    """
    assert status in RESULT_MAP
    expected = status in (base_test_result.ResultType.PASS,
                          base_test_result.ResultType.SKIP)
    status = RESULT_MAP[status]

    # Slightly smaller to allow addition of <pre> tags and message.
    report_check_size = MAX_REPORT_LEN - 45
    test_log_formatted = cgi.escape(test_log)
    if len(test_log) > report_check_size:
      test_log_formatted = ('<pre>' + test_log[:report_check_size] +
                            '...Full output in Artifact.</pre>')
    else:
      test_log_formatted = '<pre>' + test_log + '</pre>'

    tr = {
        'expected': expected,
        'status': status,
        'summaryHtml': test_log_formatted,
        'testId': test_id,
    }
    if len(test_log) > report_check_size:
      tr['artifacts'] = {'Test Log': {'contents': base64.b64encode(test_log)}}

    requests.post(url=self.url,
                  headers=self.headers,
                  data=json.dumps({'testResults': [tr]}))
