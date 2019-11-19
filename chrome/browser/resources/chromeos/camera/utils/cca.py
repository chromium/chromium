#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
from distutils import dir_util
import functools
import json
import logging
import os
import re
import shlex
import shutil
import subprocess
import sys
import time


@functools.lru_cache(1)
def GetChromiumRoot():
  path = os.path.realpath('../../../../../')
  assert os.path.basename(path) == 'src'
  return path


def ShellJoin(cmd):
  return ' '.join(shlex.quote(c) for c in cmd)


def Run(args):
  logging.debug(f'$ {ShellJoin(args)}')
  subprocess.check_call(args)


def BuildLocaleStrings():
  grit_cmd = [
      os.path.join(GetChromiumRoot(), 'tools/grit/grit.py'),
      '-i',
      'src/strings/camera_strings.grd',
      'build',
      '-o',
      'build/strings',
  ]
  Run(grit_cmd)


def BuildMojomBindings(mojom_bindings):
  pylib = os.path.join(GetChromiumRoot(), 'mojo/public/tools/bindings/pylib')
  sys.path.insert(0, pylib)
  # pylint: disable=import-error,import-outside-toplevel
  from mojom.parse.parser import Parse

  mojom_paths = set()

  def AddPath(mojom):
    path = os.path.join(GetChromiumRoot(), mojom)
    if path in mojom_paths:
      return
    mojom_paths.add(path)

    with open(path) as f:
      code = f.read()
      ast = Parse(code, path)
      for imp in ast.import_list:
        AddPath(imp.import_filename)

  for binding in mojom_bindings:
    mojom = re.sub('-lite.js$', '', binding)
    AddPath(mojom)

  # It's needed for generating mojo_bindings_lite.js.
  AddPath('mojo/public/interfaces/bindings/interface_control_messages.mojom')

  if not os.path.exists('build/mojo'):
    os.makedirs('build/mojo')

  generator = os.path.join(
      GetChromiumRoot(),
      'mojo/public/tools/bindings/mojom_bindings_generator.py')

  precompile_cmd = [
      generator,
      '--use_bundled_pylibs',
      'precompile',
      '-o',
      'build/mojo',
  ]
  Run(precompile_cmd)

  parse_cmd = [
      generator,
      '--use_bundled_pylibs',
      'parse',
      '-d',
      GetChromiumRoot(),
      '-o',
      'build/mojo',
  ] + list(mojom_paths)
  Run(parse_cmd)

  generate_cmd = [
      generator,
      '--use_bundled_pylibs',
      'generate',
      '-d',
      GetChromiumRoot(),
      '-I',
      GetChromiumRoot(),
      '--bytecode_path',
      'build/mojo',
      '-o',
      'build/mojo',
      '-g',
      'javascript',
      '--js_bindings_mode',
      'new',
  ] + list(mojom_paths)
  Run(generate_cmd)

  generate_binding_lite_cmd = [
      os.path.join(GetChromiumRoot(),
                   ('mojo/public/tools/bindings/' +
                    'concatenate_and_replace_closure_exports.py')),
      os.path.join(GetChromiumRoot(), 'mojo/public/js/bindings_lite.js'),
      os.path.join(GetChromiumRoot(), 'mojo/public/js/interface_support.js'),
      ('build/mojo/mojo/public/interfaces/' +
       'bindings/interface_control_messages.mojom-lite.js'),
      'build/mojo/mojo_bindings_lite.js',
  ]
  Run(generate_binding_lite_cmd)


def BuildCCA(overlay=None, key=None):
  with open('BUILD.gn') as f:
    mojom_bindings = re.findall(r'root_gen_dir/(.*mojom-lite\.js)', f.read())
  mojo_files = ['mojo_bindings_lite.js'] + mojom_bindings

  # TODO(shik): Check mtime and rebuild them if the source is updated.
  if not os.path.exists('build/strings'):
    BuildLocaleStrings()
  if any(not os.path.exists(os.path.join('build/mojo', f)) for f in mojo_files):
    BuildMojomBindings(mojom_bindings)

  shutil.rmtree('build/camera', ignore_errors=True)
  dir_util.copy_tree('src', 'build/camera')
  for f in mojo_files:
    shutil.copy2(os.path.join('build/mojo', f), 'build/camera/js/mojo')
  dir_util.copy_tree('build/strings', 'build/camera')

  if overlay == 'dev':
    dir_util.copy_tree('utils/dev', 'build/camera')

    git_cmd = ['git', 'rev-parse', 'HEAD']
    commit_hash = subprocess.check_output(git_cmd, text=True).strip()[:8]
    timestamp = time.strftime("%F %T")

    with open('src/manifest.json') as f:
      manifest = json.load(f)
    manifest['version_name'] = f'Dev {commit_hash} @ {timestamp}'
    if key is not None:
      manifest['key'] = key
    with open('build/camera/manifest.json', 'w') as f:
      json.dump(manifest, f, indent=2)


