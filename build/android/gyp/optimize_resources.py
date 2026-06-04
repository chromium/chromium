#!/usr/bin/env python3
#
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import sys

from util import build_utils
from util import protoresources

from proto import Resources_pb2  # prororesources adds proto to sys.path
import action_helpers  # build_utils adds //build to sys.path.


# Workaround for b/512191270. Returns true if something was edited, and we need
# to rewrite the resources file.
def _SanitizeProtoTable(msg):
  has_changes = False
  # Reference is a leaf node, so we don't need to care about recursing.
  if hasattr(msg, 'DESCRIPTOR') and msg.DESCRIPTOR.name == 'Reference':
    # aapt2 convert can't handle misnamed entries like
    # com.google.android.apps.chrome.tests:style/
    if msg.name and msg.name.endswith('/'):
      msg.name = ""
      return True
    return False
  for field_descriptor, field_value in msg.ListFields():
    if field_descriptor.type == field_descriptor.TYPE_MESSAGE:
      if field_descriptor.label == field_descriptor.LABEL_REPEATED:
        for item in field_value:
          if _SanitizeProtoTable(item):
            has_changes = True
      elif _SanitizeProtoTable(field_value):
        has_changes = True
  return has_changes


def _ParseArgs(args):
  """Parses command line options.

  Returns:
    An options object as from argparse.ArgumentParser.parse_args()
  """
  parser = argparse.ArgumentParser()
  parser.add_argument('--aapt2-path',
                      required=True,
                      help='Path to the Android aapt2 tool.')

  parser.add_argument('--input-path',
                      required=True,
                      help='Input resources APK in proto format.')

  parser.add_argument('--resources-config-paths',
                      default='[]',
                      help='GN list of paths to aapt2 resources config files.')
  parser.add_argument('--r-text-in',
                      required=True,
                      help='Path to R.txt. Used to exclude id/ resources.')
  parser.add_argument(
      '--resources-path-map-out-path',
      help='Path to file produced by aapt2 that maps original resource paths '
      'to shortened resource paths inside the apk or module.')
  parser.add_argument('--optimized-output-path',
                      required=True,
                      help='Output for `aapt2 optimize`.')
  options = parser.parse_args(args)

  options.resources_config_paths = action_helpers.parse_gn_list(
      options.resources_config_paths)

  return options


def _CombineResourceConfigs(resources_config_paths, out_config_path):
  with open(out_config_path, 'w', encoding='utf-8') as out_config:
    for config_path in resources_config_paths:
      with open(config_path, encoding='utf-8') as config:
        out_config.write(config.read())
        out_config.write('\n')


def _ExtractNonCollapsableResources(rtxt_path):
  """Extract resources that should not be collapsed from the R.txt file

  Resources of type ID are references to UI elements/views. They are used by
  UI automation testing frameworks. They are kept in so that they don't break
  tests, even though they may not actually be used during runtime. See
  https://crbug.com/900993
  App icons (aka mipmaps) are sometimes referenced by other apps by name so must
  be keps as well. See https://b/161564466

  Args:
    rtxt_path: Path to R.txt file with all the resources
  Returns:
    List of resources in the form of <resource_type>/<resource_name>
  """
  resources = []
  _NO_COLLAPSE_TYPES = ['id', 'mipmap']
  with open(rtxt_path, encoding='utf-8') as rtxt:
    for line in rtxt:
      for resource_type in _NO_COLLAPSE_TYPES:
        if ' {} '.format(resource_type) in line:
          resource_name = line.split()[2]
          resources.append('{}/{}'.format(resource_type, resource_name))
  return resources


def _OptimizeApk(output, options, temp_dir, unoptimized_path, r_txt_path):
  """Optimize intermediate .ap_ file with aapt2.

  Args:
    output: Path to write to.
    options: The command-line options.
    temp_dir: A temporary directory.
    unoptimized_path: path of the apk to optimize.
    r_txt_path: path to the R.txt file of the unoptimized apk.
  """
  # aapt2 optimize does not support an output format flag. If the output
  # format does not match what was requested, use aapt2 convert.
  output_format = 'proto' if '.proto' in output else 'binary'
  # We assume aapt2 optimize output matches its input format.
  current_format = 'proto' if '.proto' in unoptimized_path else 'binary'

  if output_format != current_format:
    # We will need to aapt convert to the final output location.
    optimize_output = os.path.join(temp_dir, f'optimized_res.{current_format}')
  else:
    optimize_output = output

  optimize_command = [
      options.aapt2_path,
      'optimize',
      unoptimized_path,
      '-o',
      optimize_output,
  ]

  # Optimize the resources.pb file by obfuscating resource names and only
  # allow usage via R.java constant.
  no_collapse_resources = _ExtractNonCollapsableResources(r_txt_path)
  gen_config_path = os.path.join(temp_dir, 'aapt2.config')
  if options.resources_config_paths:
    _CombineResourceConfigs(options.resources_config_paths, gen_config_path)
  with open(gen_config_path, 'a', encoding='utf-8') as config:
    for resource in no_collapse_resources:
      config.write('{}#no_collapse\n'.format(resource))

  optimize_command += [
      '--collapse-resource-names',
      '--resources-config-path',
      gen_config_path,
  ]

  optimize_command += ['--shorten-resource-paths']
  if options.resources_path_map_out_path:
    optimize_command += [
        '--resource-path-shortening-map', options.resources_path_map_out_path
    ]

  logging.info('Optimize command: %s', ' '.join(optimize_command))
  build_utils.CheckOutput(optimize_command,
                          print_stdout=False,
                          print_stderr=False)

  if output_format != current_format:
    if current_format == 'proto':

      def process_proto_resources(filename, data):
        if filename == 'resources.pb':
          table = Resources_pb2.ResourceTable()
          table.ParseFromString(data)
          if _SanitizeProtoTable(table):
            data = table.SerializeToString()
        return data

      protoresources.ProcessZip(optimize_output, process_proto_resources)

    logging.debug('Running aapt2 convert (%s -> %s)', current_format,
                  output_format)
    convert_command = [
        options.aapt2_path,
        'convert',
        '--output-format',
        output_format,
        '-o',
        output,
        optimize_output,
    ]
    build_utils.CheckOutput(convert_command,
                            print_stdout=False,
                            print_stderr=False)


def main(args):
  options = _ParseArgs(args)

  with build_utils.TempDir() as temp_dir:
    _OptimizeApk(options.optimized_output_path, options, temp_dir,
                 options.input_path, options.r_text_in)


if __name__ == '__main__':
  main(sys.argv[1:])
