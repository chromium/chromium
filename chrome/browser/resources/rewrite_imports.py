# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to rewrite scheme-relative imports to absolute chrome://
imports.

This is used when packaging WebUI code for use in component extensions, where
scheme-relative imports (e.g. '//resources/mojo/...') would incorrectly resolve
relative to the extension's origin (chrome-extension://...) instead of the WebUI
shared resources.
"""

import argparse
import os
import sys


def main():
  parser = argparse.ArgumentParser(
      description='Rewrites scheme-relative imports (e.g. "//resources/") to '
                  'absolute "chrome://resources/" imports. This is typically '
                  'needed for component extensions that use Mojo JS bindings, '
                  'as scheme-relative imports would otherwise resolve relative '
                  'to the extension\'s origin (chrome-extension://) instead of '
                  'the WebUI shared resources.')
  parser.add_argument(
      'files',
      nargs='+',
      help='Pairs of input and output files: '
           '<input1> <output1> <input2> <output2> ...'
  )
  args = parser.parse_args()

  file_pairs = args.files
  if len(file_pairs) % 2 != 0:
    parser.error("Must pass pairs of input and output files")

  for i in range(0, len(file_pairs), 2):
    in_file = file_pairs[i]
    out_file = file_pairs[i + 1]

    os.makedirs(os.path.dirname(out_file), exist_ok=True)

    with open(in_file, 'r', encoding='utf-8') as f:
      content = f.read()

    content = content.replace('//resources/', 'chrome://resources/')

    with open(out_file, 'w', encoding='utf-8') as f:
      f.write(content)


if __name__ == '__main__':
  main()
