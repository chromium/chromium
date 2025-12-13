#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates .ipa file from an unpackaged .app bundle."""

import argparse
import pathlib
import shutil
import tempfile


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("app_path", type=str)
  parser.add_argument("ipa_path", type=str)
  args = parser.parse_args()
  app_path = pathlib.Path(args.app_path)
  ipa_path = pathlib.Path(args.ipa_path)
  with tempfile.TemporaryDirectory() as tmp_dir_path:
    tmp_dir = pathlib.Path(tmp_dir_path)
    payload_dir = tmp_dir / 'Payload'
    payload_dir.mkdir()
    shutil.copytree(app_path, str(payload_dir / app_path.name))
    shutil.make_archive(str(tmp_dir / app_path.with_suffix("").name),
                        'zip',
                        root_dir=tmp_dir_path,
                        base_dir='Payload')
    shutil.move(str(tmp_dir / app_path.with_suffix(".zip").name), ipa_path)


if __name__ == '__main__':
  main()
