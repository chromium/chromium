#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for workers."""

import json
import pathlib
import queue
import shutil
import subprocess
import time
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

import promptfoo_installation
import results
import workers

# pylint: disable=protected-access

_POLLING_INTERVAL = 0.001


class WorkDirUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for the WorkDir class."""

    def setUp(self):
        self.setUpPyfakefs()
        self.fs.create_dir('/tmp/src')
        self._setUpPatches()

    def _setUpPatches(self):
        """Set up patches for the tests."""
        rmtree_patcher = mock.patch('shutil.rmtree')
        self.mock_rmtree = rmtree_patcher.start()
        self.addCleanup(rmtree_patcher.stop)

        call_patcher = mock.patch('subprocess.call')
        self.mock_call = call_patcher.start()
        self.addCleanup(call_patcher.stop)

        check_call_patcher = mock.patch('subprocess.check_call')
        self.mock_check_call = check_call_patcher.start()
        self.addCleanup(check_call_patcher.stop)

        check_btrfs_patcher = mock.patch('checkout_helpers.check_btrfs')
        self.mock_check_btrfs = check_btrfs_patcher.start()
        self.addCleanup(check_btrfs_patcher.stop)

    def test_enter_btrfs(self):
        """Tests that a btrfs snapshot is created when btrfs is true."""
        self.mock_check_btrfs.return_value = True
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=False,
                                  verbose=False,
                                  force=False)
        with workdir as w:
            self.assertEqual(w, workdir)

        self.mock_check_call.assert_called_once_with(
            [
                'btrfs',
                'subvol',
                'snapshot',
                pathlib.Path('/tmp/src'),
                pathlib.Path('/tmp/workdir'),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
        )

    def test_enter_no_btrfs(self):
        """Tests that gclient-new-workdir is called when btrfs is false."""
        self.mock_check_btrfs.return_value = False
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=False,
                                  verbose=False,
                                  force=False)
        with workdir as w:
            self.assertEqual(w, workdir)

        self.mock_check_call.assert_called_once_with(
            [
                'gclient-new-workdir.py',
                pathlib.Path('/tmp/src'),
                pathlib.Path('/tmp/workdir'),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
        )

    def test_enter_verbose(self):
        """Tests that verbose logging is enabled when verbose is true."""
        self.mock_check_btrfs.return_value = False
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=False,
                                  verbose=True,
                                  force=False)
        with workdir as w:
            self.assertEqual(w, workdir)

        self.mock_check_call.assert_called_once_with(
            [
                'gclient-new-workdir.py',
                pathlib.Path('/tmp/src'),
                pathlib.Path('/tmp/workdir'),
            ],
            stdout=None,  # Output surfaced instead of going to DEVNULL.
            stderr=subprocess.STDOUT,
        )

    def test_enter_exists_no_force(self):
        """Tests that an error is raised if the workdir exists."""
        self.fs.create_dir('/tmp/workdir')
        self.mock_check_btrfs.return_value = False
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=False,
                                  verbose=False,
                                  force=False)
        with self.assertRaises(FileExistsError):
            with workdir:
                pass

    def test_enter_exists_force(self):
        """Tests that the workdir is removed if it exists and force is on."""
        self.fs.create_dir('/tmp/workdir')
        self.mock_check_btrfs.return_value = True
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=False,
                                  verbose=False,
                                  force=True)
        with workdir:
            pass

        self.mock_call.assert_called_once_with(
            [
                'sudo',
                '-n',
                'btrfs',
                'subvolume',
                'delete',
                pathlib.Path('/tmp/workdir'),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
        )

    def test_exit_clean_btrfs(self):
        """Tests that the workdir is removed when clean is true w/ btrfs ."""
        self.mock_check_btrfs.return_value = True
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=True,
                                  verbose=False,
                                  force=False)
        with workdir:
            pass

        self.mock_call.assert_called_once_with(
            [
                'sudo',
                'btrfs',
                'subvolume',
                'delete',
                pathlib.Path('/tmp/workdir'),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
        )

    def test_exit_clean_no_btrfs(self):
        """Tests that the workdir is removed when clean is True w/o btrfs."""
        self.mock_check_btrfs.return_value = False
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=True,
                                  verbose=False,
                                  force=False)
        with workdir:
            pass

        self.mock_rmtree.assert_called_once_with(pathlib.Path('/tmp/workdir'))

    def test_exit_no_clean(self):
        """Tests that the workdir is not cleaned up when clean is False."""
        self.mock_check_btrfs.return_value = False
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=False,
                                  verbose=False,
                                  force=False)
        with workdir:
            pass

        self.mock_call.assert_not_called()
        self.mock_rmtree.assert_not_called()


    def test_exit_clean_btrfs_fallback(self):
        """Tests that shutil is used when btrfs subvolume delete fails."""
        self.mock_check_btrfs.return_value = True
        self.mock_call.return_value = 1
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=True,
                                  verbose=False,
                                  force=False)
        with workdir:
            pass

        self.mock_call.assert_called_once_with(
            [
                'sudo',
                'btrfs',
                'subvolume',
                'delete',
                pathlib.Path('/tmp/workdir'),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
        )
        self.mock_rmtree.assert_called_once_with(pathlib.Path('/tmp/workdir'))


class ExtractMetricsUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for the _extract_metrics_from_promptfoo_results."""

    def setUp(self):
        self.setUpPyfakefs()

    def test_success(self):
        """Tests a successful extraction."""
        results_data = {
            'results': {
                'results': [
                    {
                        'score': 0.5,
                        'response': {
                            'metrics': {
                                'gemini_cli_token_usage': {
                                    'total_tokens': 10,
                                },
                            },
                        },
                    },
                ],
            },
        }
        results_file = pathlib.Path('/results.json')
        self.fs.create_file(results_file, contents=json.dumps(results_data))
        metrics = workers._extract_metrics_from_promptfoo_results(results_file)
        self.assertEqual(metrics, {
            'token_usage': {
                'total_tokens': 10
            },
            'score': 0.5
        })

    def test_no_score(self):
        """Tests when the score is missing."""
        results_data = {
            'results': {
                'results': [
                    {
                        'response': {
                            'metrics': {
                                'gemini_cli_token_usage': {
                                    'total_tokens': 10,
                                },
                            },
                        },
                    },
                ],
            },
        }
        results_file = pathlib.Path('/results.json')
        self.fs.create_file(results_file, contents=json.dumps(results_data))
        metrics = workers._extract_metrics_from_promptfoo_results(results_file)
        self.assertEqual(metrics, {'token_usage': {'total_tokens': 10}})

    def test_no_token_usage(self):
        """Tests when token usage is missing."""
        results_data = {
            'results': {
                'results': [
                    {
                        'score': 0.5,
                        'response': {
                            'metrics': {},
                        },
                    },
                ],
            },
        }
        results_file = pathlib.Path('/results.json')
        self.fs.create_file(results_file, contents=json.dumps(results_data))
        metrics = workers._extract_metrics_from_promptfoo_results(results_file)
        self.assertEqual(metrics, {'token_usage': {}, 'score': 0.5})

    def test_empty_results(self):
        """Tests when the results file is empty."""
        results_file = pathlib.Path('/results.json')
        self.fs.create_file(results_file, contents='')
        metrics = workers._extract_metrics_from_promptfoo_results(results_file)
        self.assertEqual(metrics, {})


class LoadPromptfooResultsUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for the _load_promptfoo_results."""

    def setUp(self):
        self.setUpPyfakefs()

    def test_success(self):
        """Tests a successful load."""
        results_data = {
            'results': {
                'results': [],
            },
        }
        results_content = json.dumps(results_data)
        results_file = pathlib.Path('/results.json')
        self.fs.create_file(results_file, contents=results_content)
        data = workers._load_promptfoo_results(results_file)
        self.assertEqual(data, results_data)

    def test_invalid_json(self):
        """Tests with invalid JSON content."""
        results_file = pathlib.Path('/results.json')
        self.fs.create_file(results_file, contents='{invalid json')
        with self.assertLogs(level='ERROR') as cm:
            data = workers._load_promptfoo_results(results_file)
            self.assertIn('Error when parsing promptfoo results', cm.output[0])
        self.assertEqual(data, {})

    def test_unicode_error(self):
        """Tests with invalid unicode content."""
        results_file = pathlib.Path('/results.json')
        with open(results_file, 'wb') as f:
            f.write(b'\x80')
        with self.assertLogs(level='ERROR') as cm:
            data = workers._load_promptfoo_results(results_file)
            self.assertIn('Error when parsing promptfoo results', cm.output[0])
        self.assertEqual(data, {})


class ExtractTokenUsageUnittest(unittest.TestCase):
    """Unit tests for the _extract_token_usage_from_promptfoo_results."""

    def test_success(self):
        """Tests a successful extraction."""
        results_data = {
            'results': {
                'results': [
                    {
                        'response': {
                            'metrics': {
                                'gemini_cli_token_usage': {
                                    'total_tokens': 10,
                                    'prompt_tokens': 5,
                                    'completion_tokens': 5,
                                },
                            },
                        },
                    },
                ],
            },
        }
        token_usage = workers._extract_token_usage_from_promptfoo_results(
            results_data)
        self.assertEqual(token_usage, {
            'total_tokens': 10,
            'prompt_tokens': 5,
            'completion_tokens': 5,
        })

    def test_no_results_key(self):
        """Tests when the top-level 'results' key is missing."""
        with self.assertLogs(level='ERROR') as cm:
            token_usage = workers._extract_token_usage_from_promptfoo_results(
                {})
            self.assertIn('Did not find promptfoo result information',
                          cm.output[0])
        self.assertEqual(token_usage, {})

    def test_no_nested_results_key(self):
        """Tests when the nested 'results' key is missing."""
        with self.assertLogs(level='ERROR') as cm:
            token_usage = workers._extract_token_usage_from_promptfoo_results({
                'results': {},
            })
            self.assertIn('Did not find promptfoo result information',
                          cm.output[0])
        self.assertEqual(token_usage, {})

    def test_empty_results_list(self):
        """Tests when the results list is empty."""
        results_data = {
            'results': {
                'results': [],
            },
        }
        with self.assertLogs(level='ERROR') as cm:
            token_usage = workers._extract_token_usage_from_promptfoo_results(
                results_data)
            self.assertIn('Did not find promptfoo result information',
                          cm.output[0])
        self.assertEqual(token_usage, {})

    def test_multiple_results(self):
        """Tests that only the first result is used when there are many."""
        results_data = {
            'results': {
                'results': [
                    {
                        'response': {
                            'metrics': {
                                'gemini_cli_token_usage': {
                                    'total_tokens': 10,
                                },
                            },
                        },
                    },
                    {
                        'response': {
                            'metrics': {
                                'gemini_cli_token_usage': {
                                    'total_tokens': 20,
                                },
                            },
                        },
                    },
                ],
            },
        }
        with self.assertLogs(level='WARNING') as cm:
            token_usage = workers._extract_token_usage_from_promptfoo_results(
                results_data)
            self.assertIn('Unexpectedly got 2 results', cm.output[0])
        self.assertEqual(token_usage, {'total_tokens': 10})

    def test_no_response_key(self):
        """Tests when the 'response' key is missing."""
        results_data = {
            'results': {
                'results': [
                    {},
                ],
            },
        }
        with self.assertLogs(level='WARNING') as cm:
            token_usage = workers._extract_token_usage_from_promptfoo_results(
                results_data)
            self.assertIn('Did not find gemini-cli token usage', cm.output[0])
        self.assertEqual(token_usage, {})

    def test_no_metrics_key(self):
        """Tests when the 'metrics' key is missing."""
        results_data = {
            'results': {
                'results': [
                    {
                        'response': {},
                    },
                ],
            },
        }
        with self.assertLogs(level='WARNING') as cm:
            token_usage = workers._extract_token_usage_from_promptfoo_results(
                results_data)
            self.assertIn('Did not find gemini-cli token usage', cm.output[0])
        self.assertEqual(token_usage, {})

    def test_no_token_usage_key(self):
        """Tests when the 'gemini_cli_token_usage' key is missing."""
        results_data = {
            'results': {
                'results': [
                    {
                        'response': {
                            'metrics': {},
                        },
                    },
                ],
            },
        }
        with self.assertLogs(level='WARNING') as cm:
            token_usage = workers._extract_token_usage_from_promptfoo_results(
                results_data)
            self.assertIn('Did not find gemini-cli token usage', cm.output[0])
        self.assertEqual(token_usage, {})

    def test_empty_token_usage_dict(self):
        """Tests when the token usage dict is empty."""
        results_data = {
            'results': {
                'results': [
                    {
                        'response': {
                            'metrics': {
                                'gemini_cli_token_usage': {},
                            },
                        },
                    },
                ],
            },
        }
        with self.assertLogs(level='WARNING') as cm:
            token_usage = workers._extract_token_usage_from_promptfoo_results(
                results_data)
            self.assertIn('Did not find gemini-cli token usage', cm.output[0])
        self.assertEqual(token_usage, {})


class ExtractScoreUnittest(unittest.TestCase):
    """Unit tests for the _extract_score_from_promptfoo_results."""

    def test_success(self):
        """Tests a successful extraction."""
        results_data = {
            'results': {
                'results': [
                    {
                        'score': 0.5,
                    },
                ],
            },
        }
        score = workers._extract_score_from_promptfoo_results(results_data)
        self.assertEqual(score, 0.5)

    def test_no_score(self):
        """Tests when the score is missing."""
        results_data = {
            'results': {
                'results': [
                    {},
                ],
            },
        }
        with self.assertLogs(level='WARNING') as cm:
            score = workers._extract_score_from_promptfoo_results(results_data)
            self.assertIn('Did not find reported score', cm.output[0])
        self.assertIsNone(score)

    def test_multiple_results(self):
        """Tests that only the first result is used when there are many."""
        results_data = {
            'results': {
                'results': [
                    {
                        'score': 0.5,
                    },
                    {
                        'score': 1.0,
                    },
                ],
            },
        }
        with self.assertLogs(level='WARNING') as cm:
            score = workers._extract_score_from_promptfoo_results(results_data)
            self.assertIn('Unexpectedly got 2 results', cm.output[0])
        self.assertEqual(score, 0.5)

    def test_empty_results(self):
        """Tests when the results list is empty."""
        results_data = {
            'results': {
                'results': [],
            },
        }
        with self.assertLogs(level='ERROR') as cm:
            score = workers._extract_score_from_promptfoo_results(results_data)
            self.assertIn('Did not find promptfoo result information',
                          cm.output[0])
        self.assertIsNone(score)


class WorkerThreadUnittest(unittest.TestCase):
    """Unit tests for the WorkerThread class."""

    def setUp(self):
        self._setUpMocks()
        self._setUpPatches()

    def _setUpMocks(self):
        """Set up mocks for the tests."""
        self.mock_promptfoo = mock.Mock(
            spec=promptfoo_installation.PromptfooInstallation)
        self.mock_promptfoo.run.return_value = subprocess.CompletedProcess(
            args=[], returncode=0, stdout='Success')
        self.worker_options = workers.WorkerOptions(
            clean=True,
            verbose=False,
            force=False,
            sandbox=False,
            gemini_cli_bin=None,
        )
        self.test_input_queue = queue.Queue()
        self.test_result_queue = queue.Queue()

    def _setUpPatches(self):
        """Set up patches for the tests."""
        workdir_patcher = mock.patch('workers.WorkDir')
        self.mock_workdir = workdir_patcher.start()
        mock_workdir_instance = (
            self.mock_workdir.return_value.__enter__.return_value)
        mock_workdir_instance.path = pathlib.Path('/workdir')
        self.addCleanup(workdir_patcher.stop)

        polling_patcher = mock.patch(
            'workers._AVAILABLE_TEST_POLLING_SLEEP_DURATION',
            _POLLING_INTERVAL)
        polling_patcher.start()
        self.addCleanup(polling_patcher.stop)

        get_gclient_root_patcher = mock.patch(
            'checkout_helpers.get_gclient_root')
        self.mock_get_gclient_root = get_gclient_root_patcher.start()
        self.mock_get_gclient_root.return_value = pathlib.Path('/root')
        self.addCleanup(get_gclient_root_patcher.stop)

    def _create_and_run_worker(self, test_paths):
        """Helper to create and run a worker thread."""
        for path in test_paths:
            self.test_input_queue.put(path)

        worker = workers.WorkerThread(
            worker_index=0,
            promptfoo=self.mock_promptfoo,
            worker_options=self.worker_options,
            test_input_queue=self.test_input_queue,
            test_result_queue=self.test_result_queue,
        )
        worker.start()

        while self.test_result_queue.qsize() < len(test_paths):
            worker.maybe_reraise_fatal_exception()
            time.sleep(_POLLING_INTERVAL)

        worker.shutdown()
        worker.join(1)
        return worker

    def test_run_one_test_pass(self):
        """Tests running a single passing test."""
        test_path = pathlib.Path('/test/a.yaml')
        self._create_and_run_worker([test_path])

        self.mock_workdir.assert_called_once_with('workdir-0',
                                                  pathlib.Path('/root'), True,
                                                  False, False)
        self.mock_promptfoo.run.assert_called_once()
        self.assertEqual(self.test_result_queue.qsize(), 1)
        result = self.test_result_queue.get()
        self.assertEqual(result.test_file, test_path)
        self.assertTrue(result.success)

    def test_run_one_test_fail(self):
        """Tests running a single failing test."""
        self.mock_promptfoo.run.return_value = subprocess.CompletedProcess(
            args=[], returncode=1, stdout='Failure')
        test_path = pathlib.Path('/test/a.yaml')
        self._create_and_run_worker([test_path])

        self.assertEqual(self.test_result_queue.qsize(), 1)
        result = self.test_result_queue.get()
        self.assertEqual(result.test_file, test_path)
        self.assertFalse(result.success)

    def test_run_multiple_tests(self):
        """Tests running multiple tests."""
        test_paths = [
            pathlib.Path('/test/a.yaml'),
            pathlib.Path('/test/b.yaml')
        ]
        self._create_and_run_worker(test_paths)

        self.assertEqual(self.mock_workdir.call_count, 2)
        self.assertEqual(self.mock_promptfoo.run.call_count, 2)
        self.assertEqual(self.test_result_queue.qsize(), 2)

    def test_shutdown(self):
        """Tests that the worker thread shuts down gracefully."""
        worker = self._create_and_run_worker([])
        self.assertFalse(worker.is_alive())

    def test_fatal_exception(self):
        """Tests that fatal exceptions are propagated."""
        worker = self._create_and_run_worker([])
        with mock.patch.object(worker,
                               '_run_incoming_tests_until_shutdown',
                               side_effect=ValueError('Test Error')):
            worker.run()

        with self.assertRaisesRegex(ValueError, 'Test Error'):
            worker.maybe_reraise_fatal_exception()

    def test_no_fatal_exception(self):
        """Tests that no exception is raised when there is no fatal error."""
        worker = self._create_and_run_worker([])
        # Should be a no-op.
        worker.maybe_reraise_fatal_exception()

    def test_sandbox_and_verbose(self):
        """Tests that sandbox and verbose flags are passed to promptfoo."""
        self.worker_options.sandbox = True
        self.worker_options.verbose = True
        self._create_and_run_worker([pathlib.Path('/test/a.yaml')])

        self.mock_promptfoo.run.assert_called_once()
        command = self.mock_promptfoo.run.call_args[0][0]
        self.assertIn('--var', command)
        self.assertIn('sandbox=True', command)
        self.assertIn('verbose=True', command)
        self.assertIn(f'console_width={shutil.get_terminal_size().columns}',
                      command)


    def test_gemini_cli_bin(self):
        """Tests that gemini_cli_bin is passed to promptfoo."""
        gemini_cli_bin = pathlib.Path('/', 'custom', 'gemini')
        self.worker_options.gemini_cli_bin = gemini_cli_bin
        self._create_and_run_worker([pathlib.Path('/test/a.yaml')])

        self.mock_promptfoo.run.assert_called_once()
        command = self.mock_promptfoo.run.call_args[0][0]
        self.assertIn('--var', command)
        self.assertIn(f'gemini_cli_bin={gemini_cli_bin}', command)


class WorkerPoolUnittest(unittest.TestCase):
    """Unit tests for the WorkerPool class."""

    def setUp(self):
        self._setUpMocks()
        self._setUpPatches()

    def _setUpMocks(self):
        """Set up mocks for the tests."""
        self.mock_promptfoo = mock.Mock(
            spec=promptfoo_installation.PromptfooInstallation)
        self.worker_options = workers.WorkerOptions(
            clean=True,
            verbose=False,
            force=False,
            sandbox=False,
        )

    def _setUpPatches(self):
        """Set up patches for the tests."""

        def create_thread_join_side_effect(mock_thread):

            def thread_join_side_effect(*args, **kwargs):
                # pylint: disable=unused-argument
                mock_thread.is_alive.return_value = False

            return thread_join_side_effect

        atomic_counter_patcher = mock.patch('workers.results.AtomicCounter')
        self.mock_atomic_counter = atomic_counter_patcher.start()
        self.addCleanup(atomic_counter_patcher.stop)

        result_thread_patcher = mock.patch('workers.results.ResultThread')
        self.mock_result_thread = result_thread_patcher.start()
        mock_result_thread_instance = self.mock_result_thread.return_value
        mock_result_thread_instance.is_alive.return_value = True
        mock_result_thread_instance.join.side_effect = (
            create_thread_join_side_effect(mock_result_thread_instance))
        mock_result_thread_instance.total_results_reported = (
            self.mock_atomic_counter.return_value)
        mock_result_thread_instance.failed_result_output_queue = mock.Mock(
            spec=queue.Queue)
        self.addCleanup(result_thread_patcher.stop)

        worker_thread_patcher = mock.patch('workers.WorkerThread')
        self.mock_worker_thread = worker_thread_patcher.start()
        mock_worker_thread_instance = self.mock_worker_thread.return_value
        mock_worker_thread_instance.is_alive.return_value = True
        mock_worker_thread_instance.join.side_effect = (
            create_thread_join_side_effect(mock_worker_thread_instance))
        self.addCleanup(worker_thread_patcher.stop)

        polling_patcher = mock.patch(
            'workers._ALL_QUEUED_TESTS_RUN_POLLING_SLEEP_DURATION',
            _POLLING_INTERVAL)
        polling_patcher.start()
        self.addCleanup(polling_patcher.stop)

    def test_create_pool(self):
        """Tests that the pool is created with the correct number of workers."""
        pool = workers.WorkerPool(
            num_workers=3,
            promptfoo=self.mock_promptfoo,
            worker_options=self.worker_options,
            print_output_on_success=False,
        )
        self.assertEqual(self.mock_worker_thread.call_count, 3)
        self.mock_result_thread.assert_called_once()
        pool.shutdown_blocking(1)

    def test_queue_tests(self):
        """Tests that tests are queued correctly."""
        pool = workers.WorkerPool(
            num_workers=1,
            promptfoo=self.mock_promptfoo,
            worker_options=self.worker_options,
            print_output_on_success=False,
        )
        test_paths = [
            pathlib.Path('/test/a.yaml'),
            pathlib.Path('/test/b.yaml')
        ]
        pool.queue_tests(test_paths)
        self.assertEqual(pool._test_input_queue.qsize(), 2)
        pool.shutdown_blocking(1)

    def test_wait_for_all_queued_tests(self):
        """Tests that the pool waits for all tests to complete."""
        self.mock_atomic_counter.return_value.get.side_effect = [0, 1, 2]
        pool = workers.WorkerPool(
            num_workers=1,
            promptfoo=self.mock_promptfoo,
            worker_options=self.worker_options,
            print_output_on_success=False,
        )
        test_paths = [
            pathlib.Path('/test/a.yaml'),
            pathlib.Path('/test/b.yaml')
        ]
        pool.queue_tests(test_paths)
        failed_tests = pool.wait_for_all_queued_tests()
        self.assertEqual(len(failed_tests), 0)
        self.assertEqual(self.mock_atomic_counter.return_value.get.call_count,
                         3)
        pool.shutdown_blocking(1)

    def test_wait_for_all_queued_tests_with_failures(self):
        """Tests that failed tests are returned."""
        self.mock_atomic_counter.return_value.get.return_value = 1
        failed_test = results.TestResult(test_file='fail.yaml',
                                         success=False,
                                         duration=1,
                                         test_log='',
                                         metrics={})
        mock_failed_queue = (
            self.mock_result_thread.return_value.failed_result_output_queue)
        mock_failed_queue.empty.side_effect = [False, True]
        mock_failed_queue.get.return_value = failed_test

        pool = workers.WorkerPool(
            num_workers=1,
            promptfoo=self.mock_promptfoo,
            worker_options=self.worker_options,
            print_output_on_success=False,
        )
        pool.queue_tests([pathlib.Path('fail.yaml')])
        failed_tests = pool.wait_for_all_queued_tests()
        self.assertEqual(len(failed_tests), 1)
        self.assertEqual(failed_tests[0], failed_test)
        pool.shutdown_blocking(1)

    def test_shutdown_blocking(self):
        """Tests that shutdown_blocking shuts down all threads."""
        pool = workers.WorkerPool(
            num_workers=2,
            promptfoo=self.mock_promptfoo,
            worker_options=self.worker_options,
            print_output_on_success=False,
        )
        mock_workers = self.mock_worker_thread.return_value
        mock_result = self.mock_result_thread.return_value

        pool.shutdown_blocking(1)

        self.assertEqual(mock_workers.shutdown.call_count, 2)
        mock_result.shutdown.assert_called_once()
        self.assertEqual(mock_workers.join.call_count, 2)
        mock_result.join.assert_called_once()

    def test_shutdown_blocking_timeout(self):
        """Tests that shutdown_blocking logs an error on timeout."""
        pool = workers.WorkerPool(
            num_workers=1,
            promptfoo=self.mock_promptfoo,
            worker_options=self.worker_options,
            print_output_on_success=False,
        )
        self.mock_worker_thread.return_value.join.side_effect = None
        self.mock_worker_thread.return_value.is_alive.return_value = True
        with self.assertLogs(level='ERROR') as cm:
            pool.shutdown_blocking(0.01)
            self.assertIn('Failed to gracefully shut down thread',
                          cm.output[0])

    def test_wait_for_all_queued_tests_with_multiple_workers(self):
        """Tests that the pool waits for all tests with multiple workers."""
        self.mock_atomic_counter.return_value.get.side_effect = [0, 0, 1, 1, 2]
        pool = workers.WorkerPool(
            num_workers=2,
            promptfoo=self.mock_promptfoo,
            worker_options=self.worker_options,
            print_output_on_success=False,
        )
        test_paths = [
            pathlib.Path('/test/a.yaml'),
            pathlib.Path('/test/b.yaml')
        ]
        pool.queue_tests(test_paths)
        failed_tests = pool.wait_for_all_queued_tests()
        self.assertEqual(len(failed_tests), 0)
        self.assertEqual(self.mock_atomic_counter.return_value.get.call_count,
                         5)
        pool.shutdown_blocking(1)

    def test_worker_thread_fatal_exception(self):
        """Tests that a fatal exception in a worker thread is propagated."""
        self.mock_worker_thread.return_value.maybe_reraise_fatal_exception.\
            side_effect = ValueError('Worker Error')
        pool = workers.WorkerPool(
            num_workers=1,
            promptfoo=self.mock_promptfoo,
            worker_options=self.worker_options,
            print_output_on_success=False,
        )
        pool.queue_tests([pathlib.Path('/test/a.yaml')])
        with self.assertRaisesRegex(ValueError, 'Worker Error'):
            pool.wait_for_all_queued_tests()
        pool.shutdown_blocking(1)

    def test_result_thread_fatal_exception(self):
        """Tests that a fatal exception in the result thread is propagated."""
        self.mock_result_thread.return_value.maybe_reraise_fatal_exception.\
            side_effect = ValueError('Result Error')
        pool = workers.WorkerPool(
            num_workers=1,
            promptfoo=self.mock_promptfoo,
            worker_options=self.worker_options,
            print_output_on_success=False,
        )
        pool.queue_tests([pathlib.Path('/test/a.yaml')])
        with self.assertRaisesRegex(ValueError, 'Result Error'):
            pool.wait_for_all_queued_tests()
        pool.shutdown_blocking(1)

    def test_del(self):
        """Tests that the destructor calls shutdown_blocking."""
        pool = workers.WorkerPool(
            num_workers=1,
            promptfoo=self.mock_promptfoo,
            worker_options=self.worker_options,
            print_output_on_success=False,
        )
        shutdown_mock = mock.Mock()
        pool.shutdown_blocking = shutdown_mock
        del pool
        shutdown_mock.assert_called_once_with(2)


if __name__ == '__main__':
    unittest.main()
