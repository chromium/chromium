#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A wrapper script for upload_to_google_storage_first_class.py.

Usage:

# Uploads file and generates .sha256 file
$ ./test_data.py upload data/trace.pftrace

# Generates deps entry for a single file
$ ./test_data.py get_deps data/trace.pftrace

# Generate full deps entry for all files in base/tracing/test/data/
$ ./test_data.py get_all_deps

The upload command uploads the given file to the gs://perfetto bucket and
generates a .sha256 file in the base/tracing/test/data/ directory,
which is rolled to the Perfetto repository.
The .sha256 file is used by Perfetto to download the files with their
own test_data download script (third_party/perfetto/tools/test_data).

The script outputs a GCS entry which should be manually added to the
deps dict in /DEPS. See
https://chromium.googlesource.com/chromium/src/+/HEAD/docs/gcs_dependencies.md.

The files are uploaded as gs://perfetto/test_data/file_name-a1b2c3f4.
"""

import argparse
import os
import subprocess
import sys
import json
import re

SRC_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
TEST_DATA_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), 'data'))
DEPOT_TOOLS_PATH = os.path.abspath(os.path.join(SRC_PATH, 'third_party', 'depot_tools'))
sys.path.append(DEPOT_TOOLS_PATH)
from upload_to_google_storage_first_class import get_sha256sum
from download_from_google_storage import Gsutil, GSUTIL_DEFAULT_PATH


# Write .sha256 file to test/data_sha256 to be rolled into Perfetto.
def write_sha256_file(filepath: str):
  sha256sum = get_sha256sum(filepath)
  sha256_filepath = os.path.abspath(os.path.join(
    os.path.dirname(filepath),
    '..',
    'data_sha256',
    os.path.basename(filepath) + '.sha256'))
  with open(sha256_filepath, 'w') as sha_file:
    sha_file.write(sha256sum)
  return sha256sum


# Run `upload_to_google_storage_first_class.py --bucket perfetto <file>`.
def upload_file(filepath: str, dry_run: bool, force: bool):
  sha256sum = write_sha256_file(filepath)

  # Perfetto uses 'test_data/file_name-a1b2c3f4' object name format.
  object_name = '%s/%s-%s' % ('test_data', os.path.basename(filepath), sha256sum)

  tool = 'upload_to_google_storage_first_class.py'
  command = [tool, '--bucket', 'perfetto', '--object-name', object_name, filepath]
  if dry_run:
    command.append('--dry-run')
  if force:
    command.append('--force')

  completed_process = subprocess.run(
      command,
      check=False,
      capture_output=True)

  if completed_process.returncode == 0:
    print('Manually add the deps entry below to the DEPS file. See '
          'https://chromium.googlesource.com/chromium/src/+/HEAD/docs/gcs_dependencies.md '
          'for more details. Run `test_data.py get_all_deps` to get the full deps entry.')
    sys.stdout.buffer.write(completed_process.stdout)
  else:
    sys.stderr.buffer.write(completed_process.stderr)


# Generate the deps entry for `filepath`, assuming it has been uploaded already.
def generate_deps_entry(filepath: str):
  sha256sum = get_sha256sum(filepath)
  object_name = '%s/%s-%s' % ('test_data', os.path.basename(filepath), sha256sum)

  # Run `gcloud storage ls -L gs://perfetto/test_data/file_name-a1b2c3f4` to
  # get the 'generation' and 'size_bytes' fields for the deps entry
  gsutil = Gsutil(GSUTIL_DEFAULT_PATH)
  gsutil_args = ['ls', '-L', 'gs://perfetto/%s' % object_name]
  code, out, err = gsutil.check_call(*gsutil_args)
  if code != 0:
    raise Exception(code, err + ' ' + object_name)
  generation = int(out.split()[out.split().index('Generation:') + 1])
  size = int(out.split()[out.split().index('Content-Length:') + 1])

  return {
    'object_name': object_name,
    'sha256sum': sha256sum,
    'size_bytes': size,
    'generation': generation,
    'output_file': os.path.basename(filepath),
  }


# Generate the full deps entry for Perfetto test data
def generate_all_deps():
  sha256_path = os.path.join(SRC_PATH, 'base/tracing/test/data_sha256')
  data_path = os.path.join(SRC_PATH, 'base/tracing/test/data')
  objects = []
  for file in os.listdir(sha256_path):
    if file.endswith('.sha256'):
      filepath = os.path.join(data_path, file)[:-7]
      assert os.path.isfile(filepath), 'File does not exist'
      object_entry = generate_deps_entry(filepath)
      objects.append(object_entry)
  return {
    'src/base/tracing/test/data': {
            'bucket':
            'perfetto',
            'objects': objects,
            'dep_type':
            'gcs',
    },
  }


def main():
  parser = argparse.ArgumentParser()
  subparsers = parser.add_subparsers(dest='cmd')

  upload_parser = subparsers.add_parser('upload', help='Upload a file to gs://perfetto')
  upload_parser.add_argument('filepath', help='Path to file you want to upload')
  upload_parser.add_argument('--dry-run', action='store_true',
                             help='Check if file already exists on GCS without '
                             'uploading it and output DEP blob.')
  upload_parser.add_argument('-f',
                             '--force',
                             action='store_true',
                             help='Force upload even if remote file exists.')

  get_deps_parser = subparsers.add_parser('get_deps', help='Print deps entry for a single file')
  get_deps_parser.add_argument('filepath', help='Path to test data file you want the deps entry for.')

  subparsers.add_parser('get_all_deps', help='Print deps entry for all files in `base/tracing/test/data/`')

  args = parser.parse_args()

  if args.cmd == 'get_all_deps':
    print(json.dumps(generate_all_deps(), indent=2).replace('"', "'"))
    return

  filepath = os.path.abspath(args.filepath)
  assert os.path.dirname(filepath) == TEST_DATA_PATH, ('You can only '
                            'upload files in base/tracing/test/data/')

  if args.cmd == 'upload':
    upload_file(filepath, args.dry_run, args.force)
  elif args.cmd == 'get_deps':
    print(json.dumps(generate_deps_entry(filepath), indent=2).replace('"', "'"))

if __name__ == '__main__':
  sys.exit(main())