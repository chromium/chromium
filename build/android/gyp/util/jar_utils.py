# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods to run tools over jars and cache their output."""

import logging
import pathlib
import subprocess
import zipfile
from typing import List, Optional, Union

_SRC_PATH = pathlib.Path(__file__).resolve().parents[4]
_JDEPS_PATH = _SRC_PATH / 'third_party/jdk/current/bin/jdeps'

_IGNORED_JAR_PATHS = [
    # This matches org_ow2_asm_asm_commons and org_ow2_asm_asm_analysis, both of
    # which fail jdeps (not sure why), see: https://crbug.com/348423879
    'third_party/android_deps/cipd/libs/org_ow2_asm_asm',
]

def _should_ignore(jar_path: pathlib.Path) -> bool:
  for ignored_jar_path in _IGNORED_JAR_PATHS:
    if ignored_jar_path in str(jar_path):
      return True
  return False


def run_jdeps(filepath: pathlib.Path,
              *,
              jdeps_path: pathlib.Path = _JDEPS_PATH,
              verbose: bool = False) -> Optional[str]:
  """Runs jdeps on the given filepath and returns the output."""
  if not filepath.exists() or _should_ignore(filepath):
    # Some __compile_java targets do not generate a .jar file, skipping these
    # does not affect correctness.
    return None

  cmd = [
      str(jdeps_path),
      '-verbose:class',
      '--multi-release',  # Some jars support multiple JDK releases.
      'base',
      str(filepath),
  ]

  if verbose:
    logging.debug('Starting %s', filepath)
  try:
    return subprocess.run(
        cmd,
        check=True,
        text=True,
        capture_output=True,
    ).stdout
  except subprocess.CalledProcessError as e:
    # Pack all the information into the error message since that is the last
    # thing visible in the output.
    raise RuntimeError(f'\nFilepath:\n{filepath}\ncmd:\n{" ".join(cmd)}\n'
                       f'stdout:\n{e.stdout}\nstderr:{e.stderr}\n') from e
  finally:
    if verbose:
      logging.debug('Finished %s', filepath)


def extract_full_class_names_from_jar(
    jar_path: Union[str, pathlib.Path]) -> List[str]:
  """Returns set of fully qualified class names in passed-in jar."""
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
  return sorted(out)


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