def Deploy(args):
  BuildCCA()
  cmd = [
      'rsync',
      '--archive',
      '--checksum',
      '--chown=chronos:chronos',
      '--omit-dir-times',
      '--perms',
      '--verbose',
      'build/camera/',
      f'{args.device}:/opt/google/chrome/resources/chromeos/camera',
  ]
  Run(cmd)


def Test(args):
  assert 'CCAUI' not in args.device, (
      'The first argument should be <device> instead of a test name pattern.')
  tast_cmd = ['local_test_runner'] + args.pattern
  cmd = [
      'ssh',
      args.device,
      ShellJoin(tast_cmd),
  ]
  Run(cmd)


def Pack(args):
  assert os.path.exists(args.key), f'There is no key at {args.key}'

  pubkey = None
  if shutil.which('openssl'):
    openssl_cmd = ['openssl', 'rsa', '-in', args.key, '-pubout']
    openssl_output = subprocess.check_output(
        openssl_cmd, stderr=subprocess.DEVNULL, text=True)
    pubkey = ''.join(openssl_output.splitlines()[1:-1])
  BuildCCA(overlay='dev', key=pubkey)

  if os.path.exists('build/camera.crx'):
    os.remove('build/camera.crx')
  pack_cmd = [
      'google-chrome',
      '--disable-gpu',  # suppress an error about sandbox on gpu process
      '--pack-extension=build/camera',
      shlex.quote(f'--pack-extension-key={args.key}'),
  ]
  if shutil.which('xvfb-run'):
    # Run it in a virtual X server environment
    pack_cmd.insert(0, 'xvfb-run')
  Run(pack_cmd)
  assert os.path.exists('build/camera.crx')
  shutil.move('build/camera.crx', args.output)

  # TODO(shik): Add an option to deploy/install the packed crx on device


def ParseArgs(args):
  parser = argparse.ArgumentParser(description='CCA developer tools.')
  parser.add_argument('--debug', action='store_true')
  subparsers = parser.add_subparsers()

  deploy_parser = subparsers.add_parser(
      'deploy', help='deploy to device', description='Deploy CCA to device.')
  deploy_parser.add_argument('device')
  deploy_parser.set_defaults(func=Deploy)

  test_parser = subparsers.add_parser(
      'test', help='run tests', description='Run CCA tests on device.')
  test_parser.add_argument('device')
  test_parser.add_argument(
      'pattern',
      nargs='*',
      default=['camera.CCAUI*'],
      help='test patterns. (default: camera.CCAUI*)')
  test_parser.set_defaults(func=Test)

  pack_parser = subparsers.add_parser(
      'pack', help='pack crx', description='Pack CCA into a crx.')
  pack_parser.add_argument(
      '-o',
      '--output',
      help='output file (default: build/camera.crx)',
      default='build/camera.crx')
  pack_parser.add_argument(
      '-k',
      '--key',
      help='private key file (default: camera.pem)',
      default='camera.pem')
  pack_parser.set_defaults(func=Pack)

  parser.set_defaults(func=lambda _args: parser.print_help())

  return parser.parse_args(args)


def Main(args):
  cca_root = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
  assert os.path.basename(cca_root) == 'camera'
  os.chdir(cca_root)

  args = ParseArgs(args)

  log_level = logging.DEBUG if args.debug else logging.INFO
  log_format = '%(asctime)s - %(levelname)s - %(funcName)s: %(message)s'
  logging.basicConfig(level=log_level, format=log_format)

  logging.debug(f'args = {args}')
  return args.func(args)


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
