# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import sys


def ParseArgs():
  parser = argparse.ArgumentParser(
      description='Host file generator for CELab E2E tests')

  all_tokens = ['project_id', 'storage_bucket', 'storage_prefix']
  template_help = 'The full path to the *.host.textpb template file to use. '
  template_help += 'Must contain the following tokens: %s' % all_tokens
  parser.add_argument(
      '--template', metavar='<host_file>', required=True, help=template_help)
  parser.add_argument(
      '--projects',
      metavar='<projectA;projectB;...>',
      dest="projects",
      required=True,
      help='The values to replace "<project_id>" with.')
  parser.add_argument(
      '--storage_bucket',
      metavar='<token>',
      dest="storage_bucket",
      required=True,
      help='The value to replace "<storage_bucket>" with.')
  parser.add_argument(
      '--storage_prefix',
      metavar='<token>',
      dest="storage_prefix",
      required=True,
      help='The value to replace "<storage_prefix>" with.')
  parser.add_argument(
      '--destination_dir',
      metavar='<path>',
      dest='destination',
      required=True,
      action='store',
      help='Where to collect extra logs on test failures')

  return parser.parse_args()


def ConfigureLogging(args):
  logfmt = '%(asctime)s %(filename)s:%(lineno)s: [%(levelname)s] %(message)s'
  datefmt = '%Y/%m/%d %H:%M:%S'

  logging.basicConfig(level=logging.INFO, format=logfmt, datefmt=datefmt)


if __name__ == '__main__':
  args = ParseArgs()

  ConfigureLogging(args)

  logging.info("Arguments: %s" % args)

  if not os.path.exists(args.template):
    raise ValueError('Template host file not found: %s' % args.template)

  if not os.path.exists(args.destination):
    raise ValueError('Destination directory not found: %s' % args.destination)

  # Generate all the host files based off the arguments passed.
  with open(args.template, 'r') as f:
    template = f.read()

  for project_id in args.projects.split(';'):
    filename = "%s.host.textpb" % project_id
    destination = os.path.join(args.destination, filename)
    with open(destination, 'w') as f:
      logging.info("Generating %s" % destination)
      content = template.replace("<project_id>", project_id)
      content = content.replace("<storage_bucket>", args.storage_bucket)
      content = content.replace("<storage_prefix>", args.storage_prefix)
      f.write(content)

  sys.exit(0)
