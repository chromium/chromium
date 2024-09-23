# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import absolute_import
import base64
import json
import logging
import os

import requests  # pylint: disable=import-error
from lib.results import result_types

HTML_SUMMARY_MAX = 4096

_HTML_SUMMARY_ARTIFACT = '<text-artifact artifact-id="HTML Summary" />'
_TEST_LOG_ARTIFACT = '<text-artifact artifact-id="Test Log" />'

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
    self.update_invocation_url = base_url + '/UpdateInvocation'

    headers = {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
        'Authorization': 'ResultSink %s' % context['auth_token'],
    }
    self.session = requests.Session()
    self.session.headers.update(headers)

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    self.close()

  def close(self):
    """Closes the session backing the sink."""
    self.session.close()

  def Post(self,
           test_id,
           status,
           duration,
           test_log,
           test_file,
           variant=None,
           artifacts=None,
           failure_reason=None,
           html_artifact=None,
           tags=None):
    """Uploads the test result to the ResultSink server.

    This assumes that the rdb stream has been called already and that
    server is ready listening.

    Args:
      test_id: A string representing the test's name.
      status: A string representing if the test passed, failed, etc...
      duration: An int representing time in ms.
      test_log: A string representing the test's output.
      test_file: A string representing the file location of the test.
      variant: An optional dict of variant key value pairs as the
          additional variant sent from test runners, which can override
          or add to the variants passed to `rdb stream` command.
      artifacts: An optional dict of artifacts to attach to the test.
      failure_reason: An optional string with the reason why the test failed.
          Should be None if the test did not fail.
      html_artifact: An optional html-formatted string to prepend to the test's
          log. Useful to encode click-able URL links in the test log, since that
          won't be formatted in the test_log.
      tags: An optional list of tuple of key name and value to prepend to the
          test's tags.

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
                'key': 'raw_status',
                'value': status,
            }
        ],
        'testId':
        test_id,
        'testMetadata': {
            'name': test_id,
        }
    }

    if tags:
      tr['tags'].extend({
          'key': key_name,
          'value': value
      } for (key_name, value) in tags)

    if variant:
      tr['variant'] = {'def': variant}

    artifacts = artifacts or {}
    tr['summaryHtml'] = html_artifact if html_artifact else ''

    # If over max supported length of html summary, replace with artifact
    # upload.
    if (test_log
        and len(tr['summaryHtml']) + len(_TEST_LOG_ARTIFACT) > HTML_SUMMARY_MAX
        or len(tr['summaryHtml']) > HTML_SUMMARY_MAX):
      b64_summary = base64.b64encode(tr['summaryHtml'].encode()).decode()
      artifacts.update({'HTML Summary': {'contents': b64_summary}})
      tr['summaryHtml'] = _HTML_SUMMARY_ARTIFACT

    if test_log:
      # Upload the original log without any modifications.
      b64_log = base64.b64encode(test_log.encode()).decode()
      artifacts.update({'Test Log': {'contents': b64_log}})
      tr['summaryHtml'] += _TEST_LOG_ARTIFACT

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
      tr['testMetadata']['location'] = {
          'file_name': test_file,
          'repo': 'https://chromium.googlesource.com/chromium/src',
      }

    res = self.session.post(url=self.test_results_url,
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
    res = self.session.post(url=self.report_artifacts_url, data=json.dumps(req))
    res.raise_for_status()

  def UpdateInvocation(self, invocation, update_mask):
    """Update the invocation to the ResultSink server.

    Details can be found in the proto luci.resultsink.v1.UpdateInvocationRequest

    Args:
      invocation: a dict representation of luci.resultsink.v1.Invocation proto
      update_mask: a dict representation of google.protobuf.FieldMask proto
    """
    req = {
        'invocation': invocation,
        'update_mask': update_mask,
    }
    res = self.session.post(url=self.update_invocation_url,
                            data=json.dumps(req))
    res.raise_for_status()

  def UpdateInvocationExtendedProperties(self, extended_properties, keys=None):
    """Update the extended_properties field of an invocation.

    Details can be found in the "extended_properties" field of the proto
    luci.resultdb.v1.Invocation.

    Args:
      extended_properties: a dict containing the content of extended_properties.
        The value in the dict shall be a dict containing a "@type" key
        representing the data schema, and corresponding data.
      keys: (Optional) a list of keys in extended_properties to add, replace,
        or remove. If a key exists in "keys", but not in "extended_properties",
        this is considered as deleting the key from the resultdb record side
        If None, the keys in "extended_properties" dict will be used.
    """
    if not keys:
      keys = extended_properties.keys()
    mask_paths = ['extended_properties.%s' % key for key in keys]
    invocation = {'extended_properties': extended_properties}
    update_mask = {'paths': mask_paths}
    self.UpdateInvocation(invocation, update_mask)


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
  try:
    encoded = s.encode('utf-8')
  # When encode throws UnicodeDecodeError in py2, it usually means the str is
  # already encoded and has non-ascii chars. So skip re-encoding it.
  except UnicodeDecodeError:
    encoded = s
  if len(encoded) > length:
    # Truncate, leaving space for trailing ellipsis (...).
    encoded = encoded[:length - 3]
    # Truncating the string encoded as UTF-8 may have left the final codepoint
    # only partially present. Pass 'ignore' to acknowledge and ensure this is
    # dropped.
    return encoded.decode('utf-8', 'ignore') + "..."
  return s
