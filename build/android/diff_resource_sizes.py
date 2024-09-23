#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs resource_sizes.py on two apks and outputs the diff."""


import argparse
import json
import logging
import os
import subprocess
import sys

from pylib.constants import host_paths

with host_paths.SysPath(host_paths.BUILD_UTIL_PATH):
  from lib.common import perf_tests_results_helper

with host_paths.SysPath(host_paths.TRACING_PATH):
  from tracing.value import convert_chart_json # pylint: disable=import-error

_ANDROID_DIR = os.path.dirname(os.path.abspath(__file__))
with host_paths.SysPath(os.path.join(_ANDROID_DIR, 'gyp')):
  from util import build_utils  # pylint: disable=import-error


_BASE_CHART = {
    'format_version': '0.1',
    'benchmark_name': 'resource_sizes_diff',
    'benchmark_description': 'APK resource size diff information',
    'trace_rerun_options': [],
    'charts': {},
}

_CHARTJSON_FILENAME = 'results-chart.json'
_HISTOGRAMS_FILENAME = 'perf_results.json'


def DiffResults(chartjson, base_results, diff_results):
  """Reports the diff between the two given results.

  Args:
    chartjson: A dictionary that chartjson results will be placed in, or None
        to only print results.
    base_results: The chartjson-formatted size results of the base APK.
    diff_results: The chartjson-formatted size results of the diff APK.
  """
  for graph_title, graph in base_results['charts'].items():
    for trace_title, trace in graph.items():
      perf_tests_results_helper.ReportPerfResult(
          chartjson, graph_title, trace_title,
          diff_results['charts'][graph_title][trace_title]['value']
              - trace['value'],
          trace['units'], trace['improvement_direction'],
          trace['important'])


def AddIntermediateResults(chartjson, base_results, diff_results):
  """Copies the intermediate size results into the output chartjson.

  Args:
    chartjson: A dictionary that chartjson results will be placed in.
    base_results: The chartjson-formatted size results of the base APK.
    diff_results: The chartjson-formatted size results of the diff APK.
  """
  for graph_title, graph in base_results['charts'].items():
    for trace_title, trace in graph.items():
      perf_tests_results_helper.ReportPerfResult(
          chartjson, graph_title + '_base_apk', trace_title,
          trace['value'], trace['units'], trace['improvement_direction'],
          trace['important'])

  # Both base_results and diff_results should have the same charts/traces, but
  # loop over them separately in case they don't
  for graph_title, graph in diff_results['charts'].items():
    for trace_title, trace in graph.items():
      perf_tests_results_helper.ReportPerfResult(
          chartjson, graph_title + '_diff_apk', trace_title,
          trace['value'], trace['units'], trace['improvement_direction'],
          trace['important'])


def _CreateArgparser():
  def chromium_path(arg):
    if arg.startswith('//'):
      return os.path.join(host_paths.DIR_SOURCE_ROOT, arg[2:])
    return arg

  argparser = argparse.ArgumentParser(
      description='Diff resource sizes of two APKs. Arguments not listed here '
                  'will be passed on to both invocations of resource_sizes.py.')
  argparser.add_argument('--chromium-output-directory-base',
                         dest='out_dir_base',
                         type=chromium_path,
                         help='Location of the build artifacts for the base '
                              'APK, i.e. what the size increase/decrease will '
                              'be measured from.')
  argparser.add_argument('--chromium-output-directory-diff',
                         dest='out_dir_diff',
                         type=chromium_path,
                         help='Location of the build artifacts for the diff '
                              'APK.')
  argparser.add_argument('--chartjson',
                         action='store_true',
                         help='DEPRECATED. Use --output-format=chartjson '
                              'instead.')
  argparser.add_argument('--output-format',
                         choices=['chartjson', 'histograms'],
                         help='Output the results to a file in the given '
                              'format instead of printing the results.')
  argparser.add_argument('--include-intermediate-results',
                         action='store_true',
                         help='Include the results from the resource_sizes.py '
                              'runs in the chartjson output.')
  argparser.add_argument('--output-dir',
                         default='.',
                         type=chromium_path,
                         help='Directory to save chartjson to.')
  argparser.add_argument('--base-apk',
                         required=True,
                         type=chromium_path,
                         help='Path to the base APK, i.e. what the size '
                              'increase/decrease will be measured from.')
  argparser.add_argument('--diff-apk',
                         required=True,
                         type=chromium_path,
                         help='Path to the diff APK, i.e. the APK whose size '
                              'increase/decrease will be measured against the '
                              'base APK.')
  return argparser


def main():
  args, unknown_args = _CreateArgparser().parse_known_args()
  # TODO(bsheedy): Remove this once all uses of --chartjson are removed.
  if args.chartjson:
    args.output_format = 'chartjson'

  chartjson = _BASE_CHART.copy() if args.output_format else None

  with build_utils.TempDir() as base_dir, build_utils.TempDir() as diff_dir:
    # Run resource_sizes.py on the two APKs
    resource_sizes_path = os.path.join(_ANDROID_DIR, 'resource_sizes.py')
    shared_args = (['python', resource_sizes_path, '--output-format=chartjson']
                   + unknown_args)

    base_args = shared_args + ['--output-dir', base_dir, args.base_apk]
    if args.out_dir_base:
      base_args += ['--chromium-output-directory', args.out_dir_base]
    try:
      subprocess.check_output(base_args, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
      print(e.output)
      raise

    diff_args = shared_args + ['--output-dir', diff_dir, args.diff_apk]
    if args.out_dir_diff:
      diff_args += ['--chromium-output-directory', args.out_dir_diff]
    try:
      subprocess.check_output(diff_args, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
      print(e.output)
      raise

    # Combine the separate results
    with open(os.path.join(base_dir, _CHARTJSON_FILENAME)) as base_file:
      base_results = json.load(base_file)
    with open(os.path.join(diff_dir, _CHARTJSON_FILENAME)) as diff_file:
      diff_results = json.load(diff_file)
    DiffResults(chartjson, base_results, diff_results)
    if args.include_intermediate_results:
      AddIntermediateResults(chartjson, base_results, diff_results)

    if args.output_format:
      chartjson_path = os.path.join(os.path.abspath(args.output_dir),
                                    _CHARTJSON_FILENAME)
      logging.critical('Dumping diff chartjson to %s', chartjson_path)
      with open(chartjson_path, 'w') as outfile:
        json.dump(chartjson, outfile)

      if args.output_format == 'histograms':
        histogram_result = convert_chart_json.ConvertChartJson(chartjson_path)
        if histogram_result.returncode != 0:
          logging.error('chartjson conversion failed with error: %s',
              histogram_result.stdout)
          return 1

        histogram_path = os.path.join(os.path.abspath(args.output_dir),
            'perf_results.json')
        logging.critical('Dumping diff histograms to %s', histogram_path)
        with open(histogram_path, 'w') as json_file:
          json_file.write(histogram_result.stdout)
  return 0


if __name__ == '__main__':
  sys.exit(main())
