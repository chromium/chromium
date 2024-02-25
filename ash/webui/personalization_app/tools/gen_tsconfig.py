#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Helper tool to generate a local tsconfig.json file.

This tool can be used to generate a local tsconfig.json file that helps
typescript language servers to parse code files. The tsconfig.json file will be
generated in the gn target's dir.

The script need to run in the Chromium src root dir to properly build the ts gn
targets.

e.g.
gen_tsconfig.py --root_out_dir out_local/Release \
        --gn_target ash/webui/personalization_app/resources:build_ts

gen_tsconfig.py --root_out_dir out_local/Release \
        --gn_target chrome/test/data/webui/chromeos/personalization_app:build_ts

gen_tsconfig.py --root_out_dir out_local/Release \
        --gn_target chrome/browser/resources/settings:build_ts

"""

import argparse
import json
import os
import subprocess
import sys


def parse_arguments(arguments):
    parser = argparse.ArgumentParser()
    parser.add_argument('--root_out_dir',
                        help='The root dir of gn output directory.',
                        required=True)
    parser.add_argument(
        '--gn_target',
        help='The typescript gn build target which builds the source code '
             'files',
        required=True)
    parser.add_argument(
        '--custom_def_files',
        help='Comma separate file list which will be added to "files" section '
             'in tsconfig as additional type definitions',
        required=False)
    return parser.parse_args(arguments)


def normalize_path(root_dir, relative_dir):
    """ normalize the combined path, also make it absolute. """
    abs_root_dir = os.path.abspath(root_dir)
    return os.path.normpath(os.path.join(abs_root_dir, relative_dir))


def main(args):
    arguments = parse_arguments(args)
    # execute the ts build target to generate the tsconfig.json file.
    subprocess.check_call(
        ['autoninja', '-C', arguments.root_out_dir, arguments.gn_target],
        stdout=subprocess.DEVNULL)

    gn_target_src_dir, gn_target_suffix = arguments.gn_target.split(':')

    # find the auto generated tsconfig.json used by the build rule itself.
    out_json_path = os.path.join(
        arguments.root_out_dir,
        'gen/',
        # build target's location in gen/ folder
        gn_target_src_dir,
        f'tsconfig_{gn_target_suffix}.json')
    if not os.path.exists(out_json_path):
        print('Can not find the auto generated tsconfig.json file:',
              out_json_path)
        return

    with open(out_json_path, 'r') as f:
        out_json = json.load(f)

    out_json_dir = os.path.dirname(out_json_path)
    definitions = ['.d.ts']
    if arguments.custom_def_files:
        definitions.extend(arguments.custom_def_files.split(','))
    local_json = {
        'extends': normalize_path(out_json_dir, out_json['extends']),
        'compilerOptions': {
            'baseUrl': '.',
            'allowJs': out_json['compilerOptions'].get('allowJs', False),
            'rootDirs': [
                '.',
                normalize_path(out_json_dir,
                               out_json['compilerOptions']['rootDir']),
            ],
            'noEmit': True,
            'paths': {
                key: [normalize_path(out_json_dir, path) for path in value]
                for key, value in out_json['compilerOptions']['paths'].items()
            },
        },
        'files': [
            # Add the .d.ts files.
            normalize_path(out_json_dir, path)
            for path in out_json['files'] if path.endswith(tuple(definitions))
        ],
        'include': [
            # Include every source file underneath the generated tsconfig.json.
            '**/*'
        ],
        'references': [{
            'path': normalize_path(out_json_dir, path['path'])
        } for path in out_json['references']],
    }

    output_path = os.path.join(gn_target_src_dir, 'tsconfig.json')
    with open(output_path, 'w') as f:
        json.dump(local_json, f, indent=2)

    print(os.path.basename(__file__), 'wrote file', output_path)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
