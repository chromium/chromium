# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for Skia Perf dashboard-related code."""

import datetime
import hashlib
import json
import logging
import posixpath
import pprint
import queue
import shutil
import statistics
import subprocess
import sys

import constants
import metrics
import results

sys.path.append(str(constants.CHROMIUM_SRC))
from agents.common import tempfile_ext

BUCKET_SUBDIR = 'ingest'


class SkiaPerfMetricReporter:
    """Encapsulates state to upload to a Skia Perf-based dashboard."""

    def __init__(self, git_revision: str, bucket: str, build_id: str,
                 builder: str, builder_group: str, build_number: int):
        """
        Args:
            git_revision: The current Chromium git revision being tested.
            bucket: The GCS bucket to upload data to.
            build_id: The Buildbucket ID of the current build.
            builder: The name of the builder the tests are running on.
            builder_group: The name of the group the builder belongs to.
            build_number: The build number of the current build.
        """
        self._git_revision = git_revision
        self._bucket = bucket
        self._build_id = build_id
        self._builder = builder
        self._builder_group = builder_group
        self._build_number = build_number

        self._metrics_to_upload = queue.Queue()
        self._pprinter = pprint.PrettyPrinter(indent=2)

    def queue_result_for_upload(self, test_result: results.TestResult) -> None:
        """Queues data from a TestResult for upload at a later time.

        Args:
            test_result: A TestResult instance containing the result to queue.
        """
        for ir in test_result.iteration_results:
            logging.debug('Queueing metrics: %s',
                          self._pprinter.pformat(ir.metrics))
            self._metrics_to_upload.put(
                metrics.IterationMetrics(config=test_result.config,
                                         metrics=ir.metrics))

    def upload_queued_metrics(self) -> None:
        """Merges and uploads all queued metric data."""
        metrics_to_upload = []
        while not self._metrics_to_upload.empty():
            metrics_to_upload.append(self._metrics_to_upload.get())

        dashboard_json = self._create_dashboard_json(metrics_to_upload)
        try:
            self._upload_dashboard_json(dashboard_json)
        except Exception as e:
            # These tests are primarily meant to be functional tests, so make
            # a failure to upload non-fatal.
            logging.error('Error occurred while uploading to bucket %s: %s',
                          self._bucket, e)

    def _create_dashboard_json(
            self, metrics_to_upload: list[metrics.IterationMetrics]) -> dict:
        """Converts |metrics_to_upload| into dashboard-compatible JSON.

        See
        https://skia.googlesource.com/buildbot/+/refs/heads/main/perf/FORMAT.md
        for documentation on the dashboard format.

        Returns:
            A dict containing the merged data from |metrics_to_upload|. The
            returned dict is directly uploadable to the perf dashboard when
            encoded as JSON.
        """
        merged_metrics = metrics.merge_metrics(metrics_to_upload)
        dashboard_json = {
            'version': 1,
            'git_hash': self._git_revision,
            'key': {
                'benchmark': 'gcli_prompt_eval',
                'builder_group': self._builder_group,
                'builder': self._builder,
            },
            'results': [],
            'links': {
                'build': f'https://ci.chromium.org/b/{self._build_id}',
            },
        }
        for test_name, metrics_mapping in merged_metrics.items():
            for metric_name, metric_values in metrics_mapping.items():
                result = {
                    'key': {
                        'test': test_name,
                        'metric': metric_name
                    },
                    'measurements': {
                        'stat':
                        _generate_stats_for_metric_values(metric_values),
                    },
                }
                dashboard_json['results'].append(result)

        return dashboard_json

    def _upload_dashboard_json(self, dashboard_json: dict) -> None:
        """Uploads dashboard JSOn to a GCS bucket for ingestion.

        Args:
            dashboard_json: Valid perf dashboard JSON to upload. See
                _create_dashboard_json for more details.
        """
        gsutil = shutil.which('gsutil.py')
        if not gsutil:
            raise RuntimeError(
                'Unable to find gsutil.py. Is depot_tools in PATH?')

        # Ensure that the filename is unique in GCS.
        json_contents = json.dumps(dashboard_json, sort_keys=True)
        content_hash = hashlib.sha1(json_contents.encode('utf-8')).hexdigest()
        gcs_filename = f'{content_hash}.json'

        now = datetime.datetime.now(tz=datetime.timezone.utc)
        # YYYY/MM/DD/HH
        timestamp_path_component = now.strftime('%Y/%m/%d/%H')
        gcs_path = posixpath.join(
            f'gs://{self._bucket}',
            BUCKET_SUBDIR,
            timestamp_path_component,
            self._builder_group,
            self._builder,
            str(self._build_number),
            gcs_filename,
        )

        with tempfile_ext.mkstemp_closed() as json_file:
            with open(json_file, 'w', encoding='utf-8') as outfile:
                outfile.write(json_contents)

            cmd = [
                sys.executable,
                gsutil,
                'cp',
                json_file,
                gcs_path,
            ]
            subprocess.run(cmd, check=True)


def _generate_stats_for_metric_values(
        metric_values: list[float]) -> list[dict[str, str | float]]:
    """Generates statistics for a list of metric values.

    Args:
        metric_values: A list of raw recorded metric values.

    Returns:
        A dashboard JSON-compatible list of common statistics, each statistic
        in the form:
        {
          'value': 'statistic_name',
          'measurement': value,
        }
    """
    sorted_values = sorted(metric_values)
    stats = [
        {
            'value': 'min',
            'measurement': sorted_values[0],
        },
        {
            'value': 'max',
            'measurement': sorted_values[-1],
        },
        {
            'value': 'median',
            'measurement': statistics.median(sorted_values),
        },
        {
            'value': 'mean',
            'measurement': statistics.fmean(sorted_values),
        },
    ]
    return stats
