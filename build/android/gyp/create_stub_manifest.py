#!/usr/bin/env python3

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates AndroidManifest.xml for a -Stub.apk."""

import argparse
import pathlib

_MAIN_TEMPLATE = """\
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="will.be.replaced">
  <application android:label="APK Stub">{}</application>
</manifest>
"""

_STATIC_LIBRARY_TEMPLATE = """
    <static-library android:name="{}" android:version="{}" />
"""


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--static-library-name')
  parser.add_argument('--static-library-version')
  parser.add_argument('--output', required=True)
  args = parser.parse_args()

  static_library_part = ''
  if args.static_library_name:
    static_library_part = _STATIC_LIBRARY_TEMPLATE.format(
        args.static_library_name, args.static_library_version)

  data = _MAIN_TEMPLATE.format(static_library_part)
  pathlib.Path(args.output).write_text(data, encoding='utf8')


if __name__ == '__main__':
  main()
