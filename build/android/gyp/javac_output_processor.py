#!/usr/bin/env python3
#
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains helper class for processing javac output."""

import functools
import os
import pathlib
import posixpath
import re
import shlex
import sys
import traceback

from util import dep_utils


_PACKAGE_RE = re.compile(r'^package\s+(.*?)(;|\s*$)', flags=re.MULTILINE)


@functools.cache
def _running_locally():
  return os.path.exists('build.ninja')


@functools.cache  # pylint: disable=method-cache-max-size-none
def _extract_package_name(path):
  data = pathlib.Path(path).read_text('utf-8')
  if m := _PACKAGE_RE.search(data):
    return m.group(1)
  return None


class JavacOutputProcessor:
  def __init__(self, target_name):
    self._target_name = self._RemoveSuffixesIfPresent(
        ["__compile_java", "__errorprone", "__header"], target_name)
    self._missing_classes_short = set()
    self._missing_classes_full = set()
    self._suggested_targets = set()
    self._unresolvable_classes = set()

    # Examples:
    # ../../path/to/Foo.java:54: error: cannot find symbol
    #        ClangProfiler.writeClangProfilingProfile();
    #        ^
    # symbol:   variable ClangProfiler
    #
    # ../../path/to/Foo.java:69: error: could not resolve Linker.LibInfo
    #     static Linker.LibInfo anyLibInfo() {
    #            ^
    self._resolution_re = re.compile(
        # ../../ui/android/java/src/org/chromium/ui/base/Clipboard.java:45:
        r'^(?P<path>[-.\w/\\]+.java):(?:[0-9]+):'
        # error: package org.chromium.components.url_formatter does not exist
        r'(?: error: package [\w.]+ does not exist|'
        # error: cannot find symbol
        r' error: cannot find symbol|'
        # error: could not resolve Linker.LibInfo
        r' error: could not resolve (?P<unresolved>[A-Z]\w*).*|'
        # error: symbol not found org.chromium.url.GURL
        r' error: symbol not found [\w.]+)$'
        r'\s*(?:'
        # import org.chromium.url.GURL;
        r'import (?P<import>[\w\.]+);|'
        # import static org.chromium.url.GURL.method;
        r'import static (?P<static_import>[\w\.]+)\.\S+;|'
        r'.*)$'
        r'\s*\^*$'
        # symbol:   variable ClangProfiler
        r'\s*(symbol:\s+variable (?P<variable>[A-Z]\w+))?',
        re.MULTILINE)

    self._class_lookup_index = None

  def Process(self, output):
    self._LookForUnknownSymbols(output)

    if not self._missing_classes_full:
      return output

    sb = []
    sb.append('💡 One or more errors due to missing GN deps 💡')
    if self._unresolvable_classes:
      sb.append('Failed to find targets for the following classes:')
      for class_name in sorted(self._unresolvable_classes):
        sb.append(f'* {class_name}')
    if self._suggested_targets:
      sb.append('Hint: Try adding the following to ' + self._target_name)

      for targets in sorted(self._suggested_targets):
        if len(targets) > 1:
          suggested_targets_str = 'one of: ' + ', '.join(targets)
        else:
          suggested_targets_str = targets[0]
        # Show them in quotes so they can be copy/pasted into BUILD.gn files.
        sb.append(f'    "{suggested_targets_str}",')

      sb.append('')
      sb.append('Hint: Run the following command to add the missing deps:')
      missing_targets = {targets[0] for targets in self._suggested_targets}
      cmd = dep_utils.CreateAddDepsCommand(self._target_name,
                                           sorted(missing_targets))
      sb.append(f'    {shlex.join(cmd)}')
      sb.append('')
      sb.append('Use tools/android/auto_fix_missing_java_deps.py'
                ' to auto-apply suggestions.')
    elif not self._unresolvable_classes:
      sb.append(
          'Hint: Rebuild with -config no-remote-javac to show missing deps.')

    sb.append('')
    return output + '\n'.join(sb)

  def _LookForUnknownSymbols(self, output):
    for m in self._resolution_re.finditer(output):
      full_class_name = m.group('import') or m.group('static_import')
      if not full_class_name:
        short_class_name = m.group('unresolved') or m.group('variable')
        if not short_class_name:
          continue
        if short_class_name in self._missing_classes_short:
          continue
        path = m.group('path')
        if os.path.exists(path):
          package_name = _extract_package_name(path)
          if not package_name:
            continue
        else:
          # This can happen for .srcjars in turbine.py (which directly supports
          # .srcjar files).
          # E.g.: "org/chromium/network/mojom/ParsedHeaders.java"
          package_name = posixpath.dirname(path).replace('/', '.')
        full_class_name = f'{package_name}.{short_class_name}'

      if full_class_name not in self._missing_classes_full:
        short_class_name = full_class_name.split('.')[-1]
        self._missing_classes_short.add(short_class_name)
        self._missing_classes_full.add(full_class_name)
        if _running_locally():
          try:
            self._AnalyzeMissingClass(full_class_name)
          except Exception:
            sys.stderr.write('Error in _AnalyzeMissingClass---\n' +
                             traceback.format_exc())
            return

  def _AnalyzeMissingClass(self, class_name):
    if self._class_lookup_index is None:
      self._class_lookup_index = dep_utils.ClassLookupIndex(
          pathlib.Path(os.getcwd()),
          should_build=False,
      )

    suggested_deps = self._class_lookup_index.match(class_name)

    if not suggested_deps:
      self._unresolvable_classes.add(class_name)
      return

    suggested_deps = dep_utils.DisambiguateDeps(suggested_deps)
    self._suggested_targets.add(tuple(d.target for d in suggested_deps))

  @staticmethod
  def _RemoveSuffixesIfPresent(suffixes, text):
    for suffix in suffixes:
      if text.endswith(suffix):
        return text[:-len(suffix)]
    return text


def Process(target_name, output):
  processor = JavacOutputProcessor(target_name)
  return processor.Process(output)
