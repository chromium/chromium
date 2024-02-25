#!/usr/bin/env python

# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
''' Generates a deps.js file based on an input list of javascript files using
Closure style provide/require calls.
'''

import optparse
import os
import sys

from jsbundler import PathRewriter

_SCRIPT_DIR = os.path.realpath(os.path.dirname(__file__))
_CHROME_SOURCE = os.path.realpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 6))
sys.path.insert(
    0,
    os.path.join(_CHROME_SOURCE, ('third_party/chromevox/third_party/' +
                                  'closure-library/closure/bin/build')))
import source


def _HasSameContent(filename, content):
  '''Returns true if the given file is readable and has the given content.'''
  try:
    with open(filename) as file:
      return file.read() == content
  except:
    # Ignore all errors and fall back on a safe bet.
    return False


def main():
  parser = optparse.OptionParser(description=__doc__)
  parser.add_option(
      '-w',
      '--rewrite_prefix',
      action='append',
      default=[],
      dest='prefix_map',
      metavar='SPEC',
      help=('Two path prefixes, separated by colons ' +
            'specifying that a file whose (relative) path ' +
            'name starts with the first prefix should have ' +
            'that prefix replaced by the second prefix to ' +
            'form a path relative to the output directory. ' +
            'The resulting path is used in the deps mapping ' +
            'file path to a list of provided and required ' + 'namespaces.'))
  parser.add_option(
      '-o',
      '--output_file',
      action='store',
      default=[],
      metavar='SPEC',
      help=('Where to output the generated deps file.'))
  options, args = parser.parse_args()

  path_rewriter = PathRewriter(options.prefix_map)

  content = ''
  for path in args:
    js_deps = source.Source(source.GetFileContents(path))
    path = path_rewriter.RewritePath(path)
    content += 'goog.addDependency(\'%s\', %s, %s);\n' % (
        path, sorted(js_deps.provides), sorted(js_deps.requires))
  if _HasSameContent(options.output_file, content):
    return
  # Write the generated deps file.
  with open(options.output_file, 'w') as output:
    output.write(content)


if __name__ == '__main__':
  main()
