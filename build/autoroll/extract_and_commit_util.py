# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extracts the zipfile from the cipd instance and commits it to the repo."""

import argparse
import os
import pathlib
import shutil
import subprocess
import sys
from typing import List
import zipfile

_SRC_PATH = pathlib.Path(__file__).resolve().parents[2]
_TO_COMMIT_ZIP_NAME = 'to_commit.zip'
_CHROMIUM_SRC_PREFIX = 'CHROMIUM_SRC'


def _HasChanges(repo):
  output = subprocess.check_output(
      ['git', '-C', repo, 'status', '--porcelain=v1'])
  return bool(output)


def main(committed_dir_path):
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument(
      '--cipd-package-path', required=True, help='Path to cipd package.')
  parser.add_argument(
      '--no-git-add',
      action='store_false',
      dest='git_add',
      help='Whether to git add the extracted files.')
  options = parser.parse_args()

  cipd_package_path = pathlib.Path(options.cipd_package_path)
  to_commit_zip_path = cipd_package_path / _TO_COMMIT_ZIP_NAME
  if not to_commit_zip_path.exists():
    print(f'No zipfile found at {to_commit_zip_path}', file=sys.stderr)
    print('Doing nothing', file=sys.stderr)
    return
  if os.path.exists(committed_dir_path):
    # Delete original contents.
    shutil.rmtree(committed_dir_path)
  os.makedirs(committed_dir_path)
  # Replace with the contents of the zip.
  with zipfile.ZipFile(to_commit_zip_path) as z:
    z.extractall(committed_dir_path)
  changed_file_paths: List[str] = [
      str(committed_dir_path.relative_to(_SRC_PATH))
  ]

  committed_chromium_src_dir = committed_dir_path / _CHROMIUM_SRC_PREFIX
  for root, _, files in os.walk(committed_chromium_src_dir):
    for file in files:
      file_path = os.path.join(root, file)
      path_relative_to_src = os.path.relpath(file_path,
                                             committed_chromium_src_dir)
      full_src_path = _SRC_PATH / path_relative_to_src
      if full_src_path.exists():
        full_src_path.unlink()
      shutil.move(file_path, full_src_path)
      changed_file_paths.append(path_relative_to_src)

  if not _HasChanges(_SRC_PATH):
    print("No changes found after extracting zip. Did you run this script "
          "before?")
    return

  if options.git_add:
    git_add_cmd = ['git', '-C', _SRC_PATH, 'add'] + changed_file_paths
    subprocess.check_call(git_add_cmd)
