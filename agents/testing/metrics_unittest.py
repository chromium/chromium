#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for the metrics module."""

import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

import eval_config
import metrics

# pylint: disable=protected-access


class MergeAndUploadMetricsUnittest(unittest.TestCase):

    def setUp(self):
        self.create_dashboard_json_patcher = mock.patch(
            'metrics._create_dashboard_json')
        self.mock_create_dashboard_json = (
            self.create_dashboard_json_patcher.start())
        self.addCleanup(self.create_dashboard_json_patcher.stop)

        self.upload_dashboard_json_patcher = mock.patch(
            'metrics._upload_dashboard_json')
        self.mock_upload_dashboard_json = (
            self.upload_dashboard_json_patcher.start())
        self.addCleanup(self.upload_dashboard_json_patcher.stop)

    def test_success(self):
        self.mock_create_dashboard_json.return_value = {'foo': 'bar'}
        metrics.merge_and_upload_metrics(
            iteration_metrics=[],
            git_revision='test_revision',
            bucket='test-bucket',
            build_id='123',
            builder='test_builder',
        )
        self.mock_create_dashboard_json.assert_called_once_with(
            [],
            'test_revision',
            '123',
            'test_builder',
        )
        self.mock_upload_dashboard_json.assert_called_once_with(
            {
                'foo': 'bar',
            },
            'test-bucket',
        )

    def test_upload_fails(self):
        self.mock_upload_dashboard_json.side_effect = Exception('test error')
        with self.assertLogs(level='ERROR') as cm:
            metrics.merge_and_upload_metrics(
                iteration_metrics=[],
                git_revision='test_revision',
                bucket='test-bucket',
                build_id='123',
                builder='test_builder',
            )
            self.assertIn('Error occurred while uploading to bucket',
                          cm.output[0])


class CreateDashboardJsonUnittest(unittest.TestCase):

    def setUp(self):
        self.merge_metrics_patcher = mock.patch('metrics._merge_metrics')
        self.mock_merge_metrics = self.merge_metrics_patcher.start()
        self.addCleanup(self.merge_metrics_patcher.stop)

        self.generate_stats_patcher = mock.patch(
            'metrics._generate_stats_for_metric_values')
        self.mock_generate_stats = self.generate_stats_patcher.start()
        self.addCleanup(self.generate_stats_patcher.stop)

    def test_success(self):
        self.mock_merge_metrics.return_value = {
            'test.yaml.a': [1.0, 2.0],
        }
        self.mock_generate_stats.return_value = [
            {
                'value': 'mean',
                'measurement': 1.5,
            },
        ]
        dashboard_json = metrics._create_dashboard_json(
            iteration_metrics=[],
            git_revision='test_revision',
            build_id='123',
            builder='test_builder',
        )
        self.assertEqual(
            dashboard_json, {
                'version':
                1,
                'git_hash':
                'test_revision',
                'key': {
                    'benchmark': 'gcli_prompt_eval',
                    'bot': 'test_builder',
                },
                'results': [
                    {
                        'key': {
                            'test': 'test.yaml.a',
                        },
                        'measurements': {
                            'stat': [
                                {
                                    'value': 'mean',
                                    'measurement': 1.5,
                                },
                            ],
                        },
                    },
                ],
                'links': {
                    'build': 'https://ci.chromium.org/b/123',
                },
            })
        self.mock_merge_metrics.assert_called_once_with([])
        self.mock_generate_stats.assert_called_once_with([1.0, 2.0])


class MergeMetricsUnittest(unittest.TestCase):

    def test_empty_list(self):
        self.assertEqual(metrics._merge_metrics([]), {})

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
        self.assertEqual(metrics._merge_metrics(iteration_metrics), {
            'test.yaml.a': [1.0],
        })

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
        self.assertEqual(metrics._merge_metrics(iteration_metrics), {
            'test.yaml.a': [1.0, 2.0],
        })

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
        self.assertEqual(metrics._merge_metrics(iteration_metrics), {
            'test1.yaml.a': [1.0],
            'test2.yaml.b': [2.0],
        })


class IterateOverNestedMetricsUnittest(unittest.TestCase):

    def test_empty_dict(self):
        self.assertEqual(list(metrics._iterate_over_nested_metrics({})), [])

    def test_flat_dict(self):
        flat_dict = {
            'a': 1.0,
            'b': 2.0,
        }
        self.assertCountEqual(
            list(metrics._iterate_over_nested_metrics(flat_dict)), [
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
            list(metrics._iterate_over_nested_metrics(nested_dict)), [
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
            list(metrics._iterate_over_nested_metrics(deeply_nested_dict)), [
                ('a.b.c', 1.0),
                ('d', 2.0),
            ])


class GenerateStatsForMetricValuesUnittest(unittest.TestCase):

    def test_single_value(self):
        stats = metrics._generate_stats_for_metric_values([5.0])
        self.assertCountEqual(stats, [
            {
                'value': 'min',
                'measurement': 5.0,
            },
            {
                'value': 'max',
                'measurement': 5.0,
            },
            {
                'value': 'median',
                'measurement': 5.0,
            },
            {
                'value': 'mean',
                'measurement': 5.0,
            },
        ])

    def test_multiple_values(self):
        stats = metrics._generate_stats_for_metric_values([1.0, 2.0, 3.0])
        self.assertCountEqual(stats, [
            {
                'value': 'min',
                'measurement': 1.0,
            },
            {
                'value': 'max',
                'measurement': 3.0,
            },
            {
                'value': 'median',
                'measurement': 2.0,
            },
            {
                'value': 'mean',
                'measurement': 2.0,
            },
        ])

    def test_decimal_values(self):
        stats = metrics._generate_stats_for_metric_values([1.5, 2.5, 3.5])
        # Pop the mean value to compare it separately with assertAlmostEqual.
        mean_stat = next(s for s in stats if s['value'] == 'mean')
        stats.remove(mean_stat)
        self.assertAlmostEqual(mean_stat['measurement'], 2.5)

        self.assertCountEqual(stats, [
            {
                'value': 'min',
                'measurement': 1.5,
            },
            {
                'value': 'max',
                'measurement': 3.5,
            },
            {
                'value': 'median',
                'measurement': 2.5,
            },
        ])


class UploadDashboardJsonUnittest(fake_filesystem_unittest.TestCase):

    def setUp(self):
        self.setUpPyfakefs()

        self.which_patcher = mock.patch('shutil.which')
        self.mock_which = self.which_patcher.start()
        self.mock_which.return_value = '/path/to/gsutil.py'
        self.addCleanup(self.which_patcher.stop)

        self.run_patcher = mock.patch('subprocess.run')
        self.mock_run = self.run_patcher.start()
        self.addCleanup(self.run_patcher.stop)

    def test_success(self):
        metrics._upload_dashboard_json({}, 'test-bucket')
        self.mock_run.assert_called_once()
        self.assertIn('/path/to/gsutil.py', self.mock_run.call_args[0][0])
        self.assertRegex(
            self.mock_run.call_args[0][0][-1],
            r'gs://test-bucket/chromium_prompt_eval/\d{4}/\d{2}/'
            r'\d{2}/\d{2}/[a-f0-9]{40}\.json')

    def test_no_gsutil(self):
        self.mock_which.return_value = None
        with self.assertRaisesRegex(RuntimeError, 'Unable to find gsutil.py'):
            metrics._upload_dashboard_json({}, 'test-bucket')


if __name__ == '__main__':
    unittest.main()
