#!/usr/bin/env vpython3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import unittest

from pylib.base import base_test_result
from pylib.results import json_results


class JsonResultsTest(unittest.TestCase):

  def testGenerateResultsDict_passedResult(self):
    result = base_test_result.BaseTestResult(
        'test.package.TestName', base_test_result.ResultType.PASS)

    all_results = base_test_result.TestRunResults()
    all_results.AddResult(result)

    results_dict = json_results.GenerateResultsDict([all_results])
    self.assertEqual(['test.package.TestName'], results_dict['all_tests'])
    self.assertEqual(1, len(results_dict['per_iteration_data']))

    iteration_result = results_dict['per_iteration_data'][0]
    self.assertTrue('test.package.TestName' in iteration_result)
    self.assertEqual(1, len(iteration_result['test.package.TestName']))

    test_iteration_result = iteration_result['test.package.TestName'][0]
    self.assertTrue('status' in test_iteration_result)
    self.assertEqual('SUCCESS', test_iteration_result['status'])

  def testGenerateResultsDict_skippedResult(self):
    result = base_test_result.BaseTestResult(
        'test.package.TestName', base_test_result.ResultType.SKIP)

    all_results = base_test_result.TestRunResults()
    all_results.AddResult(result)

    results_dict = json_results.GenerateResultsDict([all_results])
    self.assertEqual(['test.package.TestName'], results_dict['all_tests'])
    self.assertEqual(1, len(results_dict['per_iteration_data']))

    iteration_result = results_dict['per_iteration_data'][0]
    self.assertTrue('test.package.TestName' in iteration_result)
    self.assertEqual(1, len(iteration_result['test.package.TestName']))

    test_iteration_result = iteration_result['test.package.TestName'][0]
    self.assertTrue('status' in test_iteration_result)
    self.assertEqual('SKIPPED', test_iteration_result['status'])

  def testGenerateResultsDict_failedResult(self):
    result = base_test_result.BaseTestResult(
        'test.package.TestName', base_test_result.ResultType.FAIL)

    all_results = base_test_result.TestRunResults()
    all_results.AddResult(result)

    results_dict = json_results.GenerateResultsDict([all_results])
    self.assertEqual(['test.package.TestName'], results_dict['all_tests'])
    self.assertEqual(1, len(results_dict['per_iteration_data']))

    iteration_result = results_dict['per_iteration_data'][0]
    self.assertTrue('test.package.TestName' in iteration_result)
    self.assertEqual(1, len(iteration_result['test.package.TestName']))

    test_iteration_result = iteration_result['test.package.TestName'][0]
    self.assertTrue('status' in test_iteration_result)
    self.assertEqual('FAILURE', test_iteration_result['status'])

  def testGenerateResultsDict_duration(self):
    result = base_test_result.BaseTestResult(
        'test.package.TestName', base_test_result.ResultType.PASS, duration=123)

    all_results = base_test_result.TestRunResults()
    all_results.AddResult(result)

    results_dict = json_results.GenerateResultsDict([all_results])
    self.assertEqual(['test.package.TestName'], results_dict['all_tests'])
    self.assertEqual(1, len(results_dict['per_iteration_data']))

    iteration_result = results_dict['per_iteration_data'][0]
    self.assertTrue('test.package.TestName' in iteration_result)
    self.assertEqual(1, len(iteration_result['test.package.TestName']))

    test_iteration_result = iteration_result['test.package.TestName'][0]
    self.assertTrue('elapsed_time_ms' in test_iteration_result)
    self.assertEqual(123, test_iteration_result['elapsed_time_ms'])

  def testGenerateResultsDict_multipleResults(self):
    result1 = base_test_result.BaseTestResult(
        'test.package.TestName1', base_test_result.ResultType.PASS)
    result2 = base_test_result.BaseTestResult(
        'test.package.TestName2', base_test_result.ResultType.PASS)

    all_results = base_test_result.TestRunResults()
    all_results.AddResult(result1)
    all_results.AddResult(result2)

    results_dict = json_results.GenerateResultsDict([all_results])
    self.assertEqual(['test.package.TestName1', 'test.package.TestName2'],
                     results_dict['all_tests'])

    self.assertTrue('per_iteration_data' in results_dict)
    iterations = results_dict['per_iteration_data']
    self.assertEqual(1, len(iterations))

    expected_tests = set([
        'test.package.TestName1',
        'test.package.TestName2',
    ])

    for test_name, iteration_result in iterations[0].items():
      self.assertTrue(test_name in expected_tests)
      expected_tests.remove(test_name)
      self.assertEqual(1, len(iteration_result))

      test_iteration_result = iteration_result[0]
      self.assertTrue('status' in test_iteration_result)
      self.assertEqual('SUCCESS', test_iteration_result['status'])

  def testGenerateResultsDict_passOnRetry(self):
    raw_results = []

    result1 = base_test_result.BaseTestResult(
        'test.package.TestName1', base_test_result.ResultType.FAIL)
    run_results1 = base_test_result.TestRunResults()
    run_results1.AddResult(result1)
    raw_results.append(run_results1)

    result2 = base_test_result.BaseTestResult(
        'test.package.TestName1', base_test_result.ResultType.PASS)
    run_results2 = base_test_result.TestRunResults()
    run_results2.AddResult(result2)
    raw_results.append(run_results2)

    results_dict = json_results.GenerateResultsDict([raw_results])
    self.assertEqual(['test.package.TestName1'], results_dict['all_tests'])

    # Check that there's only one iteration.
    self.assertIn('per_iteration_data', results_dict)
    iterations = results_dict['per_iteration_data']
    self.assertEqual(1, len(iterations))

    # Check that test.package.TestName1 is the only test in the iteration.
    self.assertEqual(1, len(iterations[0]))
    self.assertIn('test.package.TestName1', iterations[0])

    # Check that there are two results for test.package.TestName1.
    actual_test_results = iterations[0]['test.package.TestName1']
    self.assertEqual(2, len(actual_test_results))

    # Check that the first result is a failure.
    self.assertIn('status', actual_test_results[0])
    self.assertEqual('FAILURE', actual_test_results[0]['status'])

    # Check that the second result is a success.
    self.assertIn('status', actual_test_results[1])
    self.assertEqual('SUCCESS', actual_test_results[1]['status'])

  def testGenerateResultsDict_globalTags(self):
    raw_results = []
    global_tags = ['UNRELIABLE_RESULTS']

    results_dict = json_results.GenerateResultsDict(
        [raw_results], global_tags=global_tags)
    self.assertEqual(['UNRELIABLE_RESULTS'], results_dict['global_tags'])

  def testGenerateResultsDict_loslessSnippet(self):
    result = base_test_result.BaseTestResult(
        'test.package.TestName', base_test_result.ResultType.FAIL)
    log = 'blah-blah'
    result.SetLog(log)

    all_results = base_test_result.TestRunResults()
    all_results.AddResult(result)

    results_dict = json_results.GenerateResultsDict([all_results])
    self.assertEqual(['test.package.TestName'], results_dict['all_tests'])
    self.assertEqual(1, len(results_dict['per_iteration_data']))

    iteration_result = results_dict['per_iteration_data'][0]
    self.assertTrue('test.package.TestName' in iteration_result)
    self.assertEqual(1, len(iteration_result['test.package.TestName']))

    test_iteration_result = iteration_result['test.package.TestName'][0]
    self.assertTrue('losless_snippet' in test_iteration_result)
    self.assertTrue(test_iteration_result['losless_snippet'])
    self.assertTrue('output_snippet' in test_iteration_result)
    self.assertEqual(log, test_iteration_result['output_snippet'])
    self.assertTrue('output_snippet_base64' in test_iteration_result)
    self.assertEqual('', test_iteration_result['output_snippet_base64'])

  def testGenerateJsonTestResultFormatDict_passedResult(self):
    result = base_test_result.BaseTestResult('test.package.TestName',
                                             base_test_result.ResultType.PASS)

    all_results = base_test_result.TestRunResults()
    all_results.AddResult(result)

    results_dict = json_results.GenerateJsonTestResultFormatDict([all_results],
                                                                 False)
    self.assertEqual(1, len(results_dict['tests']))
    self.assertEqual(1, len(results_dict['tests']['test']))
    self.assertEqual(1, len(results_dict['tests']['test']['package']))
    self.assertEqual(
        'PASS',
        results_dict['tests']['test']['package']['TestName']['expected'])
    self.assertEqual(
        'PASS', results_dict['tests']['test']['package']['TestName']['actual'])

    self.assertTrue('FAIL' not in results_dict['num_failures_by_type']
                    or results_dict['num_failures_by_type']['FAIL'] == 0)
    self.assertIn('PASS', results_dict['num_failures_by_type'])
    self.assertEqual(1, results_dict['num_failures_by_type']['PASS'])

  def testGenerateJsonTestResultFormatDict_failedResult(self):
    result = base_test_result.BaseTestResult('test.package.TestName',
                                             base_test_result.ResultType.FAIL)

    all_results = base_test_result.TestRunResults()
    all_results.AddResult(result)

    results_dict = json_results.GenerateJsonTestResultFormatDict([all_results],
                                                                 False)
    self.assertEqual(1, len(results_dict['tests']))
    self.assertEqual(1, len(results_dict['tests']['test']))
    self.assertEqual(1, len(results_dict['tests']['test']['package']))
    self.assertEqual(
        'PASS',
        results_dict['tests']['test']['package']['TestName']['expected'])
    self.assertEqual(
        'FAIL', results_dict['tests']['test']['package']['TestName']['actual'])
    self.assertEqual(
        True,
        results_dict['tests']['test']['package']['TestName']['is_unexpected'])

    self.assertTrue('PASS' not in results_dict['num_failures_by_type']
                    or results_dict['num_failures_by_type']['PASS'] == 0)
    self.assertIn('FAIL', results_dict['num_failures_by_type'])
    self.assertEqual(1, results_dict['num_failures_by_type']['FAIL'])

  def testGenerateJsonTestResultFormatDict_skippedResult(self):
    result = base_test_result.BaseTestResult('test.package.TestName',
                                             base_test_result.ResultType.SKIP)

    all_results = base_test_result.TestRunResults()
    all_results.AddResult(result)

    results_dict = json_results.GenerateJsonTestResultFormatDict([all_results],
                                                                 False)
    self.assertEqual(1, len(results_dict['tests']))
    self.assertEqual(1, len(results_dict['tests']['test']))
    self.assertEqual(1, len(results_dict['tests']['test']['package']))
    self.assertEqual(
        'PASS',
        results_dict['tests']['test']['package']['TestName']['expected'])
    self.assertEqual(
        'SKIP', results_dict['tests']['test']['package']['TestName']['actual'])
    # Should only be set if the test fails.
    self.assertNotIn('is_unexpected',
                     results_dict['tests']['test']['package']['TestName'])

    self.assertTrue('FAIL' not in results_dict['num_failures_by_type']
                    or results_dict['num_failures_by_type']['FAIL'] == 0)
    self.assertTrue('PASS' not in results_dict['num_failures_by_type']
                    or results_dict['num_failures_by_type']['PASS'] == 0)
    self.assertIn('SKIP', results_dict['num_failures_by_type'])
    self.assertEqual(1, results_dict['num_failures_by_type']['SKIP'])

  def testGenerateJsonTestResultFormatDict_failedResultWithRetry(self):
    result_1 = base_test_result.BaseTestResult('test.package.TestName',
                                               base_test_result.ResultType.FAIL)
    run_results_1 = base_test_result.TestRunResults()
    run_results_1.AddResult(result_1)

    # Simulate a second retry with failure.
    result_2 = base_test_result.BaseTestResult('test.package.TestName',
                                               base_test_result.ResultType.FAIL)
    run_results_2 = base_test_result.TestRunResults()
    run_results_2.AddResult(result_2)

    all_results = [run_results_1, run_results_2]

    results_dict = json_results.GenerateJsonTestResultFormatDict(
        all_results, False)
    self.assertEqual(1, len(results_dict['tests']))
    self.assertEqual(1, len(results_dict['tests']['test']))
    self.assertEqual(1, len(results_dict['tests']['test']['package']))
    self.assertEqual(
        'PASS',
        results_dict['tests']['test']['package']['TestName']['expected'])
    self.assertEqual(
        'FAIL FAIL',
        results_dict['tests']['test']['package']['TestName']['actual'])
    self.assertEqual(
        True,
        results_dict['tests']['test']['package']['TestName']['is_unexpected'])

    self.assertTrue('PASS' not in results_dict['num_failures_by_type']
                    or results_dict['num_failures_by_type']['PASS'] == 0)
    # According to the spec: If a test was run more than once, only the first
    # invocation's result is included in the totals.
    self.assertIn('FAIL', results_dict['num_failures_by_type'])
    self.assertEqual(1, results_dict['num_failures_by_type']['FAIL'])


if __name__ == '__main__':
  unittest.main(verbosity=2)
