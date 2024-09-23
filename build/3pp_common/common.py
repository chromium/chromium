# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import pathlib
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.request

import scripthash

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _THIS_DIR.parents[1]
_CHECKOUT_SRC_ROOT_SUBDIR = '.3pp/chromium'


def parse_args():
    parser = argparse.ArgumentParser()
    # TODO(agrieve): Add required=True once 3pp builds with > python3.6.
    subparsers = parser.add_subparsers()

    subparser = subparsers.add_parser(
        'latest', help='Prints the version as $LATEST.$RUNTIME_DEPS_HASH')
    subparser.set_defaults(action='latest')

    subparser = subparsers.add_parser(
        'checkout', help='Copies files into the workdir used by docker')
    subparser.add_argument('checkout_dir')
    subparser.add_argument('--version', help='Output from "latest"')
    subparser.set_defaults(action='checkout')

    subparser = subparsers.add_parser(
        'install',
        help=('Run from workdir, inside docker container. '
              'Builds & copies outputs into |output_prefix| directory'))
    subparser.add_argument('output_prefix',
                           help='The path to install the compiled package to.')
    subparser.add_argument('deps_prefix',
                           help='The path to a directory containing all deps.')
    subparser.add_argument('--version', help='Output from "latest"')
    subparser.add_argument('--checkout-dir', help='Directory to use as CWD')
    subparser.set_defaults(action='install')

    subparser = subparsers.add_parser(
        'local-test', help='Run latest / checkout / install locally')
    subparser.add_argument('--checkout-dir',
                           default='3pp_workdir',
                           help='Workdir to use')
    subparser.add_argument('--output-prefix',
                           default='3pp_out',
                           help='Directory for final artifacts')
    subparser.set_defaults(action='local-test')

    args = parser.parse_args()
    if not hasattr(args, 'action'):
        parser.print_help()
        sys.exit(1)

    if hasattr(args, 'version'):
        args.version = args.version or os.environ.get('_3PP_VERSION')
        if not args.version:
            parser.error('Must set --version or _3PP_VERSION')
    if hasattr(args, 'output_prefix') and args.output_prefix:
        args.output_prefix = os.path.abspath(args.output_prefix)
    if hasattr(args, 'checkout_dir') and args.checkout_dir:
        args.checkout_dir = os.path.abspath(args.checkout_dir)

    if args.action == 'checkout':
        # 3pp bot recipe does this, so needed only when running locally.
        os.makedirs(args.checkout_dir, exist_ok=True)

    if args.action == 'install':
        if args.checkout_dir:
            logging.info('Setting CWD=%s', args.checkout_dir)
            os.chdir(args.checkout_dir)

        if not os.path.exists(_CHECKOUT_SRC_ROOT_SUBDIR):
            parser.error(f'Does not exist: {_CHECKOUT_SRC_ROOT_SUBDIR}.'
                         f' Use --checkout-dir?')

        # 3pp bot recipe does this, so needed only when running locally.
        os.makedirs(args.output_prefix, exist_ok=True)

    return args


def path_within_checkout(subpath):
    return os.path.abspath(os.path.join(_CHECKOUT_SRC_ROOT_SUBDIR, subpath))


def _all_files(path):
    if os.path.isfile(path):
        return [path]
    assert os.path.isdir(path), 'Not a file or dir: ' + path
    all_paths = pathlib.Path(path).glob('**/*')
    return [str(f) for f in all_paths if f.is_file()]


def _resolve_runtime_deps(runtime_deps):
    ret = []
    for p in runtime_deps:
        if p.startswith('//'):
            ret.append(os.path.relpath(str(_SRC_ROOT / p[2:])))
        elif os.path.isabs(p):
            ret.append(os.path.relpath(p))
        else:
            ret.append(p)
    return ret


def copy_runtime_deps(checkout_dir, runtime_deps):
    # Make 3pp_common scripts available in the docker container install.py
    # will run in.
    dest_dir = os.path.join(checkout_dir, _CHECKOUT_SRC_ROOT_SUBDIR)

    for src_path in _resolve_runtime_deps(runtime_deps):
        relpath = os.path.relpath(src_path, _SRC_ROOT)
        dest_path = os.path.join(dest_dir, relpath)
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        if os.path.isfile(src_path):
            shutil.copy(src_path, dest_path)
        else:
            shutil.copytree(src_path,
                            dest_path,
                            ignore=shutil.ignore_patterns('.*', '__pycache__'))
    logging.info('Runtime deps:')
    sys.stderr.write('\n'.join(_all_files(checkout_dir)) + '\n')


def download_file(url, dest):
    logging.info('Downloading %s', url)
    with urllib.request.urlopen(url) as r:
        with open(dest, 'wb') as f:
            shutil.copyfileobj(r, f)


def extract_tar(path, dest):
    logging.info('Extracting %s to %s', path, dest)
    with tarfile.open(path) as f:
        f.extractall(dest)


def run_cmd(cmd, check=True, *args, **kwargs):
    logging.info('Running: %s', shlex.join(cmd))
    return subprocess.run(cmd, check=check, *args, **kwargs)


def apply_patches(patches_dir, checkout_dir):
    for path in sorted(pathlib.Path(patches_dir).glob('*.patch')):
        cmd = ['git', 'apply', '-v', str(path)]
        run_cmd(cmd, cwd=checkout_dir)


def main(*, do_latest, do_install, runtime_deps):
    logging.basicConfig(
        level=logging.DEBUG,
        format='%(levelname).1s %(relativeCreated)6d %(message)s')
    args = parse_args()
    runtime_deps = [str(_THIS_DIR)] + runtime_deps

    if args.action == 'local-test':
        logging.warning('Will use work dir: %s', args.checkout_dir)
        logging.warning('Will use output dir: %s', args.output_prefix)
        if os.path.exists(args.checkout_dir) and os.listdir(args.checkout_dir):
            logging.warning(
                '*** Work dir not empty. This often causes failures. ***')
            time.sleep(4)
        # Approximates what 3pp recipe does for minimal configs.
        # https://source.chromium.org/search?q=symbol:Chromium3ppApi.execute&ss=chromium
        prog = os.path.abspath(sys.argv[0])
        cmd = [prog, 'latest']
        version = run_cmd(cmd, stdout=subprocess.PIPE, text=True).stdout
        os.environ['_3PP_VERSION'] = version
        checkout_dir = args.checkout_dir
        run_cmd([prog, 'checkout', checkout_dir])
        run_cmd([prog, 'install', args.output_prefix, 'UNUSED-DEPS-DIR'],
                cwd=checkout_dir)
        logging.warning('Local test complete.')
        return

    if args.action == 'latest':
        version = do_latest()
        assert version, 'do_latest() returned ' + repr(version)
        extra_paths = []
        for p in _resolve_runtime_deps(runtime_deps):
            extra_paths += _all_files(p)
        deps_hash = scripthash.compute(extra_paths=extra_paths)
        print(f'{version}.{deps_hash}')
        return

    # Remove the hash at the end: 30.4.0-alpha05.HASH => 30.4.0-alpha05
    args.version = args.version.rsplit('.', 1)[0]
    if args.action == 'checkout':
        copy_runtime_deps(args.checkout_dir, runtime_deps)
        return

    assert args.action == 'install'
    do_install(args)
    prefix_len = len(args.output_prefix) + 1
    logging.info(
        'Contents of %s: \n%s\n', args.output_prefix,
        '\n'.join(p[prefix_len:] for p in _all_files(args.output_prefix)))
