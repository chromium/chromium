#!/usr/bin/env python3
#
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Instruments classes and jar files.

This script corresponds to the 'jacoco_instr' action in the Java build process.
Depending on whether jacoco_instrument is set, the 'jacoco_instr' action will
call the instrument command which accepts a jar and instruments it using
jacococli.jar.

"""

import argparse
import json
import os
import shutil
import sys
import zipfile

from util import build_utils
import action_helpers
import zip_helpers


# This should be same as recipe side token. See bit.ly/3STSPcE.
INSTRUMENT_ALL_JACOCO_OVERRIDE_TOKEN = 'INSTRUMENT_ALL_JACOCO'


def _AddArguments(parser):
  """Adds arguments related to instrumentation to parser.

  Args:
    parser: ArgumentParser object.
  """
  parser.add_argument(
      '--input-path',
      required=True,
      help='Path to input file(s). Either the classes '
      'directory, or the path to a jar.')
  parser.add_argument(
      '--output-path',
      required=True,
      help='Path to output final file(s) to. Either the '
      'final classes directory, or the directory in '
      'which to place the instrumented/copied jar.')
  parser.add_argument(
      '--sources-json-file',
      required=True,
      help='File to create with the list of source directories '
      'and input path.')
  parser.add_argument(
      '--target-sources-file',
      required=True,
      help='File containing newline-separated .java and .kt paths')
  parser.add_argument(
      '--jacococli-jar', required=True, help='Path to jacococli.jar.')
  parser.add_argument(
      '--files-to-instrument',
      help='Path to a file containing which source files are affected.')


def _GetSourceDirsFromSourceFiles(source_files):
  """Returns list of directories for the files in |source_files|.

  Args:
    source_files: List of source files.

  Returns:
    List of source directories.
  """
  return list(set(os.path.dirname(source_file) for source_file in source_files))


def _CreateSourcesJsonFile(source_dirs, input_path, sources_json_file,
                           src_root):
  """Adds all normalized source directories and input path to
  |sources_json_file|.

  Args:
    source_dirs: List of source directories.
    input_path: The input path to non-instrumented class files.
    sources_json_file: File into which to write the list of source directories
    and input path.
    src_root: Root which sources added to the file should be relative to.

  Returns:
    An exit code.
  """
  src_root = os.path.abspath(src_root)
  relative_sources = []
  for s in source_dirs:
    abs_source = os.path.abspath(s)
    if abs_source[:len(src_root)] != src_root:
      print('Error: found source directory not under repository root: %s %s' %
            (abs_source, src_root))
      return 1
    rel_source = os.path.relpath(abs_source, src_root)

    relative_sources.append(rel_source)

  data = {}
  data['source_dirs'] = relative_sources
  data['input_path'] = []
  data['output_dir'] = src_root
  if input_path:
    data['input_path'].append(os.path.abspath(input_path))
  with open(sources_json_file, 'w') as f:
    json.dump(data, f)
  return 0


def _GetAffectedClasses(jar_file, source_files):
  """Gets affected classes by affected source files to a jar.

  Args:
    jar_file: The jar file to get all members.
    source_files: The list of affected source files.

  Returns:
    A tuple of affected classes and unaffected members.
  """
  with zipfile.ZipFile(jar_file) as f:
    members = f.namelist()

  affected_classes = []
  unaffected_members = []

  for member in members:
    if not member.endswith('.class'):
      unaffected_members.append(member)
      continue

    is_affected = False
    index = member.find('$')
    if index == -1:
      index = member.find('.class')
    for source_file in source_files:
      if source_file.endswith(
          (member[:index] + '.java', member[:index] + '.kt')):
        affected_classes.append(member)
        is_affected = True
        break
    if not is_affected:
      unaffected_members.append(member)

  return affected_classes, unaffected_members


def _InstrumentClassFiles(instrument_cmd,
                          input_path,
                          output_path,
                          temp_dir,
                          affected_source_files=None):
  """Instruments class files from input jar.

  Args:
    instrument_cmd: JaCoCo instrument command.
    input_path: The input path to non-instrumented jar.
    output_path: The output path to instrumented jar.
    temp_dir: The temporary directory.
    affected_source_files: The affected source file paths to input jar.
      Default is None, which means instrumenting all class files in jar.
  """
  affected_classes = None
  unaffected_members = None
  if affected_source_files:
    affected_classes, unaffected_members = _GetAffectedClasses(
        input_path, affected_source_files)

  # Extract affected class files.
  with zipfile.ZipFile(input_path) as f:
    f.extractall(temp_dir, affected_classes)

  instrumented_dir = os.path.join(temp_dir, 'instrumented')

  # Instrument extracted class files.
  instrument_cmd.extend([temp_dir, '--dest', instrumented_dir])
  build_utils.CheckOutput(instrument_cmd)

  if affected_source_files and unaffected_members:
    # Extract unaffected members to instrumented_dir.
    with zipfile.ZipFile(input_path) as f:
      f.extractall(instrumented_dir, unaffected_members)

  # Zip all files to output_path
  with action_helpers.atomic_output(output_path) as f:
    zip_helpers.zip_directory(f, instrumented_dir)


def _RunInstrumentCommand(parser):
  """Instruments class or Jar files using JaCoCo.

  Args:
    parser: ArgumentParser object.

  Returns:
    An exit code.
  """
  args = parser.parse_args()

  source_files = []
  if args.target_sources_file:
    source_files.extend(build_utils.ReadSourcesList(args.target_sources_file))

  with build_utils.TempDir() as temp_dir:
    instrument_cmd = build_utils.JavaCmd() + [
        '-jar', args.jacococli_jar, 'instrument'
    ]

    if not args.files_to_instrument:
      affected_source_files = None
    else:
      affected_files = build_utils.ReadSourcesList(args.files_to_instrument)
      # Check if coverage recipe decided to instrument everything by overriding
      # the try builder default setting(selective instrumentation). This can
      # happen in cases like a DEPS roll of jacoco library

      # Note: This token is preceded by ../../ because the paths to be
      # instrumented are expected to be relative to the build directory.
      # See _rebase_paths() at https://bit.ly/40oiixX
      token = '../../' + INSTRUMENT_ALL_JACOCO_OVERRIDE_TOKEN
      if token in affected_files:
        affected_source_files = None
      else:
        source_set = set(source_files)
        affected_source_files = [f for f in affected_files if f in source_set]

        # Copy input_path to output_path and return if no source file affected.
        if not affected_source_files:
          shutil.copyfile(args.input_path, args.output_path)
          # Create a dummy sources_json_file.
          _CreateSourcesJsonFile([], None, args.sources_json_file,
                                 build_utils.DIR_SOURCE_ROOT)
          return 0
    _InstrumentClassFiles(instrument_cmd, args.input_path, args.output_path,
                          temp_dir, affected_source_files)

  source_dirs = _GetSourceDirsFromSourceFiles(source_files)
  # TODO(GYP): In GN, we are passed the list of sources, detecting source
  # directories, then walking them to re-establish the list of sources.
  # This can obviously be simplified!
  _CreateSourcesJsonFile(source_dirs, args.input_path, args.sources_json_file,
                         build_utils.DIR_SOURCE_ROOT)

  return 0


def main():
  parser = argparse.ArgumentParser()
  _AddArguments(parser)
  _RunInstrumentCommand(parser)


if __name__ == '__main__':
  sys.exit(main())
