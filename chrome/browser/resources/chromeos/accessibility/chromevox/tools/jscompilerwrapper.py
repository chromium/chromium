#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Uses the closure compiler to check syntax and semantics of a js module
with dependencies.'''

import os
import re
import subprocess
import sys

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CHROME_SOURCE_DIR = os.path.normpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 7))

# Compiler path.
_CLOSURE_COMPILER_JAR = os.path.join(_CHROME_SOURCE_DIR, 'third_party',
                                     'closure_compiler', 'compiler',
                                     'compiler.jar')

# List of compilation errors to enable with the --jscomp_errors flag.
_JSCOMP_ERRORS = [
    'accessControls', 'checkTypes', 'checkVars', 'invalidCasts',
    'missingProperties', 'undefinedNames', 'undefinedVars', 'visibility'
]

# List of compilation groups to turn off with the --jscomp_off flag.
_JSCOMP_OFF = ['duplicate']

_java_executable = 'java'


def _Error(msg):
  print >> sys.stderr, msg
  sys.exit(1)


def _ExecuteCommand(args, ignore_exit_status=False):
  try:
    return subprocess.check_output(args, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    if ignore_exit_status and e.returncode > 0:
      return e.output
    _Error('%s\nCommand \'%s\' returned non-zero exit status %d' %
           (e.output, ' '.join(e.cmd), e.returncode))
  except (OSError, IOError) as e:
    _Error('Error executing %s: %s' % (_java_executable, str(e)))


def RunCompiler(js_files, externs=[]):
  args = [_java_executable, '-jar', _CLOSURE_COMPILER_JAR]
  args.extend(['--compilation_level', 'SIMPLE_OPTIMIZATIONS'])
  args.extend(['--jscomp_error=%s' % error for error in _JSCOMP_ERRORS])
  args.extend(['--jscomp_off=%s' % off for off in _JSCOMP_OFF])
  args.extend(['--language_in', 'ECMASCRIPT_NEXT'])
  args.extend(['--externs=%s' % extern for extern in externs])
  args.extend(['--js=%s' % js for js in js_files])
  args.extend(['--js_output_file', '/dev/null'])
  output = _ExecuteCommand(args, ignore_exit_status=True)
  success = len(output) == 0
  return success, output
