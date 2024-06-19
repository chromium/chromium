#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reads all icon SVGs and transform to lit svg templates in ES6 module."""

import argparse
import json
import pathlib
import shlex
import sys
from typing import List


def gen_icons_js(icons: List[pathlib.Path]) -> str:
    icon_dict = {}
    for icon in icons:
        with open(icon, 'r', encoding='utf-8') as f:
            icon_dict[icon.stem] = f.read().strip()

    formatted_icons = '[' + ','.join(f'[{json.dumps(name)}, svg`{svg}`]'
                                     for name, svg in icon_dict.items()) + ']'

    return ('import {svg} from "chrome://resources/mwc/lit/index.js";\n'
            f'export const icons = new Map({formatted_icons});')


def main():
    argument_parser = argparse.ArgumentParser()
    argument_parser.add_argument(
        '--icons_file', help='File contains a list of icons to be appended')
    argument_parser.add_argument(
        '--output_file', help='The output js file containing all icons')
    args = argument_parser.parse_args()

    with open(args.icons_file) as f:
        icons = [pathlib.Path(s) for s in shlex.split(f.read())]

    with open(args.output_file, 'w', encoding='utf-8') as f:
        f.write(gen_icons_js(icons))

    return 0


if __name__ == '__main__':
    sys.exit(main())
