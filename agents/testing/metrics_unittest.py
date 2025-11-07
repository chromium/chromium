#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for the metrics module."""

import unittest
from unittest import mock

import eval_config
import metrics

# pylint: disable=protected-access


class MergeMetricsUnittest(unittest.TestCase):

    def test_empty_list(self):
        self.assertEqual(metrics.merge_metrics([]), {})

    def test_single_iteration_metric(self):
        config = mock.Mock(spec=eval_config.TestConfig)
        config.src_relative_test_file = 'test.yaml'
        iteration_metrics = [
            metrics.IterationMetrics(
                config=config,
                metrics={
                    'a': 1.0,
                },
            ),
        ]
        self.assertEqual(
            metrics.merge_metrics(iteration_metrics),
            {
                'test.yaml': {
                    'a': [1.0],
                },
            },
        )

    def test_multiple_iteration_metrics_same_test(self):
        config = mock.Mock(spec=eval_config.TestConfig)
        config.src_relative_test_file = 'test.yaml'
        iteration_metrics = [
            metrics.IterationMetrics(
                config=config,
                metrics={
                    'a': 1.0,
                },
            ),
            metrics.IterationMetrics(
                config=config,
                metrics={
                    'a': 2.0,
                },
            ),
        ]
        self.assertEqual(
            metrics.merge_metrics(iteration_metrics),
            {
                'test.yaml': {
                    'a': [1.0, 2.0],
                },
            },
        )

    def test_multiple_iteration_metrics_different_tests(self):
        config1 = mock.Mock(spec=eval_config.TestConfig)
        config1.src_relative_test_file = 'test1.yaml'
        config2 = mock.Mock(spec=eval_config.TestConfig)
        config2.src_relative_test_file = 'test2.yaml'
        iteration_metrics = [
            metrics.IterationMetrics(
                config=config1,
                metrics={
                    'a': 1.0,
                },
            ),
            metrics.IterationMetrics(
                config=config2,
                metrics={
                    'b': 2.0,
                },
            ),
        ]
        self.assertEqual(
            metrics.merge_metrics(iteration_metrics),
            {
                'test1.yaml': {
                    'a': [1.0]
                },
                'test2.yaml': {
                    'b': [2.0]
                }
            },
        )


class IterateOverNestedMetricsUnittest(unittest.TestCase):

    def test_empty_dict(self):
        self.assertEqual(list(metrics.iterate_over_nested_metrics({})), [])

    def test_flat_dict(self):
        flat_dict = {
            'a': 1.0,
            'b': 2.0,
        }
        self.assertCountEqual(
            list(metrics.iterate_over_nested_metrics(flat_dict)), [
                ('a', 1.0),
                ('b', 2.0),
            ])

    def test_nested_dict(self):
        nested_dict = {
            'a': {
                'b': 1.0,
            },
            'c': 2.0,
        }
        self.assertCountEqual(
            list(metrics.iterate_over_nested_metrics(nested_dict)), [
                ('a.b', 1.0),
                ('c', 2.0),
            ])

    def test_deeply_nested_dict(self):
        deeply_nested_dict = {
            'a': {
                'b': {
                    'c': 1.0,
                },
            },
            'd': 2.0,
        }
        self.assertCountEqual(
            list(metrics.iterate_over_nested_metrics(deeply_nested_dict)), [
                ('a.b.c', 1.0),
                ('d', 2.0),
            ])


if __name__ == '__main__':
    unittest.main()
