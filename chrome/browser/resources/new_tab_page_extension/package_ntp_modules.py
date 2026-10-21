# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Packages chrome://new-tab-page modules for use by the NTP extension.

Collects the given NTP modules (the extension's entry points into the NTP) and
the NTP modules they transitively import from the chrome://new-tab-page
compiled output. Checks that the entry points are the NTP modules the
extension's compiled sources import, and that the collected modules match the
checked-in list of expected modules, so that any change to the set of NTP
modules the extension depends on is reviewed.

Unless --check_only is passed, also copies them to an output folder so that the
extension can serve them from its own origin under /ntp/, and writes a manifest
of the copied files for use with generate_grd(). While copying, rewrites
scheme-relative '//resources/' URLs to absolute 'chrome://resources/' URLs, as
they would otherwise resolve against the extension's origin, both in module
imports (which resolve against the importing module's URL) and in CSS.
"""

import argparse
import json
import os
import re
import shutil
import sys

# Matches the specifier of static imports/exports and dynamic imports, e.g.
#   import './foo.js';  import {a} from './foo.js';  export * from './foo.js';
#   import('./foo.js')
_IMPORT_RE = re.compile(
  r"""(?:\bimport\s*\(\s*|\bimport\s+|\bfrom\s+)(['"])([^'"\n]+)\1"""
)
_SCHEME_RELATIVE_RESOURCES_RE = re.compile(r"(?<![\w:])//resources/")
_NTP_URL_PREFIX = 'chrome://new-tab-page/'
# Prefix under which the extension imports and serves the NTP modules.
_EXTENSION_NTP_PREFIX = '/ntp/'


def _resolve(in_folder, importer, specifier):
  """Returns the path, relative to `in_folder`, of the module that `specifier`
  refers to if it is a relative import of an NTP module, or None otherwise.

  Other imports, e.g. of chrome://resources/ modules or root-relative imports
  like '/strings.m.js', are left as is: they resolve the same way on the
  extension's origin, or are served by the extension itself.
  """
  if specifier.startswith(_NTP_URL_PREFIX):
    # Imports between NTP modules are relative. Absolute ones would point to
    # chrome://new-tab-page, which extension pages can't load.
    raise ValueError(
      f'{importer} imports {specifier}. The NTP extension only supports '
      'relative imports between NTP modules.'
    )
  if not specifier.startswith('./') and not specifier.startswith('../'):
    return None
  resolved = os.path.normpath(
    os.path.join(os.path.dirname(importer), specifier)
  )
  if resolved.startswith('..'):
    raise ValueError(
      f'{importer} imports {specifier}, which is outside of {in_folder}'
    )
  return resolved


def _collect(in_folder, entry_points):
  """Returns the sorted paths, relative to `in_folder`, of `entry_points` and
  the NTP modules they transitively import."""
  pending = []
  for entry_point in entry_points:
    module = os.path.normpath(entry_point)
    if module.startswith('..'):
      raise ValueError(f'Entry point {entry_point} is outside of {in_folder}')
    pending.append(module)
  visited = set()
  while pending:
    module = pending.pop()
    if module in visited:
      continue
    visited.add(module)
    in_path = os.path.join(in_folder, module)
    if not os.path.exists(in_path):
      raise ValueError(f'{in_path} not found')
    with open(in_path, 'r', encoding='utf-8') as f:
      for match in _IMPORT_RE.finditer(f.read()):
        resolved = _resolve(in_folder, module, match.group(2))
        if resolved and resolved not in visited:
          pending.append(resolved)
  return sorted(module.replace(os.sep, '/') for module in visited)


def _check_entry_points(entry_points, sources):
  """Checks that `entry_points` are exactly the NTP modules that the
  extension's compiled `sources` import, i.e. that BUILD.gn lists every NTP
  module the extension loads at runtime. Type-only imports are compiled away."""
  imported = set()
  for source in sources:
    with open(source, 'r', encoding='utf-8') as f:
      for match in _IMPORT_RE.finditer(f.read()):
        specifier = match.group(2)
        if specifier.startswith(_EXTENSION_NTP_PREFIX):
          imported.add(specifier[len(_EXTENSION_NTP_PREFIX):])
  listed = set(os.path.normpath(e).replace(os.sep, '/') for e in entry_points)
  if imported == listed:
    return
  message = ['The NTP extension\'s NTP entry points are out of date. Update '
             '`_ntp_entry_points` in BUILD.gn.']
  missing = sorted(imported - listed)
  unused = sorted(listed - imported)
  if missing:
    message.append('Imported but not listed: ' + ', '.join(missing))
  if unused:
    message.append('Listed but not imported: ' + ', '.join(unused))
  raise ValueError('\n'.join(message))


def _check_expected(modules, expected_modules_path):
  with open(expected_modules_path, 'r', encoding='utf-8') as f:
    expected = sorted(
      line.strip()
      for line in f
      if line.strip() and not line.lstrip().startswith('#')
    )
  if modules == expected:
    return
  added = sorted(set(modules) - set(expected))
  removed = sorted(set(expected) - set(modules))
  message = [
    'The NTP modules that the NTP extension depends on changed.',
    f'Update {expected_modules_path} after making sure the change is intended.',
  ]
  if added:
    message.append('Added: ' + ', '.join(added))
  if removed:
    message.append('Removed: ' + ', '.join(removed))
  raise ValueError('\n'.join(message))


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  # The NTP modules the extension imports, relative to `--in_folder`.
  parser.add_argument('--entry_points', nargs='+', required=True)
  # The extension's compiled sources, whose /ntp/ imports must match
  # `--entry_points`.
  parser.add_argument('--sources', nargs='+', required=True)
  parser.add_argument('--expected_modules', required=True)
  parser.add_argument('--depfile', required=True)
  # The file to write once the check passes. When copying, this is the
  # manifest of the copied files.
  parser.add_argument('--output', required=True)
  parser.add_argument('--out_folder')
  parser.add_argument('--check_only', action='store_true')
  args = parser.parse_args(argv)
  if not args.check_only and not args.out_folder:
    parser.error('--out_folder is required unless --check_only is passed')

  modules = _collect(args.in_folder, args.entry_points)
  _check_entry_points(args.entry_points, args.sources)
  _check_expected(modules, args.expected_modules)

  if args.check_only:
    with open(args.output, 'w', encoding='utf-8') as f:
      f.write('\n'.join(modules) + '\n')
  else:
    # Start from an empty folder so that modules the extension no longer uses
    # don't linger from previous builds.
    shutil.rmtree(args.out_folder, ignore_errors=True)
    for module in modules:
      with open(
        os.path.join(args.in_folder, module), 'r', encoding='utf-8'
      ) as f:
        content = f.read()
      out_path = os.path.join(args.out_folder, module)
      os.makedirs(os.path.dirname(out_path), exist_ok=True)
      with open(out_path, 'w', encoding='utf-8', newline='') as f:
        f.write(
          _SCHEME_RELATIVE_RESOURCES_RE.sub('chrome://resources/', content)
        )
    with open(args.output, 'w', encoding='utf-8') as f:
      json.dump({'base_dir': args.out_folder, 'files': modules}, f)

  with open(args.depfile, 'w', encoding='utf-8') as f:
    f.write(
      args.output
      + ': '
      + ' '.join(
        [os.path.join(args.in_folder, module) for module in modules]
        + args.sources
      )
      + '\n'
    )


if __name__ == '__main__':
  main(sys.argv[1:])
