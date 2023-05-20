# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import subprocess
import sys
import tempfile


def fix_module_imports(header_path, output_path):
  """Convert modules import to work without -fmodules support.

  The Swift compiler assumes that the generated Objective-C header will be
  imported from code compiled with module support enabled (-fmodules). The
  generated code thus uses @import and provides no fallback if modules are
  not enabled.

  This function converts the generated header to instead use #import. It
  assumes that `@import Foo;` can be replaced by `#import <Foo/Foo.h>`.

  The header is read at `header_path` and written to `output_path`.
  """

  header_contents = []
  with open(header_path, 'r') as header_file:
    for line in header_file:
      if line == '#if __has_feature(objc_modules)\n':
        header_contents.append('#if 1  // #if __has_feature(objc_modules)\n')
        nesting_level = 1
        for line in header_file:
          if line == '#endif\n':
            nesting_level -= 1
          elif line.startswith('@import'):
            name = line.split()[1].split(';')[0]
            if name != 'ObjectiveC':
              header_contents.append(f'#import <{name}/{name}.h>  ')
            header_contents.append('// ')
          elif line.startswith('#if'):
            nesting_level += 1

          header_contents.append(line)
          if nesting_level == 0:
            break
      else:
        header_contents.append(line)

  with open(output_path, 'w') as header_file:
    for line in header_contents:
      header_file.write(line)


def compile_module(module, sources, settings, extras, tmpdir):
  """Compile `module` from `sources` using `settings`."""
  output_file_map = {}
  if settings.whole_module_optimization:
    output_file_map[''] = {
        'object': os.path.join(settings.object_dir, module + '.o'),
        'dependencies': os.path.join(tmpdir, module + '.d'),
    }
  else:
    for source in sources:
      name, _ = os.path.splitext(os.path.basename(source))
      output_file_map[source] = {
          'object': os.path.join(settings.object_dir, name + '.o'),
          'dependencies': os.path.join(tmpdir, name + '.d'),
      }

  for key in ('module_path', 'header_path', 'depfile'):
    path = getattr(settings, key)
    if os.path.exists(path):
      os.unlink(path)
    if key == 'module_path':
      for ext in '.swiftdoc', '.swiftsourceinfo':
        path = os.path.splitext(getattr(settings, key))[0] + ext
        if os.path.exists(path):
          os.unlink(path)
    directory = os.path.dirname(path)
    if not os.path.exists(directory):
      os.makedirs(directory)

  if not os.path.exists(settings.object_dir):
    os.makedirs(settings.object_dir)

  if not os.path.exists(settings.pch_output_dir):
    os.makedirs(settings.pch_output_dir)

  for key in output_file_map:
    path = output_file_map[key]['object']
    if os.path.exists(path):
      os.unlink(path)

  output_file_map.setdefault('', {})['swift-dependencies'] = \
      os.path.join(tmpdir, module + '.swift.d')

  output_file_map_path = os.path.join(tmpdir, module + '.json')
  with open(output_file_map_path, 'w') as output_file_map_file:
    output_file_map_file.write(json.dumps(output_file_map))
    output_file_map_file.flush()

  extra_args = []
  if settings.file_compilation_dir:
    extra_args.extend([
        '-file-compilation-dir',
        settings.file_compilation_dir,
    ])

  if settings.bridge_header:
    extra_args.extend([
        '-import-objc-header',
        os.path.abspath(settings.bridge_header),
    ])

  if settings.whole_module_optimization:
    extra_args.append('-whole-module-optimization')

  if settings.target:
    extra_args.extend([
        '-target',
        settings.target,
    ])

  if settings.sdk:
    extra_args.extend([
        '-sdk',
        os.path.abspath(settings.sdk),
    ])

  if settings.swift_version:
    extra_args.extend([
        '-swift-version',
        settings.swift_version,
    ])

  if settings.include_dirs:
    for include_dir in settings.include_dirs:
      extra_args.append('-I' + include_dir)

  if settings.system_include_dirs:
    for system_include_dir in settings.system_include_dirs:
      extra_args.extend(['-Xcc', '-isystem', '-Xcc', system_include_dir])

  if settings.framework_dirs:
    for framework_dir in settings.framework_dirs:
      extra_args.extend([
          '-F',
          framework_dir,
      ])

  if settings.system_framework_dirs:
    for system_framework_dir in settings.system_framework_dirs:
      extra_args.extend([
          '-F',
          system_framework_dir,
      ])

  if settings.enable_cxx_interop:
    extra_args.extend([
        '-Xfrontend',
        '-enable-cxx-interop',
    ])

  # The swiftc compiler uses a global module cache that is not robust against
  # changes in the sub-modules nor against corruption (see crbug.com/1358073).
  # Force the compiler to store the module cache in a sub-directory of `tmpdir`
  # to ensure a pristine module cache is used for every compiler invocation.
  module_cache_path = os.path.join(tmpdir, settings.swiftc_version,
                                   'ModuleCache')

  # If the generated header is post-processed, generate it to a temporary
  # location (to avoid having the file appear to suddenly change).
  if settings.fix_module_imports:
    header_path = os.path.join(tmpdir, f'{module}.h')
  else:
    header_path = settings.header_path

  process = subprocess.Popen([
      settings.swift_toolchain_path + '/usr/bin/swiftc',
      '-parse-as-library',
      '-module-name',
      module,
      '-module-cache-path',
      module_cache_path,
      '-emit-object',
      '-emit-dependencies',
      '-emit-module',
      '-emit-module-path',
      settings.module_path,
      '-emit-objc-header',
      '-emit-objc-header-path',
      header_path,
      '-output-file-map',
      output_file_map_path,
      '-pch-output-dir',
      os.path.abspath(settings.pch_output_dir),
  ] + extra_args + extras + sources)

  process.communicate()
  if process.returncode:
    sys.exit(process.returncode)

  if settings.fix_module_imports:
    fix_module_imports(header_path, settings.header_path)

  # The swiftc compiler generates depfile that uses absolute paths, but
  # ninja requires paths in depfiles to be identical to paths used in
  # the build.ninja files.
  #
  # Since gn generates paths relative to the build directory for all paths
  # below the repository checkout, we need to convert those to relative
  # paths.
  #
  # See https://crbug.com/1287114 for build failure that happen when the
  # paths in the depfile are kept absolute.
  out_dir = os.getcwd() + os.path.sep
  src_dir = os.path.abspath(settings.root_dir) + os.path.sep

  depfile_content = dict()
  for key in output_file_map:

    # When whole module optimisation is disabled, there will be an entry
    # with an empty string as the key and only ('swift-dependencies') as
    # keys in the value dictionary. This is expected, so skip entry that
    # do not include 'dependencies' in their keys.
    depencency_file_path = output_file_map[key].get('dependencies')
    if not depencency_file_path:
      continue

    for line in open(depencency_file_path):
      output, inputs = line.split(' : ', 2)
      _, ext = os.path.splitext(output)
      if ext == '.o':
        key = output
      else:
        key = os.path.splitext(settings.module_path)[0] + ext
      if key not in depfile_content:
        depfile_content[key] = set()
      for path in inputs.split():
        if path.startswith(src_dir) or path.startswith(out_dir):
          path = os.path.relpath(path, out_dir)
        depfile_content[key].add(path)

  with open(settings.depfile, 'w') as depfile:
    keys = sorted(depfile_content.keys())
    for key in sorted(keys):
      depfile.write('%s : %s\n' % (key, ' '.join(sorted(depfile_content[key]))))


