# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for metrics-related code, including uploading."""

from collections.abc import Iterable
import dataclasses
import datetime
import hashlib
import json
import logging
import posixpath
import shutil
import statistics
import subprocess
import sys
from typing import Generator, TypeAlias, Union

import constants
import eval_config

sys.path.append(str(constants.CHROMIUM_SRC))
from agents.common import tempfile_ext

BUCKET_SUBDIR = 'chromium_prompt_eval'

# A mapping of metric name to value. Metric names can be nested, e.g.
# {
#   'token_usage': {
#     'input': 10,
#     'output': 20,
#   },
# }
MetricsMapping: TypeAlias = dict[str, Union['MetricsMapping', float]]


@dataclasses.dataclass
class IterationMetrics:
    """Represents metrics from a single test iteration."""
    # The test config the metrics originated from.
    config: eval_config.TestConfig
    # Metrics collected from the iteration.
    metrics: MetricsMapping


def merge_and_upload_metrics(iteration_metrics: Iterable[IterationMetrics],
                             git_revision: str, bucket: str, build_id: str,
                             builder: str) -> None:
    """Merges collected metrics and uploads them to the perf dashboard.

    Args:
        iteration_metrics: All IterationMetrics from all tests run.
        git_revision: The current Chromium git revision being tested.
        bucket: The GCS bucket to upload data to.
        build_id: The Buildbucket ID of the current build.
        builder: The name of the builder the tests are running on.
    """
    dashboard_json = _create_dashboard_json(iteration_metrics, git_revision,
                                            build_id, builder)
    try:
        _upload_dashboard_json(dashboard_json, bucket)
    except Exception as e:
        # These tests are primarily meant to be functional tests, so make a
        # failure to upload non-fatal.
        logging.error('Error occurred while uploading to bucket %s: %s',
                      bucket, e)


def _create_dashboard_json(iteration_metrics: Iterable[IterationMetrics],
                           git_revision: str, build_id: str,
                           builder: str) -> dict:
    """Merges all data from |iteration_metrics| into dashboard-compatible JSON.

    See https://skia.googlesource.com/buildbot/+/refs/heads/main/perf/FORMAT.md
    for documentation on the dashboard format.

    Args:
        iteration_metrics: All IterationMetrics from all tests run.
        git_revision: The current Chromium git revision being tested.
        build_id: The Buildbucket ID of the current build.
        builder: The name of the builder the tests are running on.

    Returns:
        A dict containing the merged data from |metrics|. The returned dict is
        directly uploadable to the perf dashboard when encoded as JSON.
    """
    merged_metrics = _merge_metrics(iteration_metrics)
    dashboard_json = {
        'version': 1,
        'git_hash': git_revision,
        'key': {
            'benchmark': 'gcli_prompt_eval',
            'bot': builder,
        },
        'results': [],
        'links': {
            'build': f'https://ci.chromium.org/b/{build_id}',
        },
    }
    for metric_name, metric_values in merged_metrics.items():
        result = {
            'key': {
                'test': metric_name,
            },
            'measurements': {
                'stat': _generate_stats_for_metric_values(metric_values),
            },
        }
        dashboard_json['results'].append(result)

    return dashboard_json


def _merge_metrics(
        iteration_metrics: Iterable[IterationMetrics]
) -> dict[str, list[float]]:
    """Merges data for the same tests/metric names into a single list.

    Args:
        iteration_metrics: All IterationMetrics from all tests run.

    Returns:
        A dict mapping a unique test/metric name combination to a list of all
        reported values for that combination.
    """
    merged_metrics = {}
    for im in iteration_metrics:
        config_file = str(im.config.src_relative_test_file)
        for k, v in _iterate_over_nested_metrics(im.metrics):
            merged_metrics.setdefault(f'{config_file}.{k}', []).append(v)
    return merged_metrics


def _iterate_over_nested_metrics(
        metrics: MetricsMapping) -> Generator[tuple[str, float], None, None]:
    """Iterates over all potentially nested elements of a MetricsMapping.

    If a particular value is a nested MetricsMapping, this is called
    recursively on the nested value.

    Args:
        metrics: A MetricsMapping to iterate over.

    Yields:
        A tuple (name, value). |name| is a string containing the name of the
        metric, while |value| is a float containing the value of that metric.
        In the event that metrics are nested, each nested name is joined by a .

        For example, iterating over:

        {
          'token_usage': {
            'input': 10,
          },
          'score': 1.0,
        }

        would yield ('token_usage.input', 10) and ('score', 1.0)
    """
    for k, v in metrics.items():
        if isinstance(v, dict):
            for inner_k, inner_v in _iterate_over_nested_metrics(v):
                yield f'{k}.{inner_k}', inner_v
        else:
            yield k, v


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


def _upload_dashboard_json(dashboard_json: dict, bucket: str) -> None:
    """Uploads dashboard JSON to a GCS bucket for ingestion.

    Args:
        dashboard_json: Valid perf dashboard JSON to upload. See
            _create_dashboard_json for more details.
        bucket: The GCS bucket to upload to.
    """
    # Should be provided by depot_tools.
    gsutil = shutil.which('gsutil.py')
    if not gsutil:
        raise RuntimeError('Unable to find gsutil.py. Is depot_tools in PATH?')

    # Ensure that the filename is unique in GCS.
    json_contents = json.dumps(dashboard_json, sort_keys=True)
    content_hash = hashlib.sha1(json_contents.encode('utf-8')).hexdigest()
    gcs_filename = f'{content_hash}.json'

    now = datetime.datetime.now(tz=datetime.timezone.utc)
    # YYYY/MM/DD/HH
    timestamp_path_component = now.strftime('%Y/%m/%d/%M')
    gcs_path = posixpath.join(f'gs://{bucket}', BUCKET_SUBDIR,
                              timestamp_path_component, gcs_filename)

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
