# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for reporting test results to ResultDB."""

import sys

import base64
import constants
import metrics
import results

sys.path.insert(0, str(constants.CHROMIUM_SRC / 'build' / 'util'))
from lib.results import result_sink
from lib.results import result_types


class ResultDBReporter:
    """Encapsulates state necessary to upload results to ResultDB."""

    def __init__(self):
        self._result_sink_client = result_sink.TryInitClient()

    def report_result(self, test_result: results.TestResult) -> None:
        """Reports data from a TestResult to ResultDB.

        No-op if ResultDB integration is not available.

        Args:
            test_result: A TestResult instance containing the result to report.
        """
        if not self._result_sink_client:
            return

        relative_path = test_result.config.test_file.relative_to(
            constants.CHROMIUM_SRC)
        posix_path = relative_path.as_posix()
        for iteration_result in test_result.iteration_results:
            tags = [(name.replace('.', '_').lower(), str(value))
                    for name, value in metrics.iterate_over_nested_metrics(
                        iteration_result.metrics)]
            tags.extend([('tag', tag) for tag in test_result.config.tags])

            owner = test_result.config.owner
            if owner:
                tags.append(('owner', owner))

            artifacts = {}
            prompt = iteration_result.prompt
            if prompt:
                b64_prompt = base64.b64encode(prompt.encode()).decode()
                artifacts.update({
                    'Prompt': {
                        'contents': b64_prompt,
                        'content_type': 'text/plain',
                    }
                })

            response = iteration_result.response
            if response:
                b64_response = base64.b64encode(response.encode()).decode()
                artifacts.update({
                    'Response': {
                        'contents': b64_response,
                        'content_type': 'text/plain',
                    }
                })

            self._result_sink_client.Post(
                test_id=str(posix_path),
                status=(result_types.PASS
                        if iteration_result.success else result_types.FAIL),
                duration=iteration_result.duration * 1000,
                test_log=iteration_result.test_log,
                test_id_structured={
                    'coarseName': '',  # Leave blank for scheme 'flat'.
                    'fineName': '',  # Leave blank for scheme 'flat'.
                    'caseNameComponents': [str(posix_path)],
                },
                test_file=f'//{str(posix_path)}',
                tags=tags,
                artifacts=artifacts,
            )
