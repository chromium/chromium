#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A bindings generator for JNI on Android."""

import argparse
import os
import shutil
import sys

import jni_generator
import jni_registration_generator

# jni_zero.py requires Python 3.8+.
_MIN_PYTHON_MINOR = 8


def _add_common_args(parser):
  parser.add_argument(
      '--use-proxy-hash',
      action='store_true',
      help='Enables hashing of the native declaration for methods in '
      'a @NativeMethods interface')
  parser.add_argument('--enable-jni-multiplexing',
                      action='store_true',
                      help='Enables JNI multiplexing for Java native methods')
  parser.add_argument(
      '--package-prefix',
      help='Adds a prefix to the classes fully qualified-name. Effectively '
      'changing a class name from foo.bar -> prefix.foo.bar')


def _add_library_args(parser):
  _add_common_args(parser)
  parser.add_argument(
      '--jar-file',
      help='Extract the list of input files from a specified jar file. Uses '
      'javap to extract the methods from a pre-compiled class. --input should '
      'point to pre-compiled Java .class files.')
  parser.add_argument(
      '--namespace',
      help='Uses as a namespace in the generated header instead of the javap '
      'class name, or when there is no JNINamespace annotation in the java '
      'source.')
  parser.add_argument('--input-file',
                      action='append',
                      required=True,
                      dest='input_files',
                      help='Input filenames, or paths within a .jar if '
                      '--jar-file is used.')
  parser.add_argument('--output-dir',
                      required=True,
                      help='Output directory. '
                      'Existing .h files in this directory will be assumed '
                      'stale and removed.')
  parser.add_argument('--output-name',
                      action='append',
                      dest='output_names',
                      help='Output filenames within output directory.')
  parser.add_argument('--srcjar-path',
                      help='Path to output srcjar for generated .java files.')
  parser.add_argument('--extra-include',
                      action='append',
                      dest='extra_includes',
                      help='Header file to #include in the generated header.')
  parser.add_argument('--javap', help='The path to javap command.')
  parser.add_argument('--enable-profiling',
                      action='store_true',
                      help='Add additional profiling instrumentation.')
  parser.add_argument('--unchecked-exceptions',
                      action='store_true',
                      help='Do not check that no exceptions were thrown.')
  parser.add_argument(
      '--split-name',
      help='Split name that the Java classes should be loaded from.')

  parser.set_defaults(func=jni_generator.main)


def _add_final_args(parser):
  _add_common_args(parser)
  parser.add_argument('--depfile',
                      help='Path to depfile (for use with ninja build system)')

  parser.add_argument('--native-sources-file',
                      help='A file which contains Java file paths, derived '
                      'from native deps onto generate_jni.')
  parser.add_argument('--java-sources-file',
                      required=True,
                      help='A file which contains Java file paths, derived '
                      'from java deps metadata.')
  parser.add_argument(
      '--add-stubs-for-missing-native',
      action='store_true',
      help='Adds stub methods for any --java-sources-file which are missing '
      'from --native-sources-files. If not passed, we will assert that none of '
      'these exist.')
  parser.add_argument(
      '--remove-uncalled-methods',
      action='store_true',
      help='Removes --native-sources-files which are not in '
      '--java-sources-file. If not passed, we will assert that none of these '
      'exist.')
  parser.add_argument('--header-path', help='Path to output header file.')
  parser.add_argument(
      '--srcjar-path',
      required=True,
      help='Path to output srcjar for GEN_JNI.java (and J/N.java if proxy'
      ' hash is enabled).')
  parser.add_argument(
      '--namespace',
      help='Native namespace to wrap the registration functions into.')
  # TODO(crbug.com/898261) hook these flags up to the build config to enable
  # mocking in instrumentation tests
  parser.add_argument(
      '--enable-proxy-mocks',
      default=False,
      action='store_true',
      help='Allows proxy native impls to be mocked through Java.')
  parser.add_argument(
      '--require-mocks',
      default=False,
      action='store_true',
      help='Requires all used native implementations to have a mock set when '
      'called. Otherwise an exception will be thrown.')
  parser.add_argument(
      '--module-name',
      help='Only look at natives annotated with a specific module name.')
  parser.add_argument('--manual-jni-registration',
                      action='store_true',
                      help='Generate a call to RegisterNatives()')
  parser.add_argument('--include-test-only',
                      action='store_true',
                      help='Whether to maintain ForTesting JNI methods.')

  parser.set_defaults(func=jni_registration_generator.main)


def _maybe_relaunch_with_newer_python():
  # If "python3" is < python3.8, but a newer version is available, then use
  # that.
  py_version = sys.version_info
  if py_version < (3, _MIN_PYTHON_MINOR):
    if os.environ.get('JNI_ZERO_RELAUNCHED'):
      sys.stderr.write('JNI_ZERO_RELAUNCHED failure.\n')
      sys.exit(1)
    for i in range(_MIN_PYTHON_MINOR, 30):
      name = f'python3.{i}'
      if shutil.which(name):
        cmd = [name] + sys.argv
        env = os.environ.copy()
        env['JNI_ZERO_RELAUNCHED'] = '1'
        os.execvpe(cmd[0], cmd, env)
    sys.stderr.write(
        f'jni_zero requires Python 3.{_MIN_PYTHON_MINOR} or greater.\n')
    sys.exit(1)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  subparsers = parser.add_subparsers(required=True)

  subp = subparsers.add_parser(
      'generate-library', help='Generates files for a set of .java sources.')
  _add_library_args(subp)

  subp = subparsers.add_parser(
      'generate-final',
      help='Generates files that require knowledge of all intermediates.')
  _add_final_args(subp)

  # Default to showing full help text when no args are passed.
  if len(sys.argv) == 1:
    parser.print_help()
  elif len(sys.argv) == 2 and sys.argv[1] in subparsers.choices:
    parser.parse_args(sys.argv[1:] + ['-h'])
  else:
    args = parser.parse_args()
    args.func(parser, args)


if __name__ == '__main__':
  _maybe_relaunch_with_newer_python()
  main()
