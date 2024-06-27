#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
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
import subprocess
import sys
import tempfile
SRC_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(SRC_DIR, 'build', 'util'))
from lib.results import result_sink
from lib.results import result_types


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

BUILD_UTIL_PATH = os.path.join(DIR_SOURCE_ROOT, 'build', 'util')

TRACING_PATH = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'catapult',
                            'tracing')

EU_STRIP_PATH = os.path.join(DIR_SOURCE_ROOT, 'buildtools', 'third_party',
                             'eu-strip', 'bin', 'eu-strip')

with _SysPath(BUILD_UTIL_PATH):
  from lib.common import perf_tests_results_helper

with _SysPath(TRACING_PATH):
  from tracing.value import convert_chart_json  # pylint: disable=import-error

_BASE_CHART = {
    'format_version': '0.1',
    'benchmark_name': 'resource_sizes',
    'trace_rerun_options': [],
    'charts': {}
}

_KEY_RAW = 'raw'
_KEY_GZIPPED = 'gzipped'
_KEY_STRIPPED = 'stripped'
_KEY_STRIPPED_GZIPPED = 'stripped_then_gzipped'


class _Group:
  """A group of build artifacts whose file sizes are summed and tracked.

  Build artifacts for size tracking fall under these categories:
  * File: A single file.
  * Group: A collection of files.
  * Dir: All files under a directory.

  Attributes:
    paths: A list of files or directories to be tracked together.
    title: The display name of the group.
    track_stripped: Whether to also track summed stripped ELF sizes.
    track_compressed: Whether to also track summed compressed sizes.
  """

  def __init__(self, paths, title, track_stripped=False,
               track_compressed=False):
    self.paths = paths
    self.title = title
    self.track_stripped = track_stripped
    self.track_compressed = track_compressed

  def __eq__(self, other):
    """Overrides the default implementation"""
    if isinstance(other, _Group):
      return (self.paths == other.paths) & (self.title == other.title) & (
          self.track_stripped == other.track_stripped) & (
              self.track_compressed == other.track_compressed)
    return False

# Common artifacts in official builder lacros-arm32 and lacros64 in
# src-internal. The artifcts can be found in
# chromium/src-internal/testing/buildbot/archive/lacros64.json and
# chromium/src-internal/testing/buildbot/archive/lacros-arm32.json
# chromium/src-internal/testing/buildbot/archive/lacros-arm64.json
_TRACKED_GROUPS = [
    _Group(paths=['chrome'],
           title='File: chrome',
           track_stripped=True,
           track_compressed=True),
    _Group(paths=['chrome_crashpad_handler'],
           title='File: chrome_crashpad_handler'),
    _Group(paths=['icudtl.dat'], title='File: icudtl.dat'),
    _Group(paths=['icudtl.dat.hash'], title='File: icudtl.dat.hash'),
    _Group(paths=['libEGL.so'], title='File: libEGL.so'),
    _Group(paths=['libGLESv2.so'], title='File: libGLESv2.so'),
    _Group(paths=['nacl_helper'], title='File: nacl_helper'),
    _Group(paths=['resources.pak'], title='File: resources.pak'),
    _Group(paths=[
        'chrome_100_percent.pak', 'chrome_200_percent.pak',
        'headless_lib_data.pak', 'headless_lib_strings.pak'
    ],
           title='Group: Other PAKs'),
    _Group(paths=['snapshot_blob.bin'], title='Group: Misc'),
    _Group(paths=['locales/'], title='Dir: locales'),
    _Group(paths=['PrivacySandboxAttestationsPreloaded/'],
           title='Dir: PrivacySandboxAttestationsPreloaded'),
    _Group(paths=['resources/accessibility/'],
           title='Dir: resources/accessibility'),
    _Group(paths=['WidevineCdm/'], title='Dir: WidevineCdm'),
]


def _visit_paths(base_dir, paths):
  """Itemizes files specified by a list of paths.

  Args:
    base_dir: Base directory for all elements in |paths|.
    paths: A list of filenames or directory names to specify files whose sizes
      to be counted. Directories are recursed. There's no de-duping effort.
      Non-existing files or directories are ignored (with warning message).
  """
  for path in paths:
    full_path = os.path.join(base_dir, path)
    if os.path.exists(full_path):
      if os.path.isdir(full_path):
        for dirpath, _, filenames in os.walk(full_path):
          for filename in filenames:
            yield os.path.join(dirpath, filename)
      else:  # Assume is file.
        yield full_path
    else:
      logging.critical('Not found: %s', path)


def _is_probably_elf(filename):
  """Heuristically decides whether |filename| is ELF via magic signature."""
  with open(filename, 'rb') as fh:
    return fh.read(4) == '\x7FELF'


def _is_unstrippable_elf(filename):
  """Identifies known-unstrippable ELF files to denoise the system."""
  return filename.endswith('.nexe') or filename.endswith('libwidevinecdm.so')


def _get_filesize(filename):
  """Returns the size of a file, or 0 if file is not found."""
  try:
    return os.path.getsize(filename)
  except OSError:
    logging.critical('Failed to get size: %s', filename)
  return 0


def _get_gzipped_filesize(filename):
  """Returns the gzipped size of a file, or 0 if file is not found."""
  BUFFER_SIZE = 65536
  if not os.path.isfile(filename):
    return 0
  try:
    # Call gzip externally instead of using gzip package since it's > 2x faster.
    cmd = ['gzip', '-c', filename]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    # Manually counting bytes instead of using len(p.communicate()[0]) to avoid
    # buffering the entire compressed data (can be ~100 MB).
    ret = 0
    while True:
      chunk = len(p.stdout.read(BUFFER_SIZE))
      if chunk == 0:
        break
      ret += chunk
    return ret
  except OSError:
    logging.critical('Failed to get gzipped size: %s', filename)
  return 0


