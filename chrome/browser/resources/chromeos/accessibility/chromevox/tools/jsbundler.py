#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Produces various output formats from a set of JavaScript files with
closure style require/provide calls.

Scans one or more directory trees for JavaScript files.  Then, from a
given list of top-level files, sorts all required input files topologically.
The top-level files are appended to the sorted list in the order specified
on the command line.  If no root directories are specified, the source
files are assumed to be ordered already and no dependency analysis is
performed.  The resulting file list can then be used in one of the following
ways:

- list: a plain list of files, one per line is output.

- html: a series of html <script> tags with src attributes containing paths
  is output.

- bundle: a concatenation of all the files, separated by newlines is output.

- compressed_bundle: A bundle where non-significant whitespace, including
  comments, has been stripped is output.

- copy: the files are copied, or hard linked if possible, to the destination
  directory.  In this case, no output is generated.
'''

import errno
import optparse
import os
import re
import shutil
import sys

_SCRIPT_DIR = os.path.realpath(os.path.dirname(__file__))
_CHROME_SOURCE = os.path.realpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 7))
sys.path.insert(
    0,
    os.path.join(_CHROME_SOURCE,
                 'third_party/devtools-frontend/src/scripts/build'))
sys.path.insert(
    0,
    os.path.join(_CHROME_SOURCE, ('third_party/chromevox/third_party/' +
                                  'closure-library/closure/bin/build')))
import depstree
import rjsmin
import source
import treescan


def Die(message):
  '''Prints an error message and exit the program.'''
  print >> sys.stderr, message
  sys.exit(1)


class SourceWithPaths(source.Source):
  '''A source.Source object with its relative input and output paths'''

  def __init__(self, content, in_path, out_path):
    super(SourceWithPaths, self).__init__(content)
    self._in_path = in_path
    self._out_path = out_path

  def GetInPath(self):
    return self._in_path

  def GetOutPath(self):
    return self._out_path

  def __str__(self):
    return self.GetOutPath()

class Bundle():
  '''An ordered list of sources without duplicates.'''

  def __init__(self):
    self._added_paths = set()
    self._added_sources = []

  def Add(self, sources):
    '''Appends one or more source objects the list if it doesn't already
    exist.

    Args:
      sources: A SourceWithPath or an iterable of such objects.
    '''
    if isinstance(sources, SourceWithPaths):
      sources = [sources]
    for source in sources:
      path = source.GetInPath()
      if path not in self._added_paths:
        self._added_paths.add(path)
        self._added_sources.append(source)

  def GetInPaths(self):
    return (source.GetInPath() for source in self._added_sources)

  def GetOutPaths(self):
    return (source.GetOutPath() for source in self._added_sources)

  def GetSources(self):
    return self._added_sources

  def GetUncompressedSource(self):
    return '\n'.join((s.GetSource() for s in self._added_sources))

  def GetCompressedSource(self):
    return rjsmin.jsmin(self.GetUncompressedSource())


class PathRewriter():
  '''A list of simple path rewrite rules to map relative input paths to
  relative output paths.
  '''

  def __init__(self, specs=[]):
    '''Args:
      specs: A list of mappings, each consisting of the input prefix and
        the corresponding output prefix separated by colons.
    '''
    self._prefix_map = []
    for spec in specs:
      parts = spec.split(':')
      if len(parts) != 2:
        Die('Invalid prefix rewrite spec %s' % spec)
      if not parts[0].endswith('/') and parts[0] != '':
        parts[0] += '/'
      self._prefix_map.append(parts)
    self._prefix_map.sort(reverse=True)

  def RewritePath(self, in_path):
    '''Rewrites an input path according to the list of rules.

    Args:
      in_path, str: The input path to rewrite.
    Returns:
      str: The corresponding output path.
    '''
    for in_prefix, out_prefix in self._prefix_map:
      if in_path.startswith(in_prefix):
        return os.path.join(out_prefix, in_path[len(in_prefix):])
    return in_path


def ReadSources(roots=[],
                source_files=[],
                need_source_text=False,
                path_rewriter=PathRewriter(),
                exclude=[]):
  '''Reads all source specified on the command line, including sources
  included by --root options.
  '''

  def EnsureSourceLoaded(in_path, sources):
    if in_path not in sources:
      out_path = path_rewriter.RewritePath(in_path)
      sources[in_path] = SourceWithPaths(
          source.GetFileContents(in_path), in_path, out_path)

  # Only read the actual source file if we will do a dependency analysis or
  # the caller asks for it.
  need_source_text = need_source_text or len(roots) > 0
  sources = {}
  for root in roots:
    for name in treescan.ScanTreeForJsFiles(root):
      if any((r.search(name) for r in exclude)):
        continue
      EnsureSourceLoaded(name, sources)
  for path in source_files:
    if need_source_text:
      EnsureSourceLoaded(path, sources)
    else:
      # Just add an empty representation of the source.
      sources[path] = SourceWithPaths('', path, path_rewriter.RewritePath(path))
  return sources


def _GetBase(sources):
  '''Gets the closure base.js file if present among the sources.

  Args:
    sources: Dictionary with input path names as keys and SourceWithPaths
      as values.
  Returns:
    SourceWithPath: The source file providing the goog namespace.
  '''
  for source in sources.itervalues():
    if (os.path.basename(source.GetInPath()) == 'base.js' and
        'goog' in source.provides):
      return source
  Die('goog.base not provided by any file.')


def CalcDeps(bundle, sources, top_level):
  '''Calculates dependencies for a set of top-level files.

  Args:
    bundle: Bundle to add the sources to.
    sources, dict: Mapping from input path to SourceWithPaths objects.
    top_level, list: List of top-level input paths to calculate dependencies
      for.
  '''
  providers = [s for s in sources.itervalues() if len(s.provides) > 0]
  deps = depstree.DepsTree(providers)
  namespaces = []
  for path in top_level:
    namespaces.extend(sources[path].requires)
  # base.js is an implicit dependency that always goes first.
  bundle.Add(_GetBase(sources))
  bundle.Add(deps.GetDependencies(namespaces))


def _MarkAsCompiled(sources):
  '''Sets COMPILED to true in the Closure base.js source.

  Args:
    sources: Dictionary with input paths names as keys and SourcWithPaths
      objects as values.
  '''
  base = _GetBase(sources)
  new_content, count = re.subn(
      '^var COMPILED = false;$',
      'var COMPILED = true;',
      base.GetSource(),
      count=1,
      flags=re.MULTILINE)
  if count != 1:
    Die('COMPILED var assignment not found in %s' % base.GetInPath())
  sources[base.GetInPath()] = SourceWithPaths(new_content, base.GetInPath(),
                                              base.GetOutPath())


def LinkOrCopyFiles(sources, dest_dir):
  '''Copies a list of sources to a destination directory.'''

  def LinkOrCopyOneFile(src, dst):
    try:
      os.makedirs(os.path.dirname(dst))
    except OSError as err:
      if err.errno != errno.EEXIST:
        raise
    if os.path.exists(dst):
      os.unlink(dst)
    try:
      os.link(src, dst)
    except:
      shutil.copy(src, dst)

  for source in sources:
    LinkOrCopyOneFile(source.GetInPath(),
                      os.path.join(dest_dir, source.GetOutPath()))


def WriteOutput(bundle, format, out_file, dest_dir):
  '''Writes output in the specified format.

  Args:
    bundle: The ordered bundle iwth all sources already added.
    format: Output format, one of list, html, bundle, compressed_bundle.
    out_file: File object to receive the output.
    dest_dir: Prepended to each path mentioned in the output, if applicable.
  '''
  if format == 'list':
    paths = bundle.GetOutPaths()
    if dest_dir:
      paths = (os.path.join(dest_dir, p) for p in paths)
    paths = (os.path.normpath(p) for p in paths)
    out_file.write('\n'.join(paths))
  elif format == 'html':
    HTML_TEMPLATE = '<script src=\'%s\'>'
    script_lines = (HTML_TEMPLATE % p for p in bundle.GetOutPaths())
    out_file.write('\n'.join(script_lines))
  elif format == 'bundle':
    out_file.write(bundle.GetUncompressedSource())
  elif format == 'compressed_bundle':
    out_file.write(bundle.GetCompressedSource())
  out_file.write('\n')


def WriteStampfile(stampfile):
  '''Writes a stamp file.

  Args:
    stampfile, string: name of stamp file to touch
  '''
  with open(stampfile, 'w') as file:
    os.utime(stampfile, None)


def WriteDepfile(depfile, outfile, infiles):
  '''Writes a depfile.

  Args:
    depfile, string: name of dep file to write
    outfile, string: Name of output file to use as the target in the generated
      .d file.
    infiles, list: File names to list as dependencies in the .d file.
  '''
  content = '%s: %s' % (outfile, ' '.join(infiles))
  dirname = os.path.dirname(depfile)
  if not os.path.exists(dirname):
    os.makedirs(dirname)
  open(depfile, 'w').write(content)


def CreateOptionParser():
  parser = optparse.OptionParser(description=__doc__)
  parser.usage = '%prog [options] <top_level_file>...'
  parser.add_option(
      '-d',
      '--dest_dir',
      action='store',
      metavar='DIR',
      help=('Destination directory.  Used when translating ' +
            'input paths to output paths and when copying '
            'files.'))
  parser.add_option(
      '-o',
      '--output_file',
      action='store',
      metavar='FILE',
      help=('File to output result to for modes that output '
            'a single file.'))
  parser.add_option(
      '-r',
      '--root',
      dest='roots',
      action='append',
      default=[],
      metavar='ROOT',
      help='Roots of directory trees to scan for sources.')
  parser.add_option(
      '-M',
      '--module',
      dest='modules',
      action='append',
      default=[],
      metavar='FILENAME',
      help='Source modules to load')
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
            'form a path relative to the output directory.'))
  parser.add_option(
      '-m',
      '--mode',
      type='choice',
      action='store',
      choices=['list', 'html', 'bundle', 'compressed_bundle', 'copy'],
      default='list',
      metavar='MODE',
      help=("Otput mode. One of 'list', 'html', 'bundle', " +
            "'compressed_bundle' or 'copy'."))
  parser.add_option(
      '-x',
      '--exclude',
      action='append',
      default=[],
      help=('Exclude files whose full path contains a match for '
            'the given regular expression.  Does not apply to '
            'filenames given as arguments or with the '
            '-m option.'))
  parser.add_option(
      '--depfile',
      metavar='FILENAME',
      help='Store .d style dependencies in FILENAME')
  parser.add_option(
      '--stampfile', metavar='FILENAME', help='Write empty stamp file')
  return parser


def main():
  options, args = CreateOptionParser().parse_args()
  if len(args) < 1:
    Die('At least one top-level source file must be specified.')
  if options.depfile and not options.output_file:
    Die('--depfile requires an output file')
  will_output_source_text = options.mode in ('bundle', 'compressed_bundle')
  path_rewriter = PathRewriter(options.prefix_map)
  exclude = [re.compile(r) for r in options.exclude]
  sources = ReadSources(options.roots, options.modules + args,
                        will_output_source_text or len(options.modules) > 0,
                        path_rewriter, exclude)
  if will_output_source_text:
    _MarkAsCompiled(sources)
  bundle = Bundle()
  if len(options.roots) > 0 or len(options.modules) > 0:
    CalcDeps(bundle, sources, args)
  bundle.Add((sources[name] for name in args))
  if options.mode == 'copy':
    if options.dest_dir is None:
      Die('Must specify --dest_dir when copying.')
    LinkOrCopyFiles(bundle.GetSources(), options.dest_dir)
  else:
    if options.output_file:
      out_file = open(options.output_file, 'w')
    else:
      out_file = sys.stdout
    try:
      WriteOutput(bundle, options.mode, out_file, options.dest_dir)
    finally:
      if options.output_file:
        out_file.close()
  if options.stampfile:
    WriteStampfile(options.stampfile)
  if options.depfile:
    WriteDepfile(options.depfile, options.output_file, bundle.GetInPaths())


if __name__ == '__main__':
  main()
