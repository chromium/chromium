#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reads all SVGs and transform to lit svg templates in ES6 module."""

import argparse
import json
import pathlib
import shlex
import sys
from typing import List


def gen_images_js(images: List[pathlib.Path], root_dir: pathlib.Path) -> str:
    image_dict = {}
    for image in images:
        with open(image, 'r', encoding='utf-8') as f:
            # Removes the extension.
            relative_path = image.relative_to(root_dir)
            id = str(relative_path.parent / relative_path.stem)
            image_dict[id] = f.read().strip()

    formatted_images = '[' + ','.join(
        f'[{json.dumps(name)}, svg`{svg}`]'
        for name, svg in image_dict.items()) + ']'

    return ('import {svg} from "chrome://resources/mwc/lit/index.js";\n'
            f'export const images = new Map({formatted_images});')


def main():
    argument_parser = argparse.ArgumentParser()
    argument_parser.add_argument(
        '--images_file', help='File contains a list of images to be appended')
    argument_parser.add_argument(
        '--root_dir',
        help='Root directory of the images.'
        ' The image id will be generated based on this.')
    argument_parser.add_argument(
        '--output_file', help='The output js file containing all images')
    args = argument_parser.parse_args()

    with open(args.images_file) as f:
        images = [pathlib.Path(s) for s in shlex.split(f.read())]

    with open(args.output_file, 'w', encoding='utf-8') as f:
        f.write(gen_images_js(images, pathlib.Path(args.root_dir)))

    return 0


if __name__ == '__main__':
    sys.exit(main())
