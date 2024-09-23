#!/usr/bin/env python3
#
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import shutil
import sys
import time

import compile_java

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.


def _RunCompiler(args,
                 kotlinc_cmd,
                 source_files,
                 jar_path,
                 intermediates_out_dir=None):
  """Runs the Kotlin compiler."""
  logging.info('Starting _RunCompiler')

  source_files = source_files.copy()
  kt_files = [f for f in source_files if f.endswith('.kt')]
  assert len(kt_files) > 0, 'At least one .kt file must be passed in.'

  java_srcjars = args.java_srcjars

  # Use jar_path's directory to ensure paths are relative (needed for rbe).
  temp_dir = jar_path + '.staging'
  build_utils.DeleteDirectory(temp_dir)
  os.makedirs(temp_dir)
  try:
    classes_dir = os.path.join(temp_dir, 'classes')
    os.makedirs(classes_dir)

    input_srcjars_dir = os.path.join(intermediates_out_dir or temp_dir,
                                     'input_srcjars')

    if java_srcjars:
      logging.info('Extracting srcjars to %s', input_srcjars_dir)
      build_utils.MakeDirectory(input_srcjars_dir)
      for srcjar in args.java_srcjars:
        source_files += build_utils.ExtractAll(srcjar,
                                               no_clobber=True,
                                               path=input_srcjars_dir,
                                               pattern='*.java')
      logging.info('Done extracting srcjars')

    # Don't include the output directory in the initial set of args since it
    # being in a temp dir makes it unstable (breaks md5 stamping).
    cmd = list(kotlinc_cmd)
    cmd += ['-d', classes_dir]

    if args.classpath:
      cmd += ['-classpath', ':'.join(args.classpath)]

    # This a kotlinc plugin to generate header files for .kt files, similar to
    # turbine for .java files.
    jvm_abi_path = os.path.join(build_utils.KOTLIN_HOME, 'lib',
                                'jvm-abi-gen.jar')
    cmd += [
        f'-Xplugin={jvm_abi_path}', '-P',
        'plugin:org.jetbrains.kotlin.jvm.abi:outputDir=' +
        args.interface_jar_path
    ]

    # Pass source paths as response files to avoid extremely long command
    # lines that are tedius to debug.
    source_files_rsp_path = os.path.join(temp_dir, 'files_list.txt')
    with open(source_files_rsp_path, 'w') as f:
      f.write(' '.join(source_files))
    cmd += ['@' + source_files_rsp_path]

    # Explicitly set JAVA_HOME since some bots do not have this already set.
    env = os.environ.copy()
    env['JAVA_HOME'] = build_utils.JAVA_HOME

    logging.debug('Build command %s', cmd)
    start = time.time()
    build_utils.CheckOutput(cmd,
                            env=env,
                            print_stdout=args.chromium_code,
                            fail_on_output=args.warnings_as_errors)
    logging.info('Kotlin compilation took %ss', time.time() - start)

    compile_java.CreateJarFile(jar_path, classes_dir)

    logging.info('Completed all steps in _RunCompiler')
  finally:
    shutil.rmtree(temp_dir)


def _ParseOptions(argv):
  parser = argparse.ArgumentParser()
  action_helpers.add_depfile_arg(parser)

  parser.add_argument('--java-srcjars',
                      action='append',
                      default=[],
                      help='List of srcjars to include in compilation.')
  parser.add_argument(
      '--generated-dir',
      help='Subdirectory within target_gen_dir to place extracted srcjars and '
      'annotation processor output for codesearch to find.')
  parser.add_argument('--classpath', action='append', help='Classpath to use.')
  parser.add_argument(
      '--chromium-code',
      action='store_true',
      help='Whether code being compiled should be built with stricter '
      'warnings for chromium code.')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  parser.add_argument('--jar-path', help='Jar output path.', required=True)
  parser.add_argument('--interface-jar-path',
                      help='Interface jar output path.',
                      required=True)

  args, extra_args = parser.parse_known_args(argv)

  args.classpath = action_helpers.parse_gn_list(args.classpath)
  args.java_srcjars = action_helpers.parse_gn_list(args.java_srcjars)

  source_files = []
  for arg in extra_args:
    # Interpret a path prefixed with @ as a file containing a list of sources.
    if arg.startswith('@'):
      source_files.extend(build_utils.ReadSourcesList(arg[1:]))
    else:
      assert not arg.startswith('--'), f'Undefined option {arg}'
      source_files.append(arg)

  return args, source_files


def main(argv):
  build_utils.InitLogging('KOTLINC_DEBUG')
  argv = build_utils.ExpandFileArgs(argv)
  args, source_files = _ParseOptions(argv)

  kotlinc_cmd = [build_utils.KOTLINC_PATH]

  kotlinc_cmd += [
      '-no-jdk',  # Avoid depending on the bundled JDK.
      # Avoid depending on the bundled Kotlin stdlib. This may have a version
      # skew with the one in //third_party/android_deps (which is the one we
      # prefer to use).
      '-no-stdlib',
      # Avoid depending on the bundled Kotlin reflect libs.
      '-no-reflect',
      # We typically set a default of 1G for java commands, see
      # build_utils.JavaCmd. This may help prevent OOMs.
      '-J-Xmx1G',
  ]

  if args.generated_dir:
    # Delete any stale files in the generated directory. The purpose of
    # args.generated_dir is for codesearch.
    shutil.rmtree(args.generated_dir, True)

  _RunCompiler(args,
               kotlinc_cmd,
               source_files,
               args.jar_path,
               intermediates_out_dir=args.generated_dir)

  if args.depfile:
    # GN already knows of the source files, so avoid listing individual files
    # in the depfile.
    action_helpers.write_depfile(args.depfile, args.jar_path, args.classpath)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
