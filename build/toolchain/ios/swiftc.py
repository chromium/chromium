# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import json
import os
import subprocess
import sys
import tempfile

class OrderedSet(collections.OrderedDict):
  def add(self, value):
    self[value] = True


def compile_module(module, sources, settings, extras, tmpdir):
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
  if settings.bridge_header:
    extra_args.extend([
        '-import-objc-header',
        os.path.abspath(settings.bridge_header),
    ])

  if settings.whole_module_optimization:
    # When building with whole module optimization enabled, swiftc has a hidden
    # requirements that pch for the bridging headers are saved between runs
    # (via `-pch-output-dir $dir`) or disabled (via `-disable-bridging-pch`).
    #
    # Otherwise, the frontend generates the pch of dependent modules and then
    # try to parse it as a source file. This manifests as weird errors when
    # module B depends on module A and module A has a bridging header.
    #
    # This is not documented but is tested by swiftc unit tests:
    # https://github.com/apple/swift/blob/main/test/Driver/bridging-pch.swift
    #
    # Behaviour was introduced by the following change:
    # https://github.com/apple/swift/pull/9509
    #
    # For the moment disable the use of pch for bridging headers (simpler).
    # A more future proof solution would be to build all Objective-C code as
    # modules which would allow not using bridging headers at all.
    extra_args.append('-whole-module-optimization')
    extra_args.append('-disable-bridging-pch')

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

  process = subprocess.Popen([
      'swiftc',
      '-parse-as-library',
      '-module-name',
      module,
      '-emit-object',
      '-emit-dependencies',
      '-emit-module',
      '-emit-module-path',
      settings.module_path,
      '-emit-objc-header',
      '-emit-objc-header-path',
      settings.header_path,
      '-output-file-map',
      output_file_map_path,
  ] + extra_args + extras + sources)

  process.communicate()
  if process.returncode:
    sys.exit(process.returncode)

  depfile_content = collections.OrderedDict()
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
        depfile_content[key] = OrderedSet()
      for path in inputs.split():
        depfile_content[key].add(path)

  with open(settings.depfile, 'w') as depfile:
    for key in depfile_content:
      if not settings.depfile_filter or key in settings.depfile_filter:
        inputs = depfile_content[key]
        depfile.write('%s : %s\n' % (key, ' '.join(inputs)))


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
  parser.add_argument('-module-path', help='path to the generated module file')
  parser.add_argument('-header-path', help='path to the generated header file')
  parser.add_argument('-bridge-header',
                      help='path to the Objective-C bridge header')
  parser.add_argument('-depfile', help='path to the generated depfile')
  parser.add_argument('-swift-version',
                      help='version of Swift language to support')
  parser.add_argument('-depfile-filter',
                      action='append',
                      help='limit depfile to those files')
  parser.add_argument('-target',
                      action='store',
                      help='generate code for the given target <triple>')
  parser.add_argument('-sdk', action='store', help='compile against sdk')
  parser.add_argument('-F',
                      dest='framework_dirs',
                      action='append',
                      help='add dir to framework search path')
  parser.add_argument('-Fsystem',
                      dest='system_framework_dirs',
                      action='append',
                      help='add dir to system framework search path')

  parsed, extras = parser.parse_known_args(args)
  with tempfile.TemporaryDirectory() as tmpdir:
    compile_module(parsed.module_name, parsed.sources, parsed, extras, tmpdir)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
