#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wraps the turbine jar and expands @FileArgs."""

import argparse
import functools
import logging
import sys
import time
import zipfile

import javac_output_processor
from util import build_utils
import compile_java


def ProcessJavacOutput(output, target_name):
  output_processor = javac_output_processor.JavacOutputProcessor(target_name)
  lines = output_processor.Process(output.split('\n'))
  return '\n'.join(lines)


def main(argv):
  build_utils.InitLogging('TURBINE_DEBUG')
  argv = build_utils.ExpandFileArgs(argv[1:])
  parser = argparse.ArgumentParser()
  build_utils.AddDepfileOption(parser)
  parser.add_argument('--target-name', help='Fully qualified GN target name.')
  parser.add_argument(
      '--turbine-jar-path', required=True, help='Path to the turbine jar file.')
  parser.add_argument(
      '--java-srcjars',
      action='append',
      default=[],
      help='List of srcjars to include in compilation.')
  parser.add_argument('--classpath', action='append', help='Classpath to use.')
  parser.add_argument(
      '--processors',
      action='append',
      help='GN list of annotation processor main classes.')
  parser.add_argument(
      '--processorpath',
      action='append',
      help='GN list of jars that comprise the classpath used for Annotation '
      'Processors.')
  parser.add_argument(
      '--processor-args',
      action='append',
      help='key=value arguments for the annotation processors.')
  parser.add_argument('--jar-path', help='Jar output path.', required=True)
  parser.add_argument(
      '--generated-jar-path',
      required=True,
      help='Output path for generated source files.')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  parser.add_argument('--kotlin-jar-path',
                      help='Kotlin jar to be merged into the output jar.')
  options, unknown_args = parser.parse_known_args(argv)

  options.classpath = build_utils.ParseGnList(options.classpath)
  options.processorpath = build_utils.ParseGnList(options.processorpath)
  options.processors = build_utils.ParseGnList(options.processors)
  options.java_srcjars = build_utils.ParseGnList(options.java_srcjars)

  files = []
  for arg in unknown_args:
    # Interpret a path prefixed with @ as a file containing a list of sources.
    if arg.startswith('@'):
      files.extend(build_utils.ReadSourcesList(arg[1:]))

  # The target's .sources file contains both Java and Kotlin files. We use
  # compile_kt.py to compile the Kotlin files to .class and header jars.
  # Turbine is run only on .java files.
  java_files = [f for f in files if f.endswith('.java')]

  with build_utils.TempDir() as intermediate_dir:
    if java_files:
      # Rewrite instances of android.support.test to androidx.test. See
      # https://crbug.com/1223832
      java_files = compile_java.MaybeRewriteAndroidSupport(
          java_files, intermediate_dir)

    cmd = build_utils.JavaCmd() + [
        '-classpath', options.turbine_jar_path, 'com.google.turbine.main.Main'
    ]
    javac_cmd = [
        # We currently target JDK 11 everywhere.
        '--release',
        '11',
    ]

    # Turbine reads lists from command line args by consuming args until one
    # starts with double dash (--). Thus command line args should be grouped
    # together and passed in together.
    if options.processors:
      cmd += ['--processors']
      cmd += options.processors

    if options.processorpath:
      cmd += ['--processorpath']
      cmd += options.processorpath

    if options.processor_args:
      for arg in options.processor_args:
        javac_cmd.extend(['-A%s' % arg])

    if options.classpath:
      cmd += ['--classpath']
      cmd += options.classpath

    if options.java_srcjars:
      cmd += ['--source_jars']
      cmd += options.java_srcjars

    if java_files:
      # Use jar_path to ensure paths are relative (needed for goma).
      files_rsp_path = options.jar_path + '.java_files_list.txt'
      with open(files_rsp_path, 'w') as f:
        f.write(' '.join(java_files))
      # Pass source paths as response files to avoid extremely long command
      # lines that are tedius to debug.
      cmd += ['--sources']
      cmd += ['@' + files_rsp_path]

    cmd += ['--javacopts']
    cmd += javac_cmd
    cmd += ['--']  # Terminate javacopts

    # Use AtomicOutput so that output timestamps are not updated when outputs
    # are not changed.
    with build_utils.AtomicOutput(options.jar_path) as output_jar, \
        build_utils.AtomicOutput(options.generated_jar_path) as generated_jar:
      cmd += [
          '--output', output_jar.name, '--gensrc_output', generated_jar.name
      ]

      process_javac_output_partial = functools.partial(
          ProcessJavacOutput, target_name=options.target_name)

      logging.debug('Command: %s', cmd)
      start = time.time()
      build_utils.CheckOutput(cmd,
                              print_stdout=True,
                              stdout_filter=process_javac_output_partial,
                              stderr_filter=process_javac_output_partial,
                              fail_on_output=options.warnings_as_errors)
      end = time.time() - start
      logging.info('Header compilation took %ss', end)
      if options.kotlin_jar_path:
        with zipfile.ZipFile(output_jar.name, 'a') as out_zip:
          build_utils.MergeZips(out_zip, [options.kotlin_jar_path],
                                path_transform=lambda p: p
                                if p.endswith('.class') else None)

  if options.depfile:
    # GN already knows of the java files, so avoid listing individual java files
    # in the depfile.
    depfile_deps = (options.classpath + options.processorpath +
                    options.java_srcjars)
    build_utils.WriteDepfile(options.depfile, options.jar_path, depfile_deps)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
