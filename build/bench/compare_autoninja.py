#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import getpass
import io
import pathlib
import subprocess
import json
import sys

_DURATION_FIELDS = [
    'duration',
    'action_start',
    'queue',
    'exec',
    'utime',
    'stime',
]
_MERGED_FIELDS = ['output', 'rule', 'action', 'gn_target']
_PROTO = 'chrome.ops.chrome_browser_build.CombinedMetrics'


def error(*args, **kwargs):
  print(*args, **kwargs, file=sys.stderr)
  exit(1)


def main(args):
  # Check errors early because you don't want to wait an hour for the compile
  # to complete only to have an error occur.
  if args.output_file.suffix not in ['.recordio', '.json']:
    error('--output-file must be either .recordio or .json')
  for out_dir in args.out_dirs:
    if not out_dir.is_dir():
      error(f'Output directory {out_dir} does not exist')

  if args.offline:
    # Offline builds won't trigger cache hits, so we don't need to worry.
    remote_args = ['-offline']
  else:
    # For online builds, we want consistency, so we ensure that all actions run
    # on remote execution, and we ignore cache hits.
    remote_args = ['-re_cache_enable_read=false', '-strict_remote']

  if args.compile:
    if args.clean:
      # Clean *all* output directories first since clean can fail.
      for out_dir in args.out_dirs:
        print('Cleaning', out_dir)
        subprocess.run(['gn', 'clean', out_dir], check=True)

    for out_dir in args.out_dirs:
      command = [
          'autoninja', '-C', out_dir, '-metrics_json', '.siso_metrics.jsonl',
          *remote_args, *args.remainder
      ]

      print(f'Running for {out_dir}: {" ".join(map(str, command))}')
      subprocess.run(command, check=True)

  merged_metrics = {}
  for out_dir in args.out_dirs:
    metric_file = out_dir / '.siso_metrics.jsonl'
    print(f'Processing {metric_file}')
    with open(metric_file, 'r') as f:
      for line in f:
        step = json.loads(line)
        if 'output' not in step:
          continue
        output = merged_metrics.get(step['output'], None)
        if output is None:
          output = merged_metrics[step['output']] = {}
          output['metrics'] = []
          for field in _MERGED_FIELDS:
            if field in step:
              output[field] = step[field]
        metrics = {
            # These are measured in seconds, to 2 decimal places.
            f'{field}_millis': round(step[field] * 1000)
            for field in _DURATION_FIELDS if field in step
        }
        output['metrics'].append(metrics)

  if args.output_file.suffix == '.recordio':
    stdin = io.StringIO()
    for line in merged_metrics.values():
      json.dump(line, stdin)
      stdin.write('\n')

    encoded = f"encode.JsonToBinaryProto('{_PROTO}', value)"
    cmd = [
        # God this is painful. We read as a csv purely so it will split on
        # newlines, but have to completely change the CSV behaviour so it reads
        # properly
        'gqui',
        'from',
        'csv:-',
        '--csv_field_delimiter_char=\x1f',
        '--csv_input_use_literal_quotes_mode',
        '--csv_input_first_line_is_header=false',
        '--csv_input_columns=value',
        # If there are any entries that failed to parse, you can instead do the
        # following to find them.
        # f"SELECT value WHERE {encoded} == NULL",
        f"SELECT {encoded}",
        "format '%r'",
        f'--outfile=recordio:{args.output_file}',
    ]
    subprocess.run(
        cmd,
        check=True,
        text=True,
        input=stdin.getvalue(),
    )
    user = getpass.getuser()
    name = args.output_file.name
    print(f'''
Combined metrics written to {args.output_file}
There are several useful ways to consume this file:
Option 1 (for simple queries):
  `gqui proto chrome.ops.chrome_browser_build.CombinedMetrics from {args.output_file}`
Option 2 (for real sql queries):
  `/google/bin/releases/googlesql-devtools/execute_query/execute_query --table_spec=t=recordio:chrome.ops.chrome_browser_build.CombinedMetrics:{args.output_file} 'SELECT * FROM t'`
Option 3 (for PLX):
  1) Get a directory readable by f1-query-access (go/f1-data-access)
    Easiest way: `fileutil chmod -R 755 /cns/vn-d/home/{user}`
  2) Put the file on CNS
    `fileutil cp {args.output_file} /cns/vn-d/home/{user}/{name}.recordio`
  3) Write a plx script (template: https://plx.corp.google.com/scripts2/script_1e._9c361e_4d93_42fb_a2c1_7a7a90a48a66)
''')
  elif args.output_file.suffix == '.json':
    with args.output_file.open('w') as f:
      json.dump(list(merged_metrics.values()), f)
      f.write('\n')
    print(f'Combined metrics written to {args.output_file}')
  else:
    # Should have been checked at the start, this should be unreachable.
    assert False


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description=
      'Generate a combined siso_metrics.json for several invocations ' +
      'across different configurations.', )
  parser.add_argument(
      '-C',
      action='append',
      dest='out_dirs',
      required=True,
      help='Output directory to build in. Can be specified multiple times.',
      type=pathlib.Path)
  parser.add_argument('--no-clean',
                      action='store_false',
                      dest='clean',
                      help='Run gn clean before compiling.')
  parser.add_argument('--no-compile',
                      action='store_false',
                      dest='compile',
                      help='Do not run the compile step. ' +
                      'Use existing siso_metrics.jsonl files instead.')
  parser.add_argument('--offline',
                      action='store_true',
                      help='Run the build without remote execution')
  parser.add_argument(
      'remainder',
      nargs=argparse.REMAINDER,
      help='Arguments to pass to autoninja (e.g., targets like chrome)')
  parser.add_argument(
      '--output-file',
      '-o',
      type=pathlib.Path,
      help='Path to write the combined metrics recordio file.',
      required=True,
  )

  main(parser.parse_args())
