#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Uses the closure compiler to check the ChromeVox javascript files.

With no arguments, checks all ChromeVox scripts.  If any arguments are
specified, only scripts that include any of the specified files will be
compiled.  A useful argument list is the output of the command
'git diff --name-only --relative'.
'''

import optparse
import os
import re
import sys

from multiprocessing import pool

from jsbundler import Bundle, CalcDeps, ReadSources
from jscompilerwrapper import RunCompiler

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CHROME_SOURCE_DIR = os.path.normpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 6))


def CVoxPath(path='.'):
  '''Converts a path relative to the top-level chromevox directory to a
  path relative to the current directory.
  '''
  return os.path.relpath(os.path.join(_SCRIPT_DIR, '..', path))


def ChromeRootPath(path='.'):
  '''Converts a path relative to the top-level chromevox directory to a
  path relative to the current directory.
  '''
  return os.path.relpath(os.path.join(_CHROME_SOURCE_DIR, path))


# AccessibilityPrivate externs file.
_ACCESSIBILITY_PRIVATE_EXTERNS = (
    ChromeRootPath(
        'third_party/closure_compiler/externs/accessibility_private.js'))

# Audio API externs file.
_AUDIO_EXTERNS = (
    ChromeRootPath('third_party/closure_compiler/externs/audio.js'))

# Automation API externs file.
_AUTOMATION_EXTERNS = (
    ChromeRootPath('third_party/closure_compiler/externs/automation.js'))

# MetricsPrivate externs file.
_METRICS_PRIVATE_EXTERNS = (
    ChromeRootPath('third_party/closure_compiler/externs/metrics_private.js'))

# LoginState externs file.
_LOGIN_STATE_EXTERNS = (
    ChromeRootPath('third_party/closure_compiler/externs/login_state.js'))

# Settings private API externs file.
_SETTINGS_PRIVATE_EXTERNS = (
    ChromeRootPath('third_party/closure_compiler/externs/settings_private.js'))

# Additional chrome api externs file.
_CHROME_EXTERNS = (
    ChromeRootPath('third_party/closure_compiler/externs/chrome.js'))

# Additional chrome extension api externs file.
_CHROME_EXTENSIONS_EXTERNS = (
    ChromeRootPath('third_party/closure_compiler/externs/chrome_extensions.js'))

# CommandLinePrivate externs file.
_COMMANDLINE_PRIVATE_EXTERNS = (
    ChromeRootPath(
        'third_party/closure_compiler/externs/command_line_private.js'))


# Externs common to many ChromeVox scripts.
_COMMON_EXTERNS = [
    CVoxPath('background/externs.js'),
    CVoxPath('common/chrome_extension_externs.js'),
    CVoxPath('common/externs.js'),
    _ACCESSIBILITY_PRIVATE_EXTERNS,
    _AUDIO_EXTERNS,
    _AUTOMATION_EXTERNS,
    _CHROME_EXTERNS,
    _CHROME_EXTENSIONS_EXTERNS,
    _COMMANDLINE_PRIVATE_EXTERNS,
    _METRICS_PRIVATE_EXTERNS,
    _LOGIN_STATE_EXTERNS,
    _SETTINGS_PRIVATE_EXTERNS,]

# List of top-level scripts and externs that we can check.
_TOP_LEVEL_SCRIPTS = [
    [[CVoxPath('background/learn_mode/kbexplorer_loader.js')], _COMMON_EXTERNS],
    [[CVoxPath('background/loader.js')], _COMMON_EXTERNS],
    [[CVoxPath('background/logging/log_loader.js')], _COMMON_EXTERNS],
    [[CVoxPath('background/options/options_loader.js')], _COMMON_EXTERNS],
    [[CVoxPath('background/panel/panel_loader.js')], _COMMON_EXTERNS],
    ]


def _Compile(js_files, externs):
  try:
    return RunCompiler(js_files, externs)
  except KeyboardInterrupt:
    return (False, 'KeyboardInterrupt')


def CheckChromeVox(changed_files=None):
  if changed_files is not None:
    changed_files_set = frozenset(
        (os.path.relpath(path) for path in changed_files))
    if len(changed_files_set) == 0:
      return (True, '')
  else:
    changed_files_set = None
  ret_success = True
  ret_output = ''
  roots = [CVoxPath(),
           os.path.relpath(
               os.path.join(
                   _CHROME_SOURCE_DIR,
                   'third_party/chromevox/third_party/closure-library/'
                   'closure/goog'))]
  sources = ReadSources(roots, need_source_text=True,
                        exclude=[re.compile('testing')])
  work_pool = pool.Pool(len(_TOP_LEVEL_SCRIPTS))
  try:
    results = []
    for top_level in _TOP_LEVEL_SCRIPTS:
      tl_files, externs = top_level
      bundle = Bundle()
      CalcDeps(bundle, sources, tl_files)
      bundle.Add((sources[name] for name in tl_files))
      ordered_paths = list(bundle.GetInPaths())
      if (changed_files_set is not None and
          changed_files_set.isdisjoint(ordered_paths + externs)):
        continue
      print 'Compiling %s' % ','.join(tl_files)
      results.append([tl_files,
                      work_pool.apply_async(
                          _Compile,
                          args=[ordered_paths, externs])])
    for result in results:
      tl_files = result[0]
      success, output = result[1].get()
      if not success:
        ret_output += '\nFrom compiling %s:\n%s\n' % (','.join(tl_files),
                                                      output)
        ret_success = False
    work_pool.close()
  except:
    work_pool.terminate()
    raise
  finally:
    work_pool.join()
  return (ret_success, ret_output)


def main():
  parser = optparse.OptionParser(description=__doc__)
  parser.usage = '%prog [<changed_file>...]'
  _, args = parser.parse_args()

  changed_paths = None
  if len(args) > 0:
    changed_paths = (os.path.relpath(p) for p in args)
  success, output = CheckChromeVox(changed_paths)
  if len(output) > 0:
    print output
  return int(not success)


if __name__ == '__main__':
  sys.exit(main())
