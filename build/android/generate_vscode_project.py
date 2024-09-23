#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Given a .build_config.json file, generates an Eclipse JDT project that can
be used with the "Language Support for Java™ by Red Hat" Visual Studio Code
extension. See //docs/vscode.md for details.
"""

import argparse
import logging
import json
import os
import sys
import xml.etree.ElementTree

sys.path.append(os.path.join(os.path.dirname(__file__), 'gyp'))
from util import build_utils

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))
import gn_helpers


def _WithoutSuffix(string, suffix):
  if not string.endswith(suffix):
    raise ValueError(f'{string!r} does not end with {suffix!r}')
  return string[:-len(suffix)]


def _GetJavaRoot(path):
  # The authoritative way to determine the Java root for a given source file is
  # to parse the source code and extract the package and class names, but let's
  # keep things simple and use some heuristics to try to guess the Java root
  # from the file path instead.
  while True:
    dirname, basename = os.path.split(path)
    if not basename:
      raise RuntimeError(f'Unable to determine the Java root for {path!r}')
    if basename in ('java', 'src'):
      return path
    if basename in ('javax', 'org', 'com'):
      return dirname
    path = dirname


def _ProcessSourceFile(output_dir, source_file_path, source_dirs):
  source_file_path = os.path.normpath(os.path.join(output_dir,
                                                   source_file_path))
  java_root = _GetJavaRoot(source_file_path)
  logging.debug('Extracted java root `%s` from source file path `%s`',
                java_root, source_file_path)
  source_dirs.add(java_root)


def _ProcessSourcesFile(output_dir, sources_file_path, source_dirs):
  for source_file_path in build_utils.ReadSourcesList(
      os.path.join(output_dir, sources_file_path)):
    _ProcessSourceFile(output_dir, source_file_path, source_dirs)


def _ProcessBuildConfigFile(output_dir, build_config_path, source_dirs, libs,
                            already_processed_build_config_files,
                            android_sdk_build_tools_version):
  if build_config_path in already_processed_build_config_files:
    return
  already_processed_build_config_files.add(build_config_path)

  logging.info('Processing build config: %s', build_config_path)

  with open(os.path.join(output_dir, build_config_path)) as build_config_file:
    build_config = json.load(build_config_file)

  deps_info = build_config['deps_info']
  target_sources_file = deps_info.get('target_sources_file')
  if target_sources_file is not None:
    _ProcessSourcesFile(output_dir, target_sources_file, source_dirs)
  else:
    unprocessed_jar_path = deps_info.get('unprocessed_jar_path')
    if unprocessed_jar_path is not None:
      lib_path = os.path.normpath(os.path.join(output_dir,
                                               unprocessed_jar_path))
      logging.debug('Found lib `%s', lib_path)
      libs.add(lib_path)

  input_srcjars = os.path.join(output_dir,
    _WithoutSuffix(build_config_path, '.build_config.json'),
    'generated_java', 'input_srcjars')
  if os.path.exists(input_srcjars):
    source_dirs.add(input_srcjars)

  android = build_config.get('android')
  if android is not None:
    # This works around an issue where the language server complains about
    # `java.lang.invoke.LambdaMetafactory` not being found. The normal Android
    # build process is fine with this class being missing because d8 removes
    # references to LambdaMetafactory from the bytecode - see:
    #   https://jakewharton.com/androids-java-8-support/#native-lambdas
    # When JDT builds the code, d8 doesn't run, so the references are still
    # there. Fortunately, the Android SDK provides a convenience JAR to fill
    # that gap in:
    #   //third_party/android_sdk/public/build-tools/*/core-lambda-stubs.jar
    libs.add(
        os.path.normpath(
            os.path.join(
                output_dir,
                os.path.dirname(build_config['android']['sdk_jars'][0]),
                os.pardir, os.pardir, 'build-tools',
                android_sdk_build_tools_version, 'core-lambda-stubs.jar')))

  for dep_config in deps_info['deps_configs']:
    _ProcessBuildConfigFile(output_dir, dep_config, source_dirs, libs,
                            already_processed_build_config_files,
                            android_sdk_build_tools_version)


def _GenerateClasspathEntry(kind, path):
  classpathentry = xml.etree.ElementTree.Element('classpathentry')
  classpathentry.set('kind', kind)
  classpathentry.set('path', path)
  return classpathentry


def _GenerateProject(source_dirs, libs, output_dir):
  classpath = xml.etree.ElementTree.Element('classpath')
  for source_dir in sorted(source_dirs):
    classpath.append(_GenerateClasspathEntry('src', source_dir))
  for lib in sorted(libs):
    classpath.append(_GenerateClasspathEntry('lib', lib))
  classpath.append(
    _GenerateClasspathEntry('output', os.path.join(output_dir, 'jdt_output')))

  xml.etree.ElementTree.ElementTree(classpath).write(
    '.classpath', encoding='unicode')
  print('Generated .classpath', file=sys.stderr)

  with open('.project', 'w') as f:
    f.write("""<?xml version="1.0" encoding="UTF-8"?>
<projectDescription>
  <name>chromium</name>
  <buildSpec>
    <buildCommand>
      <name>org.eclipse.jdt.core.javabuilder</name>
      <arguments />
    </buildCommand>
  </buildSpec>
  <natures><nature>org.eclipse.jdt.core.javanature</nature></natures>
</projectDescription>
""")
  print('Generated .project', file=sys.stderr)

  # Tell the Eclipse compiler not to use java.lang.invoke.StringConcatFactory
  # in the generated bytecodes as the class is unavailable in Android.
  os.makedirs('.settings', exist_ok=True)
  with open('.settings/org.eclipse.jdt.core.prefs', 'w') as f:
    f.write("""eclipse.preferences.version=1
org.eclipse.jdt.core.compiler.codegen.useStringConcatFactory=disabled
""")
  print('Generated .settings', file=sys.stderr)


def _ParseArguments(argv):
  parser = argparse.ArgumentParser(
      description=
      'Given Chromium Java build config files, generates an Eclipse JDT '
      'project that can be used with the "Language Support for Java™ by '
      'Red Hat" Visual Studio Code extension. See //docs/vscode.md '
      'for details.')
  parser.add_argument(
      '--output-dir',
      required=True,
      help='Relative path to the output directory, e.g. "out/Debug"')
  parser.add_argument(
      '--build-config',
      action='append',
      required=True,
      help='Path to the .build_config.json file to use as input, relative to '
      '`--output-dir`. May be repeated.')
  return parser.parse_args(argv)


def main(argv):
  build_utils.InitLogging('GENERATE_VSCODE_CLASSPATH_DEBUG')

  assert os.path.exists('.gn'), 'This script must be run from the src directory'

  args = _ParseArguments(argv)
  output_dir = args.output_dir

  build_vars = gn_helpers.ReadBuildVars(output_dir)

  source_dirs = set()
  libs = set()
  already_processed_build_config_files = set()
  for build_config_path in args.build_config:
    _ProcessBuildConfigFile(output_dir, build_config_path, source_dirs, libs,
                            already_processed_build_config_files,
                            build_vars['android_sdk_build_tools_version'])

  logging.info('Done processing %d build config files',
               len(already_processed_build_config_files))

  _GenerateProject(source_dirs, libs, output_dir)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
