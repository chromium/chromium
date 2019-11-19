# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a archive manifest used for Fuchsia package generation."""

import argparse
import elfinfo
import json
import os
import re
import subprocess
import sys
import tempfile


def MakePackagePath(file_path, roots):
  """Computes a path for |file_path| that is relative to one of the directory
  paths in |roots|.

  file_path: The file path to relativize.
  roots: A list of directory paths which may serve as a relative root
         for |file_path|.

  Examples:

  >>> MakePackagePath('/foo/bar.txt', ['/foo/'])
  'bar.txt'

  >>> MakePackagePath('/foo/dir/bar.txt', ['/foo/'])
  'dir/bar.txt'

  >>> MakePackagePath('/foo/out/Debug/bar.exe', ['/foo/', '/foo/out/Debug/'])
  'bar.exe'
  """

  # Prevents greedily matching against a shallow path when a deeper, better
  # matching path exists.
  roots.sort(key=len, reverse=True)

  for next_root in roots:
    if not next_root.endswith(os.sep):
      next_root += os.sep

    if file_path.startswith(next_root):
      relative_path = file_path[len(next_root):]

      return relative_path

  return file_path


def _GetStrippedPath(bin_path):
  """Finds the stripped version of the binary |bin_path| in the build
  output directory."""

  return bin_path.replace('lib.unstripped/', 'lib/').replace(
      'exe.unstripped/', '')


def _IsBinary(path):
  """Checks if the file at |path| is an ELF executable by inspecting its FourCC
  header."""

  with open(path, 'rb') as f:
    file_tag = f.read(4)
  return file_tag == '\x7fELF'


def _WriteBuildIdsTxt(binary_paths, ids_txt_path):
  """Writes an index text file that maps build IDs to the paths of unstripped
  binaries."""

  with open(ids_txt_path, 'w') as ids_file:
    for binary_path in binary_paths:
      # Paths to the unstripped executables listed in "ids.txt" are specified
      # as relative paths to that file.
      relative_path = os.path.relpath(
          os.path.abspath(binary_path),
          os.path.dirname(os.path.abspath(ids_txt_path)))

      info = elfinfo.get_elf_info(_GetStrippedPath(binary_path))
      ids_file.write(info.build_id + ' ' + relative_path + '\n')


def BuildManifest(args):
  binaries = []
  with open(args.manifest_path, 'w') as manifest, \
       open(args.depfile_path, 'w') as depfile:
    # Process the runtime deps file for file paths, recursively walking
    # directories as needed.
    # MakePackagePath() may relativize to either the source root or output
    # directory.
    # runtime_deps may contain duplicate paths, so use a set for
    # de-duplication.
    expanded_files = set()
    for next_path in open(args.runtime_deps_file, 'r'):
      next_path = next_path.strip()
      if os.path.isdir(next_path):
        for root, _, files in os.walk(next_path):
          for current_file in files:
            if current_file.startswith('.'):
              continue
            expanded_files.add(
                os.path.join(root, current_file))
      else:
        expanded_files.add(next_path)

    # Format and write out the manifest contents.
    gen_dir = os.path.normpath(os.path.join(args.out_dir, "gen"))
    app_found = False
    excluded_files_set = set(args.exclude_file)
    for current_file in expanded_files:
      if _IsBinary(current_file):
        binaries.append(current_file)
        current_file = _GetStrippedPath(current_file)

      in_package_path = MakePackagePath(current_file,
                                        [gen_dir, args.root_dir, args.out_dir])
      if in_package_path == args.app_filename:
        app_found = True

      if in_package_path in excluded_files_set:
        excluded_files_set.remove(in_package_path)
        continue

      manifest.write('%s=%s\n' % (in_package_path, current_file))

    if len(excluded_files_set) > 0:
      raise Exception('Some files were excluded with --exclude-file, but '
                      'not found in the deps list: %s' %
                          ', '.join(excluded_files_set));

    if not app_found:
      raise Exception('Could not locate executable inside runtime_deps.')

    # Write meta/package manifest file.
    with open(os.path.join(os.path.dirname(args.manifest_path), 'package'),
              'w') as package_json:
      json.dump({'version': '0', 'name': args.app_name}, package_json)
      manifest.write('meta/package=%s\n' %
                   os.path.relpath(package_json.name, args.out_dir))

    # Write component manifest file.
    cmx_file_path = os.path.join(os.path.dirname(args.manifest_path),
                                 args.app_name + '.cmx')
    with open(cmx_file_path, 'w') as component_manifest_file:
      component_manifest = json.load(open(args.manifest_input_path, 'r'))
      component_manifest.update({
          'program': { 'binary': args.app_filename },
      })
      json.dump(component_manifest, component_manifest_file)

      manifest.write('meta/%s=%s\n' %
                     (os.path.basename(component_manifest_file.name),
                      os.path.relpath(cmx_file_path, args.out_dir)))

    depfile.write(
        "%s: %s" % (os.path.relpath(args.manifest_path, args.out_dir),
                    " ".join([os.path.relpath(f, args.out_dir)
                              for f in expanded_files])))

    _WriteBuildIdsTxt(binaries, args.build_ids_file)

  return 0


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--root-dir', required=True, help='Build root directory')
  parser.add_argument('--out-dir', required=True, help='Build output directory')
  parser.add_argument('--app-name', required=True, help='Package name')
  parser.add_argument('--app-filename', required=True,
      help='Path to the main application binary relative to the output dir.')
  parser.add_argument('--manifest-input-path', required=True,
      help='Path to the manifest file relative to the output dir.')
  parser.add_argument('--runtime-deps-file', required=True,
      help='File with the list of runtime dependencies.')
  parser.add_argument('--depfile-path', required=True,
      help='Path to write GN deps file.')
  parser.add_argument('--exclude-file', action='append', default=[],
      help='Package-relative file path to exclude from the package.')
  parser.add_argument('--manifest-path', required=True,
                      help='Manifest output path.')
  parser.add_argument('--build-ids-file', required=True,
                      help='Debug symbol index path.')

  args = parser.parse_args()

  return BuildManifest(args)

if __name__ == '__main__':
  sys.exit(main())
