#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build and push the {version}/{platform}/vr-assets.zip file to GCS so
# that the component update system will pick them up and push them
# to users.
#
# Requires gsutil to be in the user's path.

import os
import shutil
import subprocess
import sys
import zipfile
import zlib
import json
import tempfile
import parse_version

DEST_BUCKET = 'gs://chrome-component-vr-assets'
PLATFORM = 'android'


class TempDir():

  def __enter__(self):
    self._dirpath = tempfile.mkdtemp()
    return self._dirpath

  def __exit__(self, type, value, traceback):
    shutil.rmtree(self._dirpath)


def PrintInfo(header, items):
  print('\n%s' % header)
  print '   ', '\n    '.join(items)


def main():
  assets_dir = os.path.dirname(os.path.abspath(__file__))

  files = []
  with open(os.path.join(assets_dir,
                         'vr_assets_component_files.json')) as json_file:
    files = json.load(json_file)

  version = None
  with open(os.path.join(assets_dir, 'VERSION')) as version_file:
    version = parse_version.ParseVersion(version_file.readlines())
  assert version

  PrintInfo('Version', ['%s.%s' % (version.major, version.minor)])
  PrintInfo('Platform', [PLATFORM])
  PrintInfo('Asset files', files)

  with TempDir() as temp_dir:
    zip_dir = os.path.join(temp_dir, '%s.%s' % (version.major, version.minor),
                           PLATFORM)
    zip_path = os.path.join(zip_dir, 'vr-assets.zip')

    os.makedirs(zip_dir)
    zip_files = []
    with zipfile.ZipFile(zip_path, 'w') as zip:
      for file in files:
        file_path = os.path.join(assets_dir, file)
        zip.write(file_path, os.path.basename(file_path), zipfile.ZIP_DEFLATED)
      for info in zip.infolist():
        zip_files.append(info.filename)

    # Upload component.
    command = ['gsutil', 'cp', '-nR', '.', DEST_BUCKET]
    PrintInfo('Going to run the following command', [' '.join(command)])
    PrintInfo('In directory', [temp_dir])
    PrintInfo('Which pushes the following file', [zip_path])
    PrintInfo('Which contains the files', zip_files)

    if raw_input('\nAre you sure (y/N) ').lower() != 'y':
      print 'aborting'
      return 1
    return subprocess.call(command, cwd=temp_dir)


if __name__ == '__main__':
  sys.exit(main())
