#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates an array of images to be preloaded as a ES6 Module."""

import argparse
import json
import os
import shlex
import sys
from typing import List


def gen_preload_images_js(in_app_images: List[str],
                          standalone_images: List[str]) -> str:
    images = {}
    for image in in_app_images:
        with open(image, 'r', encoding='utf-8') as f:
            images[os.path.basename(image)] = f.read()

    filenames = [os.path.basename(f) for f in standalone_images]
    formatted_images = '[' + ','.join(f'[{json.dumps(name)}, svg`{image}`]'
                                      for name, image in images.items()) + ']'
    return (
        'import {svg} from "chrome://resources/mwc/lit/index.js";'
        f'export const preloadImagesList = {json.dumps(filenames, indent=2)};'
        f'export const preloadedImages = new Map({formatted_images});')


def main():
    argument_parser = argparse.ArgumentParser()
    argument_parser.add_argument(
        '--output_file', help='The output js file exporting preload images')
    argument_parser.add_argument(
        '--in_app_images_file',
        help='File contains a list of images to be appended')
    argument_parser.add_argument(
        '--standalone_images',
        help='File contains a list of standalone images to be appended',
        nargs='*')
    args = argument_parser.parse_args()
    with open(args.in_app_images_file) as f:
        in_app_images = shlex.split(f.read())

    with open(args.output_file, 'w', encoding='utf-8') as f:
        f.write(gen_preload_images_js(in_app_images, args.standalone_images))

    return 0


if __name__ == '__main__':
    sys.exit(main())