def main(args):
  parser = argparse.ArgumentParser(add_help=False)
  parser.add_argument('-module-name', help='name of the Swift module')
  parser.add_argument('-include',
                      '-I',
                      action='append',
                      dest='include_dirs',
                      help='add directory to header search path')
  parser.add_argument('-isystem',
                      action='append',
                      dest='system_include_dirs',
                      help='add directory to system header search path')
  parser.add_argument('sources', nargs='+', help='Swift source file to compile')
  parser.add_argument('-whole-module-optimization',
                      action='store_true',
                      help='enable whole module optimization')
  parser.add_argument('-object-dir',
                      help='path to the generated object files directory')
  parser.add_argument('-pch-output-dir',
                      help='path to directory where .pch files are saved')
  parser.add_argument('-module-path', help='path to the generated module file')
  parser.add_argument('-header-path', help='path to the generated header file')
  parser.add_argument('-bridge-header',
                      help='path to the Objective-C bridge header')
  parser.add_argument('-depfile', help='path to the generated depfile')
  parser.add_argument('-swift-version',
                      help='version of Swift language to support')
  parser.add_argument('-target',
                      action='store',
                      help='generate code for the given target <triple>')
  parser.add_argument('-sdk', action='store', help='compile against sdk')
  parser.add_argument('-F',
                      dest='framework_dirs',
                      action='append',
                      help='add dir to framework search path')
  parser.add_argument('-Fsystem',
                      '-iframework',
                      dest='system_framework_dirs',
                      action='append',
                      help='add dir to system framework search path')
  parser.add_argument('-root-dir',
                      dest='root_dir',
                      action='store',
                      required=True,
                      help='path to the root of the repository')
  parser.add_argument('-swift-toolchain-path',
                      default='',
                      action='store',
                      dest='swift_toolchain_path',
                      help='path to the root of the Swift toolchain')
  parser.add_argument('-file-compilation-dir',
                      default='',
                      action='store',
                      help='compilation directory to embed in the debug info')
  parser.add_argument('-enable-cxx-interop',
                      dest='enable_cxx_interop',
                      action='store_true',
                      help='allow importing C++ modules into Swift')
  parser.add_argument('-fix-module-imports',
                      action='store_true',
                      help='enable hack to fix module imports')
  parser.add_argument('-swiftc-version',
                      default='',
                      action='store',
                      help='version of swiftc compiler')
  parser.add_argument('-xcode-version',
                      default='',
                      action='store',
                      help='version of xcode')

  parsed, extras = parser.parse_known_args(args)
  with tempfile.TemporaryDirectory() as tmpdir:
    compile_module(parsed.module_name, parsed.sources, parsed, extras, tmpdir)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
