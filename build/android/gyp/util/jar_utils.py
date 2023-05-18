# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods to run tools over jars and cache their output."""

import dataclasses
import functools
import logging
import pathlib
import zipfile
from typing import List, Optional

from util import build_utils

_SRC_PATH = pathlib.Path(__file__).resolve().parents[4]
_JDEPS_PATH = _SRC_PATH / 'third_party/jdk/current/bin/jdeps'

_IGNORED_JAR_PATHS = [
    # This matches org_ow2_asm_asm_commons and org_ow2_asm_asm_analysis, both of
    # which fail jdeps (not sure why).
    'third_party/android_deps/libs/org_ow2_asm_asm',
]


def _is_relative_to(path: pathlib.Path, other_path: pathlib.Path):
  """This replicates pathlib.Path.is_relative_to.

    Since bots still run python3.8, they do not have access to is_relative_to,
    which was introduced in python3.9.
    """
  try:
    path.relative_to(other_path)
    return True
  except ValueError:
    # This error is expected when path is not a subpath of other_path.
    return False


@dataclasses.dataclass
class CacheFile:
  jar_path: pathlib.Path
  cache_suffix: str
  build_output_dir: pathlib.Path
  src_dir: pathlib.Path = _SRC_PATH

  def __post_init__(self):
    # Ensure that all paths are absolute so that relative_to works correctly.
    self.jar_path = self.jar_path.resolve()
    self.build_output_dir = self.build_output_dir.resolve()
    self.src_dir = self.src_dir.resolve()

  @functools.cached_property
  def cache_path(self):
    """Return a cache path for the jar that is always in the output dir.

      Example:
      - Given:
        src_path = /cr/src
        build_output_dir = /cr/src/out/Debug
        cache_suffix = .jdeps
      - filepath = /cr/src/out/Debug/a/d/file.jar
        Returns: /cr/src/out/Debug/a/d/file.jar.jdeps
      - filepath = /cr/src/out/b/c/file.jar
        Returns: /cr/src/out/Debug/gen/b/c/file.jar.jdeps
      - filepath = /random/path/file.jar
        Returns: /cr/src/out/Debug/gen/abs/random/path/file.jar.jdeps
      """
    path = self.jar_path.with_suffix(self.jar_path.suffix + self.cache_suffix)
    if _is_relative_to(path, self.build_output_dir):
      # already in the outdir, no need to adjust cache path
      return path
    if _is_relative_to(self.jar_path, _SRC_PATH):
      return self.build_output_dir / 'gen' / path.relative_to(_SRC_PATH)
    return self.build_output_dir / 'gen/abs' / path.relative_to(path.anchor)

  def is_valid(self):
    return (self.cache_path.exists() and self.jar_path.exists()
            and self.cache_path.stat().st_mtime > self.jar_path.stat().st_mtime)

  def read(self):
    with open(self.cache_path) as f:
      return f.read()

  def write(self, content: str):
    # If the jar file is in //src but not in the output dir or outside //src
    # then the reparented dirs within the output dir need to be created first.
    self.cache_path.parent.mkdir(parents=True, exist_ok=True)
    with open(self.cache_path, 'w') as f:
      f.write(content)


def _should_ignore(jar_path: pathlib.Path) -> bool:
  for ignored_jar_path in _IGNORED_JAR_PATHS:
    if ignored_jar_path in str(jar_path):
      return True
  return False


def run_jdeps(filepath: pathlib.Path,
              *,
              build_output_dir: pathlib.Path,
              jdeps_path: pathlib.Path = _JDEPS_PATH,
              src_path: pathlib.Path = _SRC_PATH) -> Optional[str]:
  """Runs jdeps on the given filepath and returns the output.

    Uses a simple file cache for the output of jdeps. If the jar file's mtime is
    older than the jdeps cache then just use the cached content instead.
    Otherwise jdeps is run again and the output used to update the file cache.

    Tested Nov 2nd, 2022:
    - With all cache hits, script takes 13 seconds.
    - Without the cache, script takes 1 minute 14 seconds.
    """
  # Some __compile_java targets do not generate a .jar file, skipping these
  # does not affect correctness.
  if not filepath.exists() or _should_ignore(filepath):
    return None

  cache_file = CacheFile(jar_path=filepath,
                         cache_suffix='.jdeps_cache',
                         build_output_dir=build_output_dir,
                         src_dir=src_path)
  if cache_file.is_valid():
    return cache_file.read()

  # Cache either doesn't exist or is older than the jar file.
  output = build_utils.CheckOutput([
      str(jdeps_path),
      '-verbose:class',
      '--multi-release',  # Some jars support multiple JDK releases.
      'base',
      str(filepath),
  ])

  cache_file.write(output)
  return output


def extract_full_class_names_from_jar(build_output_dir: pathlib.Path,
                                      jar_path: pathlib.Path) -> List[str]:
  """Returns set of fully qualified class names in passed-in jar."""

  cache_file = CacheFile(jar_path=jar_path,
                         cache_suffix='.class_name_cache',
                         build_output_dir=build_output_dir)
  if cache_file.is_valid():
    return cache_file.read().splitlines()

  out = set()
  with zipfile.ZipFile(jar_path) as z:
    for zip_entry_name in z.namelist():
      if not zip_entry_name.endswith('.class'):
        continue
      # Remove .class suffix
      full_java_class = zip_entry_name[:-6]

      # Remove inner class names after the first $.
      full_java_class = full_java_class.replace('/', '.')
      dollar_index = full_java_class.find('$')
      if dollar_index >= 0:
        full_java_class = full_java_class[0:dollar_index]

      out.add(full_java_class)
  out = sorted(out)

  cache_file.write('\n'.join(out))
  return out


def parse_full_java_class(source_path: pathlib.Path) -> str:
  """Guess the fully qualified class name from the path to the source file."""
  if source_path.suffix not in ('.java', '.kt'):
    logging.warning('"%s" does not end in .java or .kt.', source_path)
    return ''

  directory_path = source_path.parent
  package_list_reversed = []
  for part in reversed(directory_path.parts):
    if part == 'java':
      break
    package_list_reversed.append(part)
    if part in ('com', 'org'):
      break
  else:
    logging.debug(
        'File %s not in a subdir of "org" or "com", cannot detect '
        'package heuristically.', source_path)
    return ''

  package = '.'.join(reversed(package_list_reversed))
  class_name = source_path.stem
  return f'{package}.{class_name}'
