#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reports binary size metrics for LaCrOS build artifacts.

More information at //docs/speed/binary_size/metrics.md.
"""

import argparse
import collections
import contextlib
import json
import logging
import os
import sys


@contextlib.contextmanager
def _SysPath(path):
  """Library import context that temporarily appends |path| to |sys.path|."""
  if path and path not in sys.path:
    sys.path.insert(0, path)
  else:
    path = None  # Indicates that |sys.path| is not modified.
  try:
    yield
  finally:
    if path:
      sys.path.pop(0)


DIR_SOURCE_ROOT = os.environ.get(
    'CHECKOUT_SOURCE_ROOT',
    os.path.abspath(
        os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)))

BUILD_COMMON_PATH = os.path.join(DIR_SOURCE_ROOT, 'build', 'util', 'lib',
                                 'common')

TRACING_PATH = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'catapult',
                            'tracing')

with _SysPath(BUILD_COMMON_PATH):
  import perf_tests_results_helper  # pylint: disable=import-error

with _SysPath(TRACING_PATH):
  from tracing.value import convert_chart_json  # pylint: disable=import-error

_BASE_CHART = {
    'format_version': '0.1',
    'benchmark_name': 'resource_sizes',
    'benchmark_description': 'LaCrOS resource size information.',
    'trace_rerun_options': [],
    'charts': {}
}

_Item = collections.namedtuple('_Item', ['paths', 'title'])

# This list should be synched with chromeos-amd64-generic-lacros-rel builder
# contents, specified in
# //infra/config/subprojects/chromium/master-only/ci.star
_TRACKED_ITEMS = [
    _Item(paths=['chrome'], title='File: chrome'),
    _Item(paths=['crashpad_handler'], title='File: crashpad_handler'),
    _Item(paths=['icudtl.dat'], title='File: icudtl.dat'),
    _Item(paths=['nacl_helper'], title='File: nacl_helper'),
    _Item(paths=['nacl_irt_x86_64.nexe'], title='File: nacl_irt_x86_64.nexe'),
    _Item(paths=['resources.pak'], title='File: resources.pak'),
    _Item(paths=[
        'chrome_100_percent.pak', 'chrome_200_percent.pak', 'headless_lib.pak'
    ],
          title='Group: Other PAKs'),
    _Item(paths=['snapshot_blob.bin'], title='Group: Misc'),
    _Item(paths=['locales/'], title='Dir: locales'),
    _Item(paths=['swiftshader/'], title='Dir: swiftshader'),
]


def _get_single_filesize(filename):
  """Returns the size of a file, or 0 if file is not found."""
  try:
    return os.path.getsize(filename)
  except OSError:
    logging.critical('Failed to get size: %s', filename)
  return 0


def _get_total_pathsize(base_dir, paths):
  """Computes total file sizes given by a list of paths, best-effort.

  Args:
    base_dir: Base directory for all elements in |paths|.
    paths: A list of filenames or directory names to specify files whose sizes
      to be counted. Directories are recursed. There's no de-duping effort.
      Non-existing files or directories are ignored (with warning message).
  """
  total_size = 0
  for path in paths:
    full_path = os.path.join(base_dir, path)
    if os.path.exists(full_path):
      if os.path.isdir(full_path):
        for dirpath, _, filenames in os.walk(full_path):
          for filename in filenames:
            total_size += _get_single_filesize(os.path.join(dirpath, filename))
      else:  # Assume is file.
        total_size += _get_single_filesize(full_path)
    else:
      logging.critical('Not found: %s', path)
  return total_size


def _dump_chart_json(output_dir, chartjson):
  """Writes chart histogram to JSON files.

  Output files:
    results-chart.json contains the chart JSON.
    perf_results.json contains histogram JSON for Catapult.

  Args:
    output_dir: Directory to place the JSON files.
    chartjson: Source JSON data for output files.
  """
  results_path = os.path.join(output_dir, 'results-chart.json')
  logging.critical('Dumping chartjson to %s', results_path)
  with open(results_path, 'w') as json_file:
    json.dump(chartjson, json_file, indent=2)

  # We would ideally generate a histogram set directly instead of generating
  # chartjson then converting. However, perf_tests_results_helper is in
  # //build, which doesn't seem to have any precedent for depending on
  # anything in Catapult. This can probably be fixed, but since this doesn't
  # need to be super fast or anything, converting is a good enough solution
  # for the time being.
  histogram_result = convert_chart_json.ConvertChartJson(results_path)
  if histogram_result.returncode != 0:
    raise Exception('chartjson conversion failed with error: ' +
                    histogram_result.stdout)

  histogram_path = os.path.join(output_dir, 'perf_results.json')
  logging.critical('Dumping histograms to %s', histogram_path)
  with open(histogram_path, 'w') as json_file:
    json_file.write(histogram_result.stdout)


def _run_resource_sizes(args):
  """Main flow to extract and output size data."""
  chartjson = _BASE_CHART.copy()
  for item in _TRACKED_ITEMS:
    total_size = _get_total_pathsize(args.out_dir, item.paths)
    perf_tests_results_helper.ReportPerfResult(chart_data=chartjson,
                                               graph_title=item.title,
                                               trace_title='size',
                                               value=total_size,
                                               units='bytes')
  _dump_chart_json(args.output_dir, chartjson)


def main():
  """Parses arguments and runs high level flows."""
  argparser = argparse.ArgumentParser(description='Writes LaCrOS size metrics.')

  argparser.add_argument('--chromium-output-directory',
                         dest='out_dir',
                         required=True,
                         type=os.path.realpath,
                         help='Location of the build artifacts.')

  output_group = argparser.add_mutually_exclusive_group()

  output_group.add_argument('--output-dir',
                            default='.',
                            help='Directory to save chartjson to.')

  # Accepted to conform to the isolated script interface, but ignored.
  argparser.add_argument('--isolated-script-test-filter',
                         help=argparse.SUPPRESS)
  argparser.add_argument('--isolated-script-test-perf-output',
                         type=os.path.realpath,
                         help=argparse.SUPPRESS)

  output_group.add_argument(
      '--isolated-script-test-output',
      type=os.path.realpath,
      help='File to which results will be written in the simplified JSON '
      'output format.')

  args = argparser.parse_args()

  isolated_script_output = {'valid': False, 'failures': []}
  if args.isolated_script_test_output:
    test_name = 'lacros_resource_sizes'
    args.output_dir = os.path.join(
        os.path.dirname(args.isolated_script_test_output), test_name)
    if not os.path.exists(args.output_dir):
      os.makedirs(args.output_dir)

  try:
    _run_resource_sizes(args)
    isolated_script_output = {'valid': True, 'failures': []}
  finally:
    if args.isolated_script_test_output:
      results_path = os.path.join(args.output_dir, 'test_results.json')
      with open(results_path, 'w') as output_file:
        json.dump(isolated_script_output, output_file)
      with open(args.isolated_script_test_output, 'w') as output_file:
        json.dump(isolated_script_output, output_file)


if __name__ == '__main__':
  main()
