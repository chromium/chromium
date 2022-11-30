#!/usr/bin/env python

# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Print the dependency tree for a JavaScript module.

Given one or more root directories, specified by -r options and one top-level
file, walk the dependency tree and print all modules encountered.
A module is only expanded once; on a second encounter, its dependencies
are represented by a line containing the characters '[...]' as a short-hand.
'''

import optparse
import os
import sys

from jsbundler import ReadSources


def Die(message):
  '''Prints an error message and exit the program.'''
  print >> sys.stderr, message
  sys.exit(1)


def CreateOptionParser():
  parser = optparse.OptionParser(description=__doc__)
  parser.usage = '%prog [options] <top_level_file>'
  parser.add_option(
      '-r',
      '--root',
      dest='roots',
      action='append',
      default=[],
      metavar='ROOT',
      help='Roots of directory trees to scan for sources. '
      'If none specified, all of ChromeVox and closure sources '
      'are scanned.')
  return parser


def DefaultRoots():
  script_dir = os.path.dirname(os.path.abspath(__file__))
  source_root_dir = os.path.join(script_dir, *[os.path.pardir] * 7)
  return [
      os.path.relpath(os.path.join(script_dir, os.path.pardir)),
      os.path.relpath(
          os.path.join(source_root_dir, 'chrome', 'third_party', 'chromevox',
                       'third_party', 'closure-library', 'closure'))
  ]


def WalkDeps(sources, start_source):

  def Walk(source, depth):
    indent = '  ' * depth
    if source.GetInPath() in expanded and len(source.requires) > 0:
      print '%s[...]' % indent
      return
    expanded.add(source.GetInPath())
    for require in source.requires:
      if not require in providers:
        Die('%s not provided, required by %s' % (require, source.GetInPath()))
      require_source = providers[require]
      print '%s%s (%s)' % (indent, require, require_source.GetInPath())
      Walk(require_source, depth + 1)

  # Create a map from provided module names to source objects.
  providers = {}
  expanded = set()
  for source in sources.values():
    for provide in source.provides:
      if provide in providers:
        Die('%s provided multiple times' % provide)
      providers[provide] = source

  print '(%s)' % start_source.GetInPath()
  Walk(start_source, 1)


def main():
  parser = CreateOptionParser()
  options, args = parser.parse_args()
  if len(args) != 1:
    Die('Exactly one top-level source file must be specified.')
  start_path = args[0]
  roots = options.roots or DefaultRoots()
  sources = ReadSources(roots=roots, source_files=[start_path])
  start_source = sources[start_path]
  WalkDeps(sources, start_source)


if __name__ == '__main__':
  main()
