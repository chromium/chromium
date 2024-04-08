#!/usr/bin/env python3

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate the ABOUT file to be shipped alongside CfT assets.
"""

import argparse
import datetime
import sys

year = datetime.datetime.now().year
contents = ("""
Google Chrome

Copyright %s Google LLC. All rights reserved.

Chrome is made possible by the Chromium open source project
(https://www.chromium.org/) and other open source software
(chrome://credits).

See the Terms of Service at chrome://terms.
""" % year).lstrip()

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output-file',
                      help='Which file to write to.')
  args = parser.parse_args()

  with open(args.output_file, 'w') as fp:
    fp.write(contents)

if __name__ == '__main__':
  sys.exit(main())
