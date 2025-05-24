#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs R8's TraceReferences tool to ensure DEX files are valid."""

import argparse
import json
import logging
import os
import pathlib
import re
import sys

from util import build_utils
from util import server_utils
import action_helpers  # build_utils adds //build to sys.path.

_DUMP_DIR_NAME = 'r8inputs_tracerefs'

_SUPPRESSION_PATTERN = '|'.join([
    # Summary contains warning count, which our filtering makes wrong.
    r'Warning: Tracereferences found',
    r'dalvik\.system',
    r'libcore\.io',
    r'sun\.misc\.Unsafe',

    # Explicictly guarded by try (NoClassDefFoundError) in Flogger's
    # PlatformProvider.
    r'com\.google\.common\.flogger\.backend\.google\.GooglePlatform',
    r'com\.google\.common\.flogger\.backend\.system\.DefaultPlatform',

    # TODO(agrieve): Exclude these only when use_jacoco_coverage=true.
    r'java\.lang\.instrument\.ClassFileTransformer',
    r'java\.lang\.instrument\.IllegalClassFormatException',
    r'java\.lang\.instrument\.Instrumentation',
    r'java\.lang\.management\.ManagementFactory',
    r'javax\.management\.MBeanServer',
    r'javax\.management\.ObjectInstance',
    r'javax\.management\.ObjectName',
    r'javax\.management\.StandardMBean',

    # Explicitly guarded by try (NoClassDefFoundError) in Firebase's
    # KotlinDetector: com.google.firebase.platforminfo.KotlinDetector.
    r'kotlin\.KotlinVersion',

    # Not sure why these two are missing, but they do not seem important.
    r'ResultIgnorabilityUnspecified',
    r'kotlin\.DeprecationLevel',

    # Assume missing android.* / java.* references are OS APIs that are not in
    # android.jar. Not in the above list so as to not match parameter types.
    # E.g. Missing method void android.media.MediaRouter2$RouteCallback
    # E.g. Missing class android.util.StatsEvent$Builder
    r'Missing method \S+ android\.',
    r'Missing class android\.',

    # The follow classes are from Android XR system libraries and used on
    # immersive environment.
    r'Missing class com.google.ar.imp.core\.',
])


def _RunTraceReferences(error_title, r8jar, libs, dex_files, options):
  cmd = build_utils.JavaCmd(xmx='2G')

  if options.dump_inputs:
    cmd += [f'-Dcom.android.tools.r8.dumpinputtodirectory={_DUMP_DIR_NAME}']

  cmd += [
      '-cp', r8jar, 'com.android.tools.r8.tracereferences.TraceReferences',
      '--map-diagnostics:MissingDefinitionsDiagnostic', 'error', 'warning',
      '--check'
  ]

  for path in libs:
    cmd += ['--lib', path]
  for path in dex_files:
    cmd += ['--source', path]

  failed_holder = [False]

  def stderr_filter(stderr):

    had_unfiltered_items = '  ' in stderr
    stderr = build_utils.FilterLines(stderr, _SUPPRESSION_PATTERN)
    if stderr:
      if 'Missing' in stderr:
        failed_holder[0] = True
        stderr = 'TraceReferences failed: ' + error_title + """
Tip: Build with:
        is_java_debug=false
        treat_warnings_as_errors=false
        enable_proguard_obfuscation=false
     and then use dexdump to see which class(s) reference them.

     E.g.:
       third_party/android_sdk/public/build-tools/*/dexdump -d \
out/Release/apks/YourApk.apk > dex.txt
""" + stderr
      elif had_unfiltered_items:
        # Left only with empty headings. All indented items filtered out.
        stderr = ''
    return stderr

  try:
    if options.verbose:
      stderr_filter = None
    build_utils.CheckOutput(cmd,
                            print_stdout=True,
                            stderr_filter=stderr_filter,
                            fail_on_output=options.warnings_as_errors)
  except build_utils.CalledProcessError as e:
    # Do not output command line because it is massive and makes the actual
    # error message hard to find.
    sys.stderr.write(e.output)
    sys.exit(1)
  return failed_holder[0]


def main():
  build_utils.InitLogging('TRACEREFS_DEBUG')

  parser = argparse.ArgumentParser()
  parser.add_argument('--tracerefs-json')
  parser.add_argument('--use-build-server',
                      action='store_true',
                      help='Always use the build server.')
  parser.add_argument('--stamp')
  parser.add_argument('--depfile')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  parser.add_argument('--dump-inputs',
                      action='store_true',
                      help='Use when filing R8 bugs to capture inputs.'
                      ' Stores inputs to r8inputs.zip')
  parser.add_argument('--verbose',
                      action='store_true',
                      help='Do not filter output')
  args = parser.parse_args()

  with open(args.tracerefs_json) as f:
    spec = json.load(f)
  r8jar = spec['r8jar']
  libs = spec['libs']

  # No need for r8jar here because GN knows about it already.
  depfile_deps = []
  depfile_deps += libs
  for job in spec['jobs']:
    depfile_deps += job['jars']

  action_helpers.write_depfile(args.depfile, args.stamp, depfile_deps)

  if server_utils.MaybeRunCommand(name=args.stamp,
                                  argv=sys.argv,
                                  stamp_file=args.stamp,
                                  use_build_server=args.use_build_server):
    return

  if args.dump_inputs:
    # Dumping inputs causes output to be emitted, avoid failing due to stdout.
    args.warnings_as_errors = False

    # Use dumpinputtodirectory instead of dumpinputtofile to avoid failing the
    # build and keep running tracereferences.
    dump_dir_name = _DUMP_DIR_NAME
    dump_dir_path = pathlib.Path(dump_dir_name)
    if dump_dir_path.exists():
      shutil.rmtree(dump_dir_path)
    # The directory needs to exist before r8 adds the zip files in it.
    dump_dir_path.mkdir()

  logging.debug('Running TraceReferences')
  error_title = 'DEX contains references to non-existent symbols after R8.'
  for job in spec['jobs']:
    name = job['name']
    dex_files = job['jars']
    if _RunTraceReferences(error_title, r8jar, libs, dex_files, args):
      # Failed but didn't raise due to warnings_as_errors=False
      break
    error_title = (f'DEX within module "{name}" contains reference(s) to '
                   'symbols within child splits')

  logging.info('Checks completed.')
  server_utils.MaybeTouch(args.stamp)


if __name__ == '__main__':
  main()
