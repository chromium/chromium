#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The stable API endpoint for ChromiumIDE Java language support.

ChromiumIDE executes this script to query information and perform operations.
This script is not meant to be run manually.
"""

import argparse
import concurrent.futures
import dataclasses
import logging
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from typing import Iterator, List, Optional, Set, Tuple

_SRC_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))

sys.path.append(os.path.join(_SRC_ROOT, 'build'))
import gn_helpers

sys.path.append(os.path.join(_SRC_ROOT, 'build', 'android', 'gyp'))
from util import build_utils

_DEPOT_TOOLS_PATH = os.path.join(_SRC_ROOT, 'third_party', 'depot_tools')

# The API version of the script.
_API_VERSION = 1

# Matches with the package declaration in a Java source file.
_PACKAGE_PATTERN = re.compile(r'package\s+([A-Za-z0-9._]*)\s*;')


@dataclasses.dataclass(frozen=True)
class BuildInfo:
  """Defines the output schema of build-info subcommand."""
  # Directory paths containing Java source files, relative from the src
  # directory.
  source_paths: List[str]

  # JAR file paths, relative from the src directory.
  class_paths: List[str]

  def to_dict(self) -> dict:
    """Converts the BuildInfo object to a dictionary for JSON serialization."""
    return {'sourcePaths': self.source_paths, 'classPaths': self.class_paths}


def _gn_gen(output_dir: str) -> None:
  """Runs 'gn gen' to generate build files for the specified output directory.

  Args:
    output_dir: The path to the build output directory.
  """
  cmd = [os.path.join(_DEPOT_TOOLS_PATH, 'gn'), 'gen', output_dir]
  logging.info('Running: %s', shlex.join(cmd))
  subprocess.check_call(cmd, stdout=sys.stderr)


def _compile(output_dir: str, args: List[str]) -> None:
  """Compiles the specified targets using the build system.

  Args:
    output_dir: The path to the build output directory.
    args: A list of build targets or arguments to pass to the build command.
  """
  cmd = gn_helpers.CreateBuildCommand(output_dir) + args
  logging.info('Running: %s', shlex.join(cmd))
  subprocess.check_call(cmd, stdout=sys.stderr)


def _is_useful_source_jar(source_jar_path: str, output_dir: str) -> bool:
  """Determines if a source JAR is useful for IDE indexing.

  This function filters out certain types of source JARs that are not
  beneficial or could cause issues for IDEs.

  Args:
    source_jar_path: The path to the source JAR file.
    output_dir: The path to the build output directory.

  Returns:
    True if the source JAR should be included, False otherwise.
  """
  # JNI placeholder srcjars contain random stubs.
  if source_jar_path.endswith('_placeholder.srcjar'):
    return False

  # When building a java_library target (e.g. //chrome/android:chrome_java),
  # a srcjar containing relevant resource definitions are generated first (e.g.
  # gen/chrome/android/chrome_java__assetres.srcjar), and it's included in the
  # source path when building the java_library target. This is how R references
  # are resolved when *.java are compiled with javac.
  #
  # When multiple java libraries are linked into an APK (e.g.
  # //clank/java:chrome_apk), another srcjar containing all relevant resource
  # definitions is generated (e.g.
  # gen/clank/java/chrome_apk__compile_resources.srcjar). Note that
  # *__assetres.srcjar used on compiling java libraries are NOT linked to final
  # APKs. This is how R definitions looked up in the run time are linked to an
  # APK.
  #
  # Then let's talk about IDE's business. We cannot add *__assetres.srcjar to
  # the source path of the language server because many of those srcjars
  # contain identically named R classes (e.g. org.chromium.chrome.R) with
  # different sets of resource names. (Note that we don't explicitly exclude
  # them here, but actually they don't appear in *.params.json at all.)
  # Therefore we want to pick a single resource jar that covers all resource
  # definitions in the whole Chromium tree and add it to the source path. An
  # approximation used here is to pick the __compile_resources.srcjar for the
  # main browser binary. This is not perfect though, because there can be some
  # resources not linked into the main browser binary. Ideally we should
  # introduce a GN target producing a resource jar covering all resources
  # across the repository.
  if source_jar_path.endswith('__compile_resources.srcjar'):
    if os.path.exists(os.path.join(_SRC_ROOT, 'clank')):
      private_resources_jar_path = os.path.join(
          output_dir, 'gen/clank/java/chrome_apk__compile_resources.srcjar')
      return source_jar_path == private_resources_jar_path

    public_resources_jar_path = os.path.join(
        output_dir,
        'gen/chrome/android/chrome_public_apk__compile_resources.srcjar')
    return source_jar_path == public_resources_jar_path

  return True


def _find_source_root(source_file: str) -> Optional[str]:
  """Finds the root directory for a given source file based on its package.

  For example, if a file '/path/to/src/org/chromium/foo/Bar.java' declares
  'package org.chromium.foo;', this function will return '/path/to/src'.

  Args:
    source_file: The path to the Java source file.

  Returns:
    The path to the source root, or None if the package declaration cannot be
    found or parsed.
  """
  with open(source_file) as f:
    for line in f:
      if match := _PACKAGE_PATTERN.match(line):
        package_name = match.group(1)
        break
    else:
      return None

  depth = package_name.count('.') + 1
  source_root = source_file.rsplit('/', depth + 1)[0]
  return source_root


def _process_sources(source_files: List[str], output_dir: str,
                     source_path_set: Set[str]) -> None:
  """Processes a list of source files to find their source roots.

  Args:
    source_files: A list of source file paths, relative to the output directory.
    output_dir: The path to the build output directory.
    source_path_set: A set to which identified source root paths will be added.
  """
  processed_dir_set = set()
  for source_file in source_files:
    if not source_file.endswith('.java') or not source_file.startswith('../'):
      continue

    source_file = os.path.normpath(os.path.join(output_dir, source_file))
    source_dir = os.path.dirname(source_file)

    if source_dir in processed_dir_set:
      continue
    processed_dir_set.add(source_dir)

    if source_root := _find_source_root(source_file):
      source_path_set.add(source_root)


def _process_params(params_path: str, output_dir: str,
                    source_path_set: Set[str], class_path_set: Set[str],
                    source_jar_set: Set[str]) -> None:
  """Processes a .params.json file to extract build information.

  .params.json files are generated on `gn gen` and contain metadata about build
  targets, including sources, dependencies, and generated JARs.

  Args:
    params_path: The path to the .params.json file.
    output_dir: The path to the build output directory.
    source_path_set: A set to which identified source root paths will be added.
    class_path_set: A set to which identified classpath JAR paths will be added.
    source_jar_set: A set to which identified source JAR paths will be added.
  """
  with open(params_path) as f:
    params = json.load(f)

  if target_sources_file := params.get('target_sources_file'):
    # If the target is built from source files, add their source roots.
    _process_sources(
        build_utils.ReadSourcesList(
            os.path.join(output_dir, target_sources_file)), output_dir,
        source_path_set)
  elif unprocessed_jar_path := params.get('unprocessed_jar_path'):
    # If is_prebuilt is not set, we guess it from the jar path. The path is
    # relative to outDir, so it starts with ../ if it points to a prebuilt
    # jar in the chrome source tree.
    if params.get('is_prebuilt') or unprocessed_jar_path.startswith('../'):
      # This is a prebuilt jar file. Add it to the class path.
      class_path_set.add(
          os.path.normpath(os.path.join(output_dir, unprocessed_jar_path)))

  source_jar_relative_paths = params.get('bundled_srcjars', [])
  for source_jar_relative_path in source_jar_relative_paths:
    if not source_jar_relative_path.startswith('gen/'):
      continue
    source_jar_path = os.path.join(output_dir, source_jar_relative_path)
    if _is_useful_source_jar(source_jar_path, output_dir):
      source_jar_set.add(source_jar_path)


def _find_params(output_dir: str) -> Iterator[str]:
  """Finds all .params.json files within the output directory.

  It uses list_java_targets.py to enumerate .params.json files, correctly
  ignoring stale ones in the output directory.

  Args:
    output_dir: The path to the build output directory.

  Yields:
    The paths to the .params.json files.
  """
  output = subprocess.check_output(
      [
          os.path.join(_SRC_ROOT, 'build', 'android', 'list_java_targets.py'),
          '--output-directory=' + output_dir,
          '--print-params-paths',
      ],
      cwd=_SRC_ROOT,
      encoding='utf-8',
  )
  for line in output.splitlines():
    path = line.split(': ', 1)[1]
    yield os.path.relpath(path, _SRC_ROOT)


def _scan_params(output_dir: str) -> Tuple[List[str], List[str], List[str]]:
  """Scans the output directory for .params.json files and processes them.

  This function walks through the 'gen' subdirectory of the output directory
  to find all .params.json files and extracts source paths, class paths,
  and source JARs from them.

  Args:
    output_dir: The path to the build output directory.

  Returns:
    A tuple containing:
      - A sorted list of source root directory paths.
      - A sorted list of classpath JAR file paths.
      - A sorted list of source JAR file paths.
  """
  source_path_set: Set[str] = set()
  class_path_set: Set[str] = set()
  source_jar_set: Set[str] = set()

  for params_path in _find_params(output_dir):
    _process_params(params_path, output_dir, source_path_set, class_path_set,
                    source_jar_set)

  return sorted(source_path_set), sorted(class_path_set), sorted(source_jar_set)


def _extract_source_jar(source_jar: str) -> str:
  """Extracts a source JAR file to a directory.

  The extraction is skipped if the JAR has not been modified since the last
  extraction.

  Args:
    source_jar: The path to the source JAR file.

  Returns:
    The path to the directory where the JAR was extracted.
  """
  extract_dir = source_jar + '.extracted-for-vscode'

  # Compare timestamps to avoid extracting source jars on every startup.
  source_jar_mtime = os.stat(source_jar).st_mtime
  try:
    extract_dir_mtime = os.stat(extract_dir).st_mtime
  except OSError:
    extract_dir_mtime = 0
  if source_jar_mtime <= extract_dir_mtime:
    return extract_dir

  logging.info('Extracting %s', source_jar)

  os.makedirs(extract_dir, exist_ok=True)

  # Use `jar` command from the JDK for optimal performance. Python's zipfile is
  # not very fast, and suffer from GIL on parallelizing.
  subprocess.check_call(
      [
          os.path.join(_SRC_ROOT, 'third_party', 'jdk', 'current', 'bin',
                       'jar'),
          '-x',
          '-f',
          os.path.abspath(source_jar),
      ],
      cwd=extract_dir,
      stdout=sys.stderr,
  )

  # Remove org.jni_zero placeholders, if any.
  jni_zero_dir = os.path.join(extract_dir, 'org', 'jni_zero')
  if os.path.exists(jni_zero_dir):
    shutil.rmtree(jni_zero_dir)

  return extract_dir


def _extract_source_jars(source_jars: List[str], output_dir: str) -> List[str]:
  """Extracts a list of source JARs in parallel.

  Before extraction, it ensures that the source JARs themselves are up-to-date
  by attempting to build them.

  Args:
    source_jars: A list of paths to source JAR files.
    output_dir: The path to the build output directory.

  Returns:
    A sorted list of paths to the directories where the source JARs were
    extracted.
  """
  if not source_jars:
    return []

  source_jar_targets = [
      os.path.relpath(source_jar, output_dir) for source_jar in source_jars
  ]
  _compile(output_dir, source_jar_targets)

  # Parallelize extracting source JARs as it takes a significant amount of time
  # to process all JARs for Chromium serially.
  with concurrent.futures.ThreadPoolExecutor() as executor:
    new_source_dirs = executor.map(_extract_source_jar, source_jars)

  return sorted(new_source_dirs)


def _version_main(_options: argparse.Namespace) -> None:
  """Handles the 'version' subcommand. Prints the API version."""
  print(_API_VERSION)


def _build_info_main(options: argparse.Namespace) -> None:
  """Handles the 'build-info' subcommand.

  Gathers build information (source paths, class paths) and prints it
  as JSON.
  """
  _gn_gen(options.output_dir)
  source_roots, class_jars, source_jars = _scan_params(options.output_dir)
  source_roots.extend(_extract_source_jars(source_jars, options.output_dir))

  build_info = BuildInfo(
      source_paths=source_roots,
      class_paths=class_jars,
  )
  json.dump(build_info.to_dict(), sys.stdout, indent=2, sort_keys=True)


def _parse_arguments(args: List[str]) -> argparse.Namespace:
  """Parses command-line arguments for the script."""
  parser = argparse.ArgumentParser(description=__doc__)
  subparsers = parser.add_subparsers(dest='subcommand', required=True)

  version_parser = subparsers.add_parser('version', help='Prints version')
  version_parser.set_defaults(main_func=_version_main)

  build_info_parser = subparsers.add_parser(
      'build-info', help='Returns information needed to build Java files')
  build_info_parser.set_defaults(main_func=_build_info_main)
  build_info_parser.add_argument(
      '--output-dir',
      required=True,
      help='Relative path to the output directory, e.g. "out/Debug"')

  return parser.parse_args(args)


def main(args: List[str]) -> None:
  build_utils.InitLogging('CHROMIUMIDE_API_DEBUG')

  assert os.path.exists('.gn'), 'This script must be run from the src directory'

  options = _parse_arguments(args)
  options.main_func(options)


if __name__ == '__main__':
  main(sys.argv[1:])
