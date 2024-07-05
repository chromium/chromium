#!/usr/bin/env python3
# Copyright 2021 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import os
from pathlib import Path
from typing import List

_BUILD_TYPES = ['debug', 'devel', 'qa']

def main(out_directory: str, platform: str, build_type: str,
         overwrite_args: bool, gn_gen_args: List[str]):
    platform_path = 'chrobalt/linux'
    dst_args_gn_file = os.path.join(out_directory, 'args.gn')
    src_args_gn_file = os.path.join(platform_path, 'args.gn')
    Path(out_directory).mkdir(parents=True, exist_ok=True)

if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  builds_directory_group = parser.add_mutually_exclusive_group()
  builds_directory_group.add_argument(
      'out_directory',
      type=str,
      nargs='?',
      help='Path to the directory to build in.')

  parser.add_argument(
      '-p',
      '--platform',
      default='linux-x64x11',
      choices=['linux-x64x11'],
      help='The platform to build.')
  parser.add_argument(
      '-c',
      '-C',
      '--build_type',
      default='devel',
      choices=_BUILD_TYPES,
      help='The build_type (configuration) to build with.')
  parser.add_argument(
      '--overwrite_args',
      default=False,
      action='store_true',
      help='Whether or not to overwrite an existing args.gn file if one exists '
      'in the out directory. In general, if the file exists, you should run '
      '`gn args <out_directory>` to edit it instead.')
  parser.add_argument(
      '--no-check',
      default=False,
      action='store_true',
      help='Pass this flag to disable the header dependency gn check.')
  script_args, gen_args = parser.parse_known_args()

  if not script_args.no_check:
    gen_args.append('--check')

  if script_args.out_directory:
    builds_out_directory = script_args.out_directory
  else:
    builds_directory = 'out'
    builds_out_directory = os.path.join(
        builds_directory, f'{script_args.platform}_{script_args.build_type}')
  main(builds_out_directory, script_args.platform, script_args.build_type,
       script_args.overwrite_args, gen_args)
