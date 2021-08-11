# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import absolute_import
import base64
import json
import os

import six

import requests  # pylint: disable=import-error
from lib.results import result_types

# Maps result_types to the luci test-result.proto.
# https://godoc.org/go.chromium.org/luci/resultdb/proto/v1#TestStatus
RESULT_MAP = {
    result_types.UNKNOWN: 'ABORT',
    result_types.PASS: 'PASS',
    result_types.FAIL: 'FAIL',
    result_types.CRASH: 'CRASH',
    result_types.TIMEOUT: 'ABORT',
    result_types.SKIP: 'SKIP',
    result_types.NOTRUN: 'SKIP',
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
    base_url = 'http://%s/prpc/luci.resultsink.v1.Sink' % context['address']
    self.test_results_url = base_url + '/ReportTestResults'
    self.report_artifacts_url = base_url + '/ReportInvocationLevelArtifacts'

    self.headers = {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
        'Authorization': 'ResultSink %s' % context['auth_token'],
    }

  def Post(self,
           test_id,
           status,
           duration,
           test_log,
           test_file,
           artifacts=None,
           failure_reason=None):
    """Uploads the test result to the ResultSink server.

    This assumes that the rdb stream has been called already and that
    server is ready listening.

    Args:
      test_id: A string representing the test's name.
      status: A string representing if the test passed, failed, etc...
      duration: An int representing time in ms.
      test_log: A string representing the test's output.
      test_file: A string representing the file location of the test.
      artifacts: An optional dict of artifacts to attach to the test.
      failure_reason: An optional string with the reason why the test failed.
          Should be None if the test did not fail.

    Returns:
      N/A
    """
    assert status in RESULT_MAP
    expected = status in (result_types.PASS, result_types.SKIP)
    result_db_status = RESULT_MAP[status]

    tr = {
        'expected':
        expected,
        'status':
        result_db_status,
        'tags': [
            {
                'key': 'test_name',
                'value': test_id,
            },
            {
                # Status before getting mapped to result_db statuses.
                'key': 'android_test_runner_status',
                'value': status,
            }
        ],
        'testId':
        test_id,
    }

    artifacts = artifacts or {}
    if test_log:
      # Upload the original log without any modifications.
      b64_log = six.ensure_str(base64.b64encode(six.ensure_binary(test_log)))
      artifacts.update({'Test Log': {'contents': b64_log}})
      tr['summaryHtml'] = '<text-artifact artifact-id="Test Log" />'
    if artifacts:
      tr['artifacts'] = artifacts
    if failure_reason:
      tr['failureReason'] = {
          'primaryErrorMessage': _TruncateToUTF8Bytes(failure_reason, 1024)
      }

    if duration is not None:
      # Duration must be formatted to avoid scientific notation in case
      # number is too small or too large. Result_db takes seconds, not ms.
      # Need to use float() otherwise it does substitution first then divides.
      tr['duration'] = '%.9fs' % float(duration / 1000.0)

    if test_file and str(test_file).startswith('//'):
      tr['testMetadata'] = {
          'name': test_id,
          'location': {
              'file_name': test_file,
              'repo': 'https://chromium.googlesource.com/chromium/src',
          }
      }

    res = requests.post(url=self.test_results_url,
                        headers=self.headers,
                        data=json.dumps({'testResults': [tr]}))
    res.raise_for_status()

  def ReportInvocationLevelArtifacts(self, artifacts):
    """Uploads invocation-level artifacts to the ResultSink server.

    This is for artifacts that don't apply to a single test but to the test
    invocation as a whole (eg: system logs).

    Args:
      artifacts: A dict of artifacts to attach to the invocation.
    """
    req = {'artifacts': artifacts}
    res = requests.post(url=self.report_artifacts_url,
                        headers=self.headers,
                        data=json.dumps(req))
    res.raise_for_status()


def _TruncateToUTF8Bytes(s, length):
  """ Truncates a string to a given number of bytes when encoded as UTF-8.

  Ensures the given string does not take more than length bytes when encoded
  as UTF-8. Adds trailing ellipsis (...) if truncation occurred. A truncated
  string may end up encoding to a length slightly shorter than length because
  only whole Unicode codepoints are dropped.

  Args:
    s: The string to truncate.
    length: the length (in bytes) to truncate to.
  """
  encoded = s.encode('utf-8')
  if len(encoded) > length:
    # Truncate, leaving space for trailing ellipsis (...).
    encoded = encoded[:length - 3]
    # Truncating the string encoded as UTF-8 may have left the final codepoint
    # only partially present. Pass 'ignore' to acknowledge and ensure this is
    # dropped.
    return encoded.decode('utf-8', 'ignore') + "..."
  return s
