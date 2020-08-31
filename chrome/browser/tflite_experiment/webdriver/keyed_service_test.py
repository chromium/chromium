# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
import os
import json
import statistics
import sys

from common import TestDriver
from common import IntegrationTest

HISTOGRAMS = [
    'TFLiteExperiment.Observer.TFLitePredictor.Null',
    'TFLiteExperiment.Observer.TFLitePredictor.EvaluationRequested',
    'TFLiteExperiment.Observer.Finish',
    'TFLiteExperiment.Observer.TFLitePredictor.InputSetTime',
    'TFLiteExperiment.Observer.TFLitePredictor.EvaluationTime',
    'PageLoad.PaintTiming.NavigationToFirstContentfulPaint',
    'PageLoad.PaintTiming.NavigationToLargestContentfulPaint',
]

URL_LOOP_NUM = 10


class TFLiteKeyedServiceTest(IntegrationTest):
    def _set_tflite_experiment_config(self, test_driver):
        if test_driver._flags.tflite_model:
            test_driver.AddChromeArg("--tflite-model-path=%s" %
                                     test_driver._flags.tflite_model)
        if test_driver._flags.tflite_experiment_log:
            test_driver.AddChromeArg("--tflite-experiment-log-path=%s" %
                                     test_driver._flags.tflite_experiment_log)

        test_driver.AddChromeArg('--tflite-predictor-num-threads=%s' %
                                 str(test_driver._flags.tflite_num_threads))

    # Log histogram for url using test_driver logger.
    def _log_histogram(self, test_driver, histogram, url, is_tflite):
        log_dict = {}
        log_dict["url"] = url
        if is_tflite:
            log_dict['tflite'] = 'true'
        else:
            log_dict['tflite'] = 'false'
        histogram_value = test_driver.GetBrowserHistogram(histogram)
        test_driver._logger.info(histogram_value)
        if not histogram_value:
            return

        bucket_list = []
        for bucket in histogram_value['buckets']:
            bucket_val = (bucket['low'] + bucket['high']) / 2
            for loop in range(bucket['count']):
                bucket_list.append(bucket_val)
        std_val = statistics.stdev(bucket_list)
        mean_val = statistics.mean(bucket_list)
        if histogram == 'TFLiteExperiment.Observer.TFLitePredictor.EvaluationTime':
            log_dict['name'] = 'Evaluation Std'
            log_dict['value'] = std_val
            test_driver._logger.info(log_dict)
            log_dict['name'] = 'Evaluation Mean'
            log_dict['value'] = mean_val
            test_driver._logger.info(log_dict)
        elif histogram == 'TFLiteExperiment.Observer.TFLitePredictor.InputSetTime':
            log_dict['name'] = 'InputSet Std'
            log_dict['value'] = std_val
            test_driver._logger.info(log_dict)
            log_dict['name'] = 'InputSet Mean'
            log_dict['value'] = mean_val
            test_driver._logger.info(log_dict)
        elif histogram == 'PageLoad.PaintTiming.NavigationToFirstContentfulPaint':
            log_dict['name'] = 'FirstContent Std'
            log_dict['value'] = std_val
            test_driver._logger.info(log_dict)
            log_dict['name'] = 'FirstContent Mean'
            log_dict['value'] = mean_val
            test_driver._logger.info(log_dict)
        elif histogram == 'PageLoad.PaintTiming.NavigationToLargestContentfulPaint':
            log_dict['name'] = 'LargestContent Std'
            log_dict['value'] = std_val
            test_driver._logger.info(log_dict)
            log_dict['name'] = 'LargestContent Mean'
            log_dict['value'] = mean_val
            test_driver._logger.info(log_dict)

    def _get_url_list(self, test_driver):
        if not test_driver:
            return []
        if not test_driver._flags.url_list:
            return []

        url_list = []
        with open(test_driver._flags.url_list, 'r') as f:
            line = f.readline()
            while (line):
                url_list.append(line)
                line = f.readline()
        return url_list

    def _open_new_tab(self, test_driver, url):
        test_driver._driver.execute_script('window.open(' ');')
        test_driver._driver.switch_to.window(
            test_driver._driver.window_handles[1])
        test_driver._driver.get(url)
        test_driver._driver.switch_to.window(
            test_driver._driver.window_handles[0])
        test_driver._driver.close()
        test_driver._driver.switch_to.window(
            test_driver._driver.window_handles[0])

    # Records content load timing.
    def _keyed_service_test(self, tflite_enabled):
        with TestDriver() as test_driver:
            url_list = self._get_url_list(test_driver)
            for url in url_list:
                with TestDriver() as test_driver:
                    if tflite_enabled:
                        self._set_tflite_experiment_config(test_driver)
                    test_driver.ClearCache()
                    test_driver.LoadURL(url, timeout=120)
                    for loop in range(URL_LOOP_NUM):
                        self._open_new_tab(test_driver, url)
                    for histogram in HISTOGRAMS:
                        self._log_histogram(test_driver,
                                            histogram,
                                            url,
                                            is_tflite=tflite_enabled)

    # Records content load timing when TFLite Keyed Service
    # is enabled.
    def test_with_tflite(self):
        self._keyed_service_test(tflite_enabled=True)

    # Records content load timing when TFLite Keyed Service
    # is disabled.
    def test_no_tflite(self):
        self._keyed_service_test(tflite_enabled=False)

    # Records content load timing with running
    # with and without tflite.
    def test_in_order(self):
        url_list = self._get_url_list(TestDriver())

        for url in url_list:
            # Test with TFLite.
            with TestDriver() as test_driver_1:
                self._set_tflite_experiment_config(test_driver_1)
                test_driver_1.ClearCache()
                test_driver_1.LoadURL(url, timeout=120)
                for loop in range(URL_LOOP_NUM):
                    self._open_new_tab(test_driver_1, url)
                for histogram in HISTOGRAMS:
                    self._log_histogram(test_driver_1,
                                        histogram,
                                        url,
                                        is_tflite=True)
            # Test without TFLite.
            with TestDriver() as test_driver_2:
                test_driver_2.ClearCache()
                test_driver_2.LoadURL(url, timeout=120)
                for loop in range(URL_LOOP_NUM):
                    self._open_new_tab(test_driver_2, url)
                for histogram in HISTOGRAMS:
                    self._log_histogram(test_driver_2,
                                        histogram,
                                        url,
                                        is_tflite=False)


if __name__ == '__main__':
    IntegrationTest.RunAllTests()
