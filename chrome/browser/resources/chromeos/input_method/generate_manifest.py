#!/usr/bin/env python

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Generate an extension manifest based on a template.'''
import argparse
import io
import json
import os
import sys

src_dir_path = os.path.normpath(
    os.path.join(os.path.abspath(__file__), *[os.path.pardir] * 6))

jinja2_path = os.path.normpath(
    os.path.join(src_dir_path, 'third_party'))
nom_path = os.path.normpath(
    os.path.join(src_dir_path, 'tools/json_comment_eater'))
version_py_path = os.path.normpath(
    os.path.join(src_dir_path, 'build/util'))
sys.path.insert(0, jinja2_path)
sys.path.insert(0, nom_path)
sys.path.insert(0, version_py_path)
import jinja2
import json_comment_eater
import version


def process_jinja_template(input_file, output_file, context):
    """ Processes a jinja2 template.

    Uses Jinja2 to convert a template into an output file.

    Args:
        input_file: path to template file as string.
        output_file: path to output file as string.
        context: variables available in the template as dict.
    """
    (template_path, template_name) = os.path.split(input_file)
    env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(template_path), trim_blocks=True)
    template = env.get_template(template_name)
    rendered = template.render(context)
    rendered_without_comments = json_comment_eater.Nom(rendered)
    # Simply for validation.
    json.loads(rendered_without_comments)
    with io.open(output_file, 'w', encoding='utf-8') as manifest_file:
        manifest_file.write(rendered_without_comments)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-o',
        '--output_manifest',
        help='File to place generated manifest')
    parser.add_argument(
        '--is_manifest_v3',
        default='0',
        help='Whether to generate a manifest using manifest version 3')
    parser.add_argument('input_file')
    args = parser.parse_args()

    if args.output_manifest is None:
        print('--output_manifest option must be specified', file=sys.stderr)
        sys.exit(1)
    context = {'is_manifest_v3': args.is_manifest_v3}
    process_jinja_template(args.input_file, args.output_manifest, context)


if __name__ == '__main__':
    main()