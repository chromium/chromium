#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Expands template using Mustache template engine."""

import argparse
import codecs
import json
import os
import sys

#Import pystache from //third_party/pystache
src_dir = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                       os.pardir, os.pardir)
sys.path.insert(0, os.path.join(src_dir, 'third_party'))
import pystache
sys.path.insert(0, os.path.join(src_dir, 'build/android/gyp'))
from util import build_utils  # pylint: disable=import-error


def _AppendParsedVariables(initial_variable_list, variables_arg, error_func):
  variables = initial_variable_list
  for v in build_utils.ParseGnList(variables_arg):
    if '=' not in v:
      error_func('--variables argument must contain "=": ' + v)
    name, _, value = v.partition('=')
    if value == "false":
      value = False
    variables[name] = value
  return variables


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--template', required=True,
                      help='The template file to process.')
  parser.add_argument('--output', required=True,
                      help='The output file to generate.')
  parser.add_argument('--config_file',
                      help='JSON file with values to put into template.')
  parser.add_argument('--delta_config_file', help='JSON file with '
                      'substitutions to |config_file|.')
  parser.add_argument('--extra_variables', help='Variables to be made '
                      'available in the template processing environment (in '
                      'addition to those specified in config file), as a GN '
                      'list (e.g. --extra_variables "channel=beta mstone=39")',
                      default='')
  options = parser.parse_args()

  config = {}
  if options.config_file:
    with open(options.config_file, 'r') as f:
      config = json.loads(f.read())
  if options.delta_config_file:
    with open(options.delta_config_file, 'r') as f:
      config.update(json.loads(f.read()))

  variables = config
  variables = _AppendParsedVariables(variables, options.extra_variables,
                                     parser.error)

  with open(options.template, 'r') as f:
    render = pystache.render(f.read(), variables)
    with codecs.open(options.output, 'w', 'utf-8') as output_file:
      output_file.write(render)


if __name__ == '__main__':
  main()
