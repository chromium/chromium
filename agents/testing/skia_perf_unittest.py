#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for the skia_perf module."""

import datetime
import pathlib
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

import eval_config
import results
import skia_perf

# pylint: disable=protected-access


class QueueResultForUploadUnittest(unittest.TestCase):

    def setUp(self):
        self.reporter = skia_perf.SkiaPerfMetricReporter(
            git_revision='test_revision',
            bucket='test-bucket',
            build_id='123',
            builder='test_builder',
            builder_group='test_builder_group',
            build_number=1,
        )

    def test_basic(self):
        config = eval_config.TestConfig(test_file=pathlib.Path('test.yaml'))
        test_result = results.TestResult(config=config,
                                         success=True,
                                         iteration_results=[
                                             results.IterationResult(
                                                 success=True,
                                                 duration=1.0,
                                                 test_log='log',
                                                 metrics={
                                                     'a': 1.0,
                                                 },
                                                 prompt=None,
                                                 response=None,
                                             ),
                                         ])
        self.reporter.queue_result_for_upload(test_result)
        self.assertEqual(self.reporter._metrics_to_upload.qsize(), 1)
        iteration_metrics = self.reporter._metrics_to_upload.get()
        self.assertEqual(iteration_metrics.config, config)
        self.assertEqual(iteration_metrics.metrics, {'a': 1.0})


class UploadQueuedMetricsUnittest(unittest.TestCase):

    def setUp(self):
        self.create_dashboard_json_patcher = mock.patch(
            'skia_perf.SkiaPerfMetricReporter._create_dashboard_json')
        self.mock_create_dashboard_json = (
            self.create_dashboard_json_patcher.start())
        self.addCleanup(self.create_dashboard_json_patcher.stop)

        self.upload_dashboard_json_patcher = mock.patch(
            'skia_perf.SkiaPerfMetricReporter._upload_dashboard_json')
        self.mock_upload_dashboard_json = (
            self.upload_dashboard_json_patcher.start())
        self.addCleanup(self.upload_dashboard_json_patcher.stop)

        self.reporter = skia_perf.SkiaPerfMetricReporter(
            git_revision='test_revision',
            bucket='test-bucket',
            build_id='123',
            builder='test_builder',
            builder_group='test_builder_group',
            build_number=1,
        )

    def test_success(self):
        mock_metric_1 = mock.Mock()
        mock_metric_2 = mock.Mock()
        self.reporter._metrics_to_upload.put(mock_metric_1)
        self.reporter._metrics_to_upload.put(mock_metric_2)
        self.mock_create_dashboard_json.return_value = {'foo': 'bar'}
        self.reporter.upload_queued_metrics()
        self.mock_create_dashboard_json.assert_called_once_with(
            [mock_metric_1, mock_metric_2])
        self.mock_upload_dashboard_json.assert_called_once_with({'foo': 'bar'})

    def test_upload_fails(self):
        self.mock_upload_dashboard_json.side_effect = Exception('test error')
        with self.assertLogs(level='ERROR') as cm:
            self.reporter.upload_queued_metrics()
            self.assertIn('Error occurred while uploading to bucket',
                          cm.output[0])


class CreateDashboardJsonUnittest(unittest.TestCase):

    def setUp(self):
        self.merge_metrics_patcher = mock.patch('metrics.merge_metrics')
        self.mock_merge_metrics = self.merge_metrics_patcher.start()
        self.addCleanup(self.merge_metrics_patcher.stop)

        self.generate_stats_patcher = mock.patch(
            'skia_perf._generate_stats_for_metric_values')
        self.mock_generate_stats = self.generate_stats_patcher.start()
        self.addCleanup(self.generate_stats_patcher.stop)

        self.reporter = skia_perf.SkiaPerfMetricReporter(
            git_revision='test_revision',
            bucket='test-bucket',
            build_id='123',
            builder='test_builder',
            builder_group='test_builder_group',
            build_number=1,
        )

    def test_basic(self):
        self.mock_merge_metrics.return_value = {
            'test.yaml': {
                'a': [1.0, 2.0],
            },
        }
        self.mock_generate_stats.return_value = [
            {
                'value': 'mean',
                'measurement': 1.5,
            },
        ]
        dashboard_json = self.reporter._create_dashboard_json([])
        self.assertEqual(
            dashboard_json, {
                'version':
                1,
                'git_hash':
                'test_revision',
                'key': {
                    'benchmark': 'gcli_prompt_eval',
                    'builder_group': 'test_builder_group',
                    'builder': 'test_builder',
                },
                'results': [
                    {
                        'key': {
                            'test': 'test.yaml',
                            'metric': 'a',
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

        self.datetime_patcher = mock.patch('skia_perf.datetime')
        self.mock_dt_module = self.datetime_patcher.start()
        self.addCleanup(self.datetime_patcher.stop)

        self.reporter = skia_perf.SkiaPerfMetricReporter(
            git_revision='test_revision',
            bucket='test-bucket',
            build_id='123',
            builder='test_builder',
            builder_group='test_builder_group',
            build_number=1,
        )

    def test_success(self):
        self.mock_dt_module.datetime.now.return_value = datetime.datetime(
            2025, 10, 29, 12, 30, 0, tzinfo=datetime.timezone.utc)
        self.reporter._upload_dashboard_json({})
        self.mock_run.assert_called_once()
        self.assertIn('/path/to/gsutil.py', self.mock_run.call_args[0][0])
        self.assertRegex(
            self.mock_run.call_args[0][0][-1],
            r'gs://test-bucket/ingest/2025/10/29/12/test_builder_group/'
            r'test_builder/1/[a-f0-9]{40}\.json')

    def test_no_gsutil(self):
        self.mock_which.return_value = None
        with self.assertRaisesRegex(RuntimeError, 'Unable to find gsutil.py'):
            self.reporter._upload_dashboard_json({})


class GenerateStatsForMetricValuesUnittest(unittest.TestCase):

    def test_single_value(self):
        stats = skia_perf._generate_stats_for_metric_values([5.0])
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
        stats = skia_perf._generate_stats_for_metric_values([1.0, 2.0, 3.0])
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
        stats = skia_perf._generate_stats_for_metric_values([1.5, 2.5, 3.5])
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


if __name__ == '__main__':
    unittest.main()