def _get_catagorized_filesizes(filename):
  """Measures |filename| sizes under various transforms.

  Returns: A Counter (keyed by _Key_* constants) that stores measured sizes.
  """
  sizes = collections.Counter()
  sizes[_KEY_RAW] = _get_filesize(filename)
  sizes[_KEY_GZIPPED] = _get_gzipped_filesize(filename)

  # Pre-assign values for non-ELF, or in case of failure for ELF.
  sizes[_KEY_STRIPPED] = sizes[_KEY_RAW]
  sizes[_KEY_STRIPPED_GZIPPED] = sizes[_KEY_GZIPPED]

  if _is_probably_elf(filename) and not _is_unstrippable_elf(filename):
    try:
      fd, temp_file = tempfile.mkstemp()
      os.close(fd)
      cmd = [EU_STRIP_PATH, filename, '-o', temp_file]
      subprocess.check_output(cmd)
      sizes[_KEY_STRIPPED] = _get_filesize(temp_file)
      sizes[_KEY_STRIPPED_GZIPPED] = _get_gzipped_filesize(temp_file)
      if sizes[_KEY_STRIPPED] > sizes[_KEY_RAW]:
        # This weird case has been observed for libwidevinecdm.so.
        logging.critical('Stripping made things worse for %s' % filename)
    except subprocess.CalledProcessError:
      logging.critical('Failed to strip file: %s' % filename)
    finally:
      os.unlink(temp_file)
  return sizes


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
  with open(histogram_path, 'wb') as json_file:
    json_file.write(histogram_result.stdout)


def _run_resource_sizes(args):
  """Main flow to extract and output size data."""
  chartjson = _BASE_CHART.copy()
  chartjson.update({
      'benchmark_description':
      ('LaCrOS %s resource size information.' % args.arch)
  })
  report_func = perf_tests_results_helper.ReportPerfResult
  total_sizes = collections.Counter()

  def report_sizes(sizes, title, track_stripped, track_compressed):
    report_func(chart_data=chartjson,
                graph_title=title,
                trace_title='size',
                value=sizes[_KEY_RAW],
                units='bytes')

    if track_stripped:
      report_func(chart_data=chartjson,
                  graph_title=title + ' (Stripped)',
                  trace_title='size',
                  value=sizes[_KEY_STRIPPED],
                  units='bytes')

    if track_compressed:
      report_func(chart_data=chartjson,
                  graph_title=title + ' (Gzipped)',
                  trace_title='size',
                  value=sizes[_KEY_GZIPPED],
                  units='bytes')

    if track_stripped and track_compressed:
      report_func(chart_data=chartjson,
                  graph_title=title + ' (Stripped, Gzipped)',
                  trace_title='size',
                  value=sizes[_KEY_STRIPPED_GZIPPED],
                  units='bytes')

  tracked_groups = _TRACKED_GROUPS.copy()
  # Architecture amd64 requires artifact nacl_irt_x86_64.nexe.
  if args.arch == 'amd64':
    tracked_groups.append(
        _Group(paths=['nacl_irt_x86_64.nexe'],
               title='File: nacl_irt_x86_64.nexe'))
  # Architecture arm32 requires artifact nacl_irt_arm.nexe.
  elif args.arch == 'arm32':
    tracked_groups.append(
        _Group(paths=['nacl_irt_arm.nexe'], title='File: nacl_irt_arm.nexe'))
    tracked_groups.append(
        _Group(paths=['nacl_helper_bootstrap'],
               title='File: nacl_helper_bootstrap'))
  # TODO(crbug.com/40236427): remove the following part once nacl files
  # are available.
  elif args.arch == 'arm64':
    tracked_groups.remove(
        _Group(paths=['nacl_helper'], title='File: nacl_helper'))
  for g in tracked_groups:
    sizes = sum(
        map(_get_catagorized_filesizes, _visit_paths(args.out_dir, g.paths)),
        collections.Counter())
    report_sizes(sizes, g.title, g.track_stripped, g.track_compressed)

    # Total compressed size is summed over individual compressed sizes, instead
    # of concatanating first, then compress everything. This is done for
    # simplicity. It also gives a conservative size estimate (assuming file
    # metadata and overheads are negligible).
    total_sizes += sizes

  report_sizes(total_sizes, 'Total', True, True)

  _dump_chart_json(args.output_dir, chartjson)


def main():
  """Parses arguments and runs high level flows."""
  argparser = argparse.ArgumentParser(description='Writes LaCrOS size metrics.')

  argparser.add_argument('--chromium-output-directory',
                         dest='out_dir',
                         required=True,
                         type=os.path.realpath,
                         help='Location of the build artifacts.')
  argparser.add_argument('--arch',
                         required=True,
                         type=str,
                         help='The architecture of lacros, valid values: amd64,'
                         ' arm32, arm64')

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
  result_sink_client = result_sink.TryInitClient()
  if result_sink_client:
    status = result_types.PASS
    if not isolated_script_output['valid']:
      status = result_types.UNKNOWN
    elif isolated_script_output['failures']:
      status = result_types.FAIL
    result_sink_client.Post(test_name, status, None, None, None)


if __name__ == '__main__':
  main()
