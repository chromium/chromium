# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for metrics-related code, not including uploading."""

from collections.abc import Iterable
import dataclasses
from typing import Generator, TypeAlias, Union

import eval_config

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


def merge_metrics(
    iteration_metrics: Iterable[IterationMetrics]
) -> dict[str, dict[str, list[float]]]:
    """Merges data for the same tests/metric names into a single list.

    Args:
        iteration_metrics: All IterationMetrics from all tests run.

    Returns:
        A dict mapping a unique test/metric name combination to a list of all
        reported values for that combination. In the format:
        {
            'test_1': {
                'metric_1': [value_1, value_2],
                'metric_2': [value_3, value_4],
            },
            'test_2': {
                'metric_1': [value_5, value_6],
                'metric_2': [value_7, value_8],
            },
        }
    """
    merged_metrics = {}
    for im in iteration_metrics:
        config_file = str(im.config.src_relative_test_file)
        for k, v in iterate_over_nested_metrics(im.metrics):
            merged_metrics.setdefault(config_file, {}).setdefault(k,
                                                                  []).append(v)
    return merged_metrics


def iterate_over_nested_metrics(
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
            for inner_k, inner_v in iterate_over_nested_metrics(v):
                yield f'{k}.{inner_k}', inner_v
        else:
            yield k, v
