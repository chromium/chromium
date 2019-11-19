#!/usr/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates an .apks from an .aab."""

import argparse
import os
import sys

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir)))
from pylib.utils import app_bundle_utils


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--bundle', required=True, help='Path to input .aab file.')
  parser.add_argument(
      '--output', required=True, help='Path to output .apks file.')
  parser.add_argument('--aapt2-path', required=True, help='Path to aapt2.')
  parser.add_argument(
      '--keystore-path', required=True, help='Path to keystore.')
  parser.add_argument(
      '--keystore-password', required=True, help='Keystore password.')
  parser.add_argument(
      '--keystore-name', required=True, help='Key name within keystore')
  parser.add_argument(
      '--minimal',
      action='store_true',
      help='Create APKs archive with minimal language support.')

  args = parser.parse_args()

  app_bundle_utils.GenerateBundleApks(
      args.bundle,
      args.output,
      args.aapt2_path,
      args.keystore_path,
      args.keystore_password,
      args.keystore_name,
      minimal=args.minimal,
      check_for_noop=False)


if __name__ == '__main__':
  main()
