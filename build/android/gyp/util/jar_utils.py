# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods to run tools over jars and cache their output."""

import logging
import os
import pathlib
import zipfile
from typing import List, Optional, Set

from util import build_utils

_SRC_PATH = pathlib.Path(__file__).resolve().parents[4]
_JDEPS_PATH = _SRC_PATH / 'third_party/jdk/current/bin/jdeps'

_IGNORED_JAR_PATHS = [
    # This matches org_ow2_asm_asm_commons and org_ow2_asm_asm_analysis, both of
    # which fail jdeps (not sure why).
    'third_party/android_deps/libs/org_ow2_asm_asm',
]


def _should_ignore(jar_path: pathlib.Path) -> bool:
  for ignored_jar_path in _IGNORED_JAR_PATHS:
    if ignored_jar_path in str(jar_path):
      return True
  return False


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


def _calculate_cache_path(filepath: pathlib.Path, src_path: pathlib.Path,
                          build_output_dir: pathlib.Path) -> pathlib.Path:
  """Return a cache path for jdeps that is always in the output dir.

    Also ensures that the cache file's parent directories exist if the original
    file was not already in the output dir.

    Example:
    - Given:
      src_path = /cr/src
      build_output_dir = /cr/src/out/Debug
    - filepath = /cr/src/out/Debug/a/d/file.jar
      Returns: /cr/src/out/Debug/a/d/file.jdeps_cache
    - filepath = /cr/src/out/Debug/../../b/c/file.jar
      Returns: /cr/src/out/Debug/jdeps_cache/b/c/file.jdeps_cache
    """
  filepath = filepath.resolve(strict=True)
  if _is_relative_to(filepath, build_output_dir):
    return filepath.with_suffix('.jdeps_cache')
  assert src_path in filepath.parents, f'Jar file not under src: {filepath}'
  jdeps_cache_dir = build_output_dir / 'jdeps_cache'
  relpath = filepath.relative_to(src_path)
  cache_path = jdeps_cache_dir / relpath.with_suffix('.jdeps_cache')
  # The parent dirs may not exist since this path is re-parented from //src to
  # //src/out/Dir.
  cache_path.parent.mkdir(parents=True, exist_ok=True)
  return cache_path


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

  cache_path = _calculate_cache_path(filepath, src_path, build_output_dir)
  if (cache_path.exists()
      and cache_path.stat().st_mtime > filepath.stat().st_mtime):
    with cache_path.open() as f:
      return f.read()

  # Cache either doesn't exist or is older than the jar file.
  output = build_utils.CheckOutput([
      str(jdeps_path),
      '-verbose:class',
      '--multi-release',  # Some jars support multiple JDK releases.
      'base',
      str(filepath),
  ])
  logging.debug('Writing output to cache.')
  with cache_path.open('w') as f:
    f.write(output)
  return output


def _read_jar_namelist(abs_build_output_dir: pathlib.Path,
                       abs_jar_path: pathlib.Path) -> List[str]:
  """Returns list of jar members by name."""

  # Caching namelist speeds up lookup_dep.py runtime by 1.5s.
  cache_path = abs_jar_path.with_suffix(abs_jar_path.suffix + '.namelist_cache')
  if _is_relative_to(cache_path, abs_build_output_dir):
    # already in the outdir, no need to adjust cache path
    pass
  elif _is_relative_to(abs_jar_path, _SRC_PATH):
    cache_path = (abs_build_output_dir / 'gen' /
                  cache_path.relative_to(_SRC_PATH))
  else:
    cache_path = (abs_build_output_dir / 'gen' / 'abs' /
                  cache_path.relative_to(cache_path.anchor))

  if (cache_path.exists()
      and os.path.getmtime(cache_path) > os.path.getmtime(abs_jar_path)):
    with open(cache_path) as f:
      return [s.strip() for s in f.readlines()]

  with zipfile.ZipFile(abs_jar_path) as z:
    namelist = z.namelist()

  cache_path.parent.mkdir(parents=True, exist_ok=True)
  with open(cache_path, 'w') as f:
    f.write('\n'.join(namelist))

  return namelist


def extract_full_class_names_from_jar(abs_build_output_dir: pathlib.Path,
                                      abs_jar_path: pathlib.Path) -> Set[str]:
  """Returns set of fully qualified class names in passed-in jar."""
  out = set()
  jar_namelist = _read_jar_namelist(abs_build_output_dir, abs_jar_path)
  for zip_entry_name in jar_namelist:
    if not zip_entry_name.endswith('.class'):
      continue
    # Remove .class suffix
    full_java_class = zip_entry_name[:-6]

    full_java_class = full_java_class.replace('/', '.')
    dollar_index = full_java_class.find('$')
    if dollar_index >= 0:
      full_java_class = full_java_class[0:dollar_index]

    out.add(full_java_class)
  return out


def parse_full_java_class(source_path: pathlib.Path) -> str:
  """Guess the fully qualified class name from the path to the source file."""
  if source_path.suffix not in ('.java', '.kt'):
    logging.warning('"%s" does not end in .java or .kt.', source_path)
    return ''

  directory_path: pathlib.Path = source_path.parent
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
