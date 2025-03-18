# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import json
import pathlib
import os
import subprocess
import zipfile
import re

_SRC_PATH = pathlib.Path(__file__).resolve().parents[2]
_FETCH_ALL_PATH = _SRC_PATH / 'third_party/android_deps/fetch_all.py'
_HASH_LENGTH = 15
_SKIP_FILES = ('OWNERS', 'cipd.yaml')

_DEFAULT_GENERATED_DISCLAIMER = '''\
// **IMPORTANT**: build.gradle is generated and any changes would be overridden
//                by the autoroller. Please update build.gradle.template
//                instead.
'''


def generate_version_map_str(bom_path, with_hash=False):
  """Generate groovy code to fill the versionCache map.

    Args:
      bom_path: Path to bill_of_materials.json to parse.
      with_hash: Whether to also return a hash of all the packages in the BoM.
    """
  bom = []
  version_map_lines = []
  bom_hash = hashlib.sha256()
  with open(bom_path) as f:
    bom = json.load(f)
  bom.sort(key=lambda x: (x['group'], x['name']))
  for dep in bom:
    group = dep['group']
    name = dep['name']
    version = dep['version']
    bom_hash.update(f'${group}:${name}:${version}'.encode())
    map_line = f"versionCache['{group}:{name}'] = '{version}'"
    version_map_lines.append(map_line)
  version_map_str = '\n'.join(sorted(version_map_lines))
  version_hash = bom_hash.hexdigest()[:_HASH_LENGTH]
  if with_hash:
    return version_map_str, version_hash
  return version_map_str


def fill_template(template_path, output_path, **kwargs):
  """Fills in a template.

    Args:
      template_path: Path to <file>.template.
      output_path: Path to <file>.
      **kwargs: each kwarg should be a string to replace in the template.
    """
  content = pathlib.Path(template_path).read_text()
  for key, value in kwargs.items():
    replace_string = '{{' + key + '}}'
    if not replace_string in content:
      raise Exception(f'Replace text {replace_string} '
                      f'not found in {template_path}')
    try:
      content = content.replace(replace_string, value)
    except Exception as e:
      raise e from Exception(
          f'Failed to replace {repr(replace_string)} with {repr(value)}')

  content = content.replace(r'{{generated_disclaimer}}',
                            _DEFAULT_GENERATED_DISCLAIMER)

  unreplaced_variable_re = re.compile(r'\{\{(.+)\}\}')
  if matches := unreplaced_variable_re.findall(content):
    unreplaced_variables = ', '.join(repr(match) for match in matches)
    raise Exception('Found unreplaced variables '
                    f'[{unreplaced_variables}] in {template_path}')

  pathlib.Path(output_path).write_text(content)


def write_cipd_yaml(package_root,
                    package_name,
                    version,
                    output_path,
                    experimental=False):
  """Writes cipd.yaml file at the passed-in path."""

  root_libs_dir = package_root / 'libs'
  lib_dirs = os.listdir(root_libs_dir)
  if not lib_dirs:
    raise Exception('No generated libraries in {}'.format(root_libs_dir))

  data_files = [
      'BUILD.gn',
      'VERSION.txt',
      'bill_of_materials.json',
      'additional_readme_paths.json',
      'build.gradle',
      'to_commit.zip',
  ]
  for lib_dir in lib_dirs:
    abs_lib_dir: pathlib.Path = root_libs_dir / lib_dir
    if not abs_lib_dir.is_dir():
      continue

    for lib_file in abs_lib_dir.iterdir():
      if lib_file.name in _SKIP_FILES:
        continue
      data_files.append((abs_lib_dir / lib_file).relative_to(package_root))

  if experimental:
    package_name = (f'experimental/google.com/{os.getlogin()}/{package_name}')
  contents = [
      '# Copyright 2025 The Chromium Authors',
      '# Use of this source code is governed by a BSD-style license that can be',
      '# found in the LICENSE file.',
      f'# version: {version}',
      f'package: {package_name}',
      f'description: CIPD package for {package_name}',
      'data:',
  ]
  contents.extend(f'- file: {str(f)}' for f in data_files)

  with open(output_path, 'w') as out:
    out.write('\n'.join(contents))


def create_to_commit_zip(output_path, package_root, dirnames,
                         absolute_file_map):
  """Generates a to_commit.zip from useful text files inside |package_root|.

    Args:
      output_path: where to output the zipfile.
      package_root: path to gradle/cipd package.
      dirnames: list of subdirs under |package_root| to walk.
      absolute_file_map: List of files to be stored under the absolute prefix
        CHROMIUM_SRC/.
  """
  to_commit_paths = []
  for directory in dirnames:
    for root, _, files in os.walk(package_root / directory):
      for filename in files:
        # Avoid committing actual artifacts.
        if filename.endswith(('.aar', '.jar')):
          continue
        # TODO(mheikal): stop outputting these from gradle since they are not
        # useful.
        if filename in _SKIP_FILES:
          continue
        file_path = pathlib.Path(root) / filename
        file_path_in_zip = file_path.relative_to(package_root)
        to_commit_paths.append((file_path, file_path_in_zip))

  for filename, path_in_repo in absolute_file_map.items():
    file_path = package_root / filename
    path_in_zip = f'CHROMIUM_SRC/{path_in_repo}'
    to_commit_paths.append((file_path, path_in_zip))

  with zipfile.ZipFile(output_path, 'w') as zip_file:
    for filename, arcname in to_commit_paths:
      zip_file.write(filename, arcname=arcname)


def run_fetch_all(android_deps_dir,
                  extra_args,
                  verbose_count=0,
                  output_subdir=None):
  fetch_all_cmd = [
      _FETCH_ALL_PATH, '--android-deps-dir', android_deps_dir,
      '--ignore-vulnerabilities'
  ] + ['-v'] * verbose_count
  if output_subdir:
    fetch_all_cmd += ['--output-subdir', output_subdir]

  # Filter out -- from the args to pass to fetch_all.py.
  fetch_all_cmd += [a for a in extra_args if a != '--']

  subprocess.run(fetch_all_cmd, check=True)
