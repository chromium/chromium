#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Compiles all tests in the tests/ directory from Switch Access Automated
Testing Language into a js2gtest.'''

import os
import sys

def main() -> None:
  SAATL_dir = os.path.dirname(__file__)
  compiler_dir = os.path.abspath(os.path.join(SAATL_dir, 'compiler/'))
  test_dir = os.path.abspath(os.path.join(SAATL_dir, 'tests/')) + '/'
  out_file = os.path.abspath(os.path.join(SAATL_dir, 'gen/saatlite_tests.js'))

  initial_dir = os.getcwd()
  os.chdir(compiler_dir)

  args = ['node',
          'compiler.js',
          test_dir,
          out_file]

  os.system(' '.join(args))
  os.chdir(initial_dir)

if __name__ == '__main__':
  main()
