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

import compile_java
import javac_output_processor
from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


def ProcessJavacOutput(output, target_name):
  output_processor = javac_output_processor.JavacOutputProcessor(target_name)
  lines = output_processor.Process(output.split('\n'))
  return '\n'.join(lines)


def main(argv):
  build_utils.InitLogging('TURBINE_DEBUG')
  argv = build_utils.ExpandFileArgs(argv[1:])
  parser = argparse.ArgumentParser()
  action_helpers.add_depfile_arg(parser)
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

  options.classpath = action_helpers.parse_gn_list(options.classpath)
  options.processorpath = action_helpers.parse_gn_list(options.processorpath)
  options.processors = action_helpers.parse_gn_list(options.processors)
  options.java_srcjars = action_helpers.parse_gn_list(options.java_srcjars)

  files = []
  for arg in unknown_args:
    # Interpret a path prefixed with @ as a file containing a list of sources.
    if arg.startswith('@'):
      files.extend(build_utils.ReadSourcesList(arg[1:]))

  # The target's .sources file contains both Java and Kotlin files. We use
  # compile_kt.py to compile the Kotlin files to .class and header jars.
  # Turbine is run only on .java files.
  java_files = [f for f in files if f.endswith('.java')]

  cmd = build_utils.JavaCmd() + [
      '-classpath', options.turbine_jar_path, 'com.google.turbine.main.Main'
  ]
  javac_cmd = ['--release', '17']

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
    # Use jar_path to ensure paths are relative (needed for rbe).
    files_rsp_path = options.jar_path + '.java_files_list.txt'
    with open(files_rsp_path, 'w') as f:
      f.write('\n'.join(java_files))
    # Pass source paths as response files to avoid extremely long command
    # lines that are tedius to debug.
    cmd += ['--sources']
    cmd += ['@' + files_rsp_path]

  cmd += ['--javacopts']
  cmd += javac_cmd
  cmd += ['--']  # Terminate javacopts

  # Use AtomicOutput so that output timestamps are not updated when outputs
  # are not changed.
  with action_helpers.atomic_output(options.jar_path) as output_jar, \
      action_helpers.atomic_output(options.generated_jar_path) as gensrc_jar:
    cmd += ['--output', output_jar.name, '--gensrc_output', gensrc_jar.name]
    process_javac_output_partial = functools.partial(
        ProcessJavacOutput, target_name=options.target_name)

    logging.debug('Command: %s', cmd)
    start = time.time()
    try:
      build_utils.CheckOutput(cmd,
                              print_stdout=True,
                              stdout_filter=process_javac_output_partial,
                              stderr_filter=process_javac_output_partial,
                              fail_on_output=options.warnings_as_errors)
    except build_utils.CalledProcessError as e:
      # Do not output stacktrace as it takes up space on gerrit UI, forcing
      # you to click though to find the actual compilation error. It's never
      # interesting to see the Python stacktrace for a Java compilation error.
      sys.stderr.write(e.output)
      sys.exit(1)
    end = time.time() - start
    logging.info('Header compilation took %ss', end)
    if options.kotlin_jar_path:
      with zipfile.ZipFile(output_jar.name, 'a') as out_zip:
        path_transform = lambda p: p if p.endswith('.class') else None
        zip_helpers.merge_zips(out_zip, [options.kotlin_jar_path],
                               path_transform=path_transform)

  if options.depfile:
    # GN already knows of the java files, so avoid listing individual java files
    # in the depfile.
    depfile_deps = (options.classpath + options.processorpath +
                    options.java_srcjars)
    action_helpers.write_depfile(options.depfile, options.jar_path,
                                 depfile_deps)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
