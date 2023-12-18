#!/usr/bin/env python

# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import io
import optparse
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
from json_comment_eater import Nom
import version
'''Generate an extension manifest based on a template.'''


def getChromeVersion(version_file):
  values = version.FetchValues([version_file])
  return version.SubstTemplate('@MAJOR@.@MINOR@.@BUILD@.@PATCH@', values)


def processJinjaTemplate(input_file, output_file, context):
  (template_path, template_name) = os.path.split(input_file)
  env = jinja2.Environment(
      loader=jinja2.FileSystemLoader(template_path), trim_blocks=True)
  template = env.get_template(template_name)
  rendered = template.render(context)
  rendered_without_comments = Nom(rendered)
  # Simply for validation.
  json.loads(rendered_without_comments)
  with io.open(output_file, 'w', encoding='utf-8') as manifest_file:
    manifest_file.write(rendered_without_comments)


def main():
  parser = optparse.OptionParser(description=__doc__)
  parser.usage = '%prog [options] <template_manifest_path>'
  parser.add_option(
      '-o',
      '--output_manifest',
      action='store',
      metavar='OUTPUT_MANIFEST',
      help='File to place generated manifest')
  parser.add_option(
      '--is_guest_manifest',
      default='0',
      action='store',
      metavar='NUM',
      help='Whether to generate a guest mode capable manifest')
  parser.add_option(
      '--is_manifest_v3',
      default='0',
      action='store',
      metavar='NUM',
      help='Whether to generate a manifest using manifest version 3')
  parser.add_option(
      '--is_js_compressed',
      default='1',
      action='store',
      metavar='NUM',
      help='Whether compressed JavaScript files are used')
  parser.add_option(
      '--set_version',
      action='store',
      metavar='SET_VERSION',
      help='Set the extension version')
  parser.add_option(
      '--key', action='store', metavar='KEY', help='Set the extension key')
  parser.add_option(
      '--version_file',
      action='store',
      metavar='NAME',
      help='File with version information')

  options, args = parser.parse_args()
  if len(args) != 1:
    print >> sys.stderr, 'Expected exactly one argument'
    sys.exit(1)
  if options.output_manifest is None:
    print >> sys.stderr, '--output_manifest option must be specified'
    sys.exit(1)
  if options.set_version is not None and options.version_file is not None:
    print >> sys.stderr, (
        'only one of --set_version and --version_file may ' + 'be specified')
  if options.set_version is None and options.version_file is None:
    print >> sys.stderr, (
        'one of --set_version or --version_file option ' + 'must be specified')
    sys.exit(1)
  context = {k: v for k, v in parser.values.__dict__.items() if v is not None}
  if options.version_file is not None:
    context['set_version'] = getChromeVersion(options.version_file)
  processJinjaTemplate(args[0], options.output_manifest, context)


if __name__ == '__main__':
  main()
