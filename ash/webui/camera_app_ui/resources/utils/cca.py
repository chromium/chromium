#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
import argparse
import functools
import glob
import json
import logging
import os
import re
import shlex
import subprocess
import sys
import tempfile
import time
import xml.sax


@functools.lru_cache(1)
def get_chromium_root():
    path = os.path.realpath('../../../../')
    assert os.path.basename(path) == 'src'
    return path


def shell_join(cmd):
    return ' '.join(shlex.quote(c) for c in cmd)


def run(args, cwd=None):
    logging.debug(f'$ {shell_join(args)}')
    subprocess.check_call(args, cwd=cwd)


def check_output(args, cwd=None):
    logging.debug(f'$ {shell_join(args)}')
    return subprocess.check_output(args, cwd=cwd, text=True)


def run_node(args):
    root = get_chromium_root()
    node = os.path.join(root, 'third_party/node/linux/node-linux-x64/bin/node')
    binary = os.path.join(root, 'third_party/node/node_modules', args[0])
    run([node, binary] + args[1:])


def build_preload_images_js(outdir):
    with open('images/images.gni') as f:
        in_app_images = ast.literal_eval(
            re.search(r'in_app_images\s*=\s*(\[.*?\])', f.read(),
                      re.DOTALL).group(1))
    with tempfile.NamedTemporaryFile('w') as f:
        f.writelines(
            os.path.abspath(f'images/{asset}') + '\n'
            for asset in in_app_images)
        f.flush()
        cmd = [
            'utils/gen_preload_images_js.py',
            '--images_list_file',
            f.name,
            '--output_file',
            os.path.join(outdir, 'preload_images.js'),
        ]
        subprocess.check_call(cmd)


CCA_OVERRIDE_PATH = '/etc/camera/cca'
CCA_OVERRIDE_FEATURE = 'CCALocalOverride'
CHROME_DEV_CONF_PATH = '/etc/chrome_dev.conf'


def local_override_enabled(device):
    chrome_dev_conf = check_output(
        ['ssh', device, '--', 'cat', CHROME_DEV_CONF_PATH])
    # This is a simple heuristic that is not 100% accurate, since this only
    # matches the feature name which can be in other irrevelant position in the
    # file. This should be fine though since this is only used for developers
    # and it's not expected to have the exact string match outside of
    # --enable-features added by this script.
    return CCA_OVERRIDE_FEATURE in chrome_dev_conf


def ensure_local_override_enabled(device, force):
    if local_override_enabled(device):
        return
    run([
        'ssh', device, '--',
        f'echo "--enable-features={CCA_OVERRIDE_FEATURE}"' +
        f' >> {CHROME_DEV_CONF_PATH}'
    ])
    if not force:
        prompt = input('Need to restart UI for deploy to take effect, ' +
                       'do it now? (y/N): ').lower()
        if prompt != 'y':
            print(
                'Not restarting UI. ' +
                '`restart ui` on DUT manually for the change to take effect.')
            return
    run(['ssh', device, '--', 'restart', 'ui'])


def get_tsc_paths(board):
    root_dir = get_chromium_root()
    target_gen_dir = os.path.join(root_dir, f'out_{board}/Release/gen')

    resources_dir = os.path.join(target_gen_dir, 'ui/webui/resources/tsc/*')

    lit_d_ts = os.path.join(
        root_dir, 'third_party/material_web_components/lit_exports.d.ts')

    return {
        '//resources/*': [os.path.relpath(resources_dir)],
        'chrome://resources/*': [os.path.relpath(resources_dir)],
        'chrome://resources/mwc/lit/index.js': [os.path.relpath(lit_d_ts)],
    }


def make_mojom_symlink(board):
    cca_root = os.getcwd()
    root_dir = get_chromium_root()
    target_gen_dir = os.path.join(root_dir, f'out_{board}/Release/gen')
    src_relative_dir = os.path.relpath(cca_root, root_dir)
    generated_mojom_dir = os.path.join(target_gen_dir, src_relative_dir,
                                       'mojom')
    target = os.path.join(cca_root, 'mojom')

    if os.path.islink(target):
        if os.readlink(target) != generated_mojom_dir:
            # There's a symlink here that's not pointing to the correct path.
            # This might happen when changing board. Remove the symlink and
            # recreate in this case.
            os.remove(target)
            os.symlink(generated_mojom_dir, target)
    elif os.path.exists(target):
        # Some other things are at the mojom path. cca.py won't work in
        # this case.
        raise Exception("resources/mojom exists but not a symlink."
                        " Please remove it and try again.")
    else:
        os.symlink(generated_mojom_dir, target)


def get_tsc_references(board):
    root_dir = get_chromium_root()
    target_gen_dir = os.path.join(root_dir, f'out_{board}/Release/gen')
    mwc_tsconfig_path = os.path.join(
        target_gen_dir,
        'third_party/material_web_components/tsconfig_library.json')

    return [{'path': os.path.relpath(mwc_tsconfig_path)}]


def generate_tsconfig(board):
    cca_root = os.getcwd()
    # TODO(pihsun): This needs to be in sync with BUILD.gn, have some heuristic
    # to get the dependency from there or from the generated tsconfig.json
    # instead?
    root_dir = get_chromium_root()
    common_definitions = os.path.join(root_dir, 'tools/typescript/definitions')

    target_gen_dir = os.path.join(root_dir, f'out_{board}/Release/gen')
    assert os.path.exists(target_gen_dir), (
        f"Failed to find the build output dir {target_gen_dir}."
        " Please check the board name and build Chrome once.")

    with open(os.path.join(cca_root, 'tsconfig_base.json')) as f:
        tsconfig = json.load(f)

    make_mojom_symlink(board)

    tsconfig['files'] = glob.glob('js/**/*.ts', recursive=True)
    tsconfig['files'].append(os.path.join(common_definitions, 'pending.d.ts'))
    tsconfig['compilerOptions']['rootDir'] = cca_root
    tsconfig['compilerOptions']['noEmit'] = True
    tsconfig['compilerOptions']['paths'] = get_tsc_paths(board)
    # TODO(b:269971867): Remove this once we have type definition for ffmpeg.js
    tsconfig['compilerOptions']['allowJs'] = True
    tsconfig['compilerOptions']['plugins'] = [{
        "name": "ts-lit-plugin",
        "strict": True
    }]
    tsconfig['references'] = get_tsc_references(board)

    with open(os.path.join(cca_root, 'tsconfig.json'), 'w') as f:
        json.dump(tsconfig, f)


# Use a fixed temporary output folder for deploy, so incremental compilation
# works and deploy is faster.
DEPLOY_OUTPUT_TEMP_DIR = '/tmp/cca-deploy-out'


def deploy(args):
    root_dir = get_chromium_root()
    cca_root = os.getcwd()

    os.makedirs(DEPLOY_OUTPUT_TEMP_DIR, exist_ok=True)
    js_out_dir = os.path.join(DEPLOY_OUTPUT_TEMP_DIR, 'js')

    generate_tsconfig(args.board)

    run_node([
        'typescript/bin/tsc',
        '--outDir',
        DEPLOY_OUTPUT_TEMP_DIR,
        '--noEmit',
        'false',
        # Makes compilation faster
        '--incremental',
        # For better debugging experience on DUT.
        '--inlineSourceMap',
        '--inlineSources',
        # Makes devtools show TypeScript source with better path
        '--sourceRoot',
        '/js/',
        # For easier developing / test cycle.
        '--noUnusedLocals',
        'false',
        '--noUnusedParameters',
        'false',
    ])

    build_preload_images_js(js_out_dir)

    deploy_new_tsc_files = [
        'rsync',
        '--recursive',
        '--inplace',
        '--delete',
        '--mkpath',
        '--exclude=tsconfig.tsbuildinfo',
        # rsync by default use source file permission masked by target file
        # system umask while transferring new files, and since workstation
        # defaults to have file not readable by others, this makes deployed
        # file not readable by Chrome.
        # Set --chmod=a+rX to rsync to fix this ('a' so it won't be affected by
        # local umask, +r for read and +X for executable bit on folder), and
        # set --perms so existing files that might have the wrong permission
        # will have their permission fixed.
        '--perms',
        '--chmod=a+rX',
        f'{js_out_dir}/',
        f'{args.device}:{CCA_OVERRIDE_PATH}/js/',
    ]
    run(deploy_new_tsc_files)

    for dir in ['css', 'images', 'views', 'sounds']:
        deploy_new_assets = [
            'rsync',
            '--recursive',
            '--inplace',
            '--delete',
            '--mkpath',
            '--perms',
            '--chmod=a+rX',
            f'{os.path.join(cca_root, dir)}/',
            f'{args.device}:{CCA_OVERRIDE_PATH}/{dir}/',
        ]
        run(deploy_new_assets)

    current_time = time.strftime('%F %T%z')
    run([
        'ssh',
        args.device,
        '--',
        'printf',
        '%s',
        shlex.quote(
            f'export const DEPLOYED_VERSION = "cca.py deploy {current_time}";'
        ),
        '>',
        f'{CCA_OVERRIDE_PATH}/js/deployed_version.js',
    ])

    ensure_local_override_enabled(args.device, args.force)


def test(args):
    assert 'CCAUI' not in args.device, (
        'The first argument should be <device> instead of a test name pattern.'
    )
    cmd = ['cros_run_test', '--device', args.device, '--tast'] + args.pattern
    run(cmd)


def lint(args):
    cmd = [
        'eslint/bin/eslint.js',
        'js',
        'eslint_plugin',
        '.eslintrc.js',
        '--resolve-plugins-relative-to',
        os.path.join(get_chromium_root(), 'third_party/node'),
    ]
    if args.fix:
        cmd.append('--fix')
    if args.eslintrc:
        cmd.extend(['--config', args.eslintrc])
    try:
        run_node(cmd)
    except subprocess.CalledProcessError as e:
        print('ESLint check failed, return code =', e.returncode)
    # TODO(pihsun): Add lit-analyzer to the check. It's not included in the
    # chrome source tree and can be manually installed with `npm install -g
    # lit-analyzer ts-lit-plugin`. Maybe this can be added as an optional check
    # for now?


def tsc(args):
    generate_tsconfig(args.board)

    try:
        run_node(['typescript/bin/tsc'])
    except subprocess.CalledProcessError as e:
        print('TypeScript check failed, return code =', e.returncode)


RESOURCES_H_PATH = '../resources.h'
I18N_STRING_TS_PATH = './js/i18n_string.ts'
CAMERA_STRINGS_GRD_PATH = './strings/camera_strings.grd'


def parse_resources_h():
    with open(RESOURCES_H_PATH, 'r') as f:
        content = f.read()
        return set(re.findall(r'\{"(\w+)",\s*(\w+)\}', content))


def parse_i18n_string_ts():
    with open(I18N_STRING_TS_PATH, 'r') as f:
        content = f.read()
        return dict(re.findall(r"(\w+) =\s*'(\w+)'", content))


# Same as tools/check_grd_for_unused_strings.py
class GrdIDExtractor(xml.sax.handler.ContentHandler):
    """Extracts the IDs from messages in GRIT files"""

    def __init__(self):
        self.id_set_ = set()

    def startElement(self, name, attrs):
        if name == 'message':
            self.id_set_.add(attrs['name'])

    def allIDs(self):
        """Return all the IDs found"""
        return self.id_set_.copy()


def parse_camera_strings_grd():
    handler = GrdIDExtractor()
    xml.sax.parse(CAMERA_STRINGS_GRD_PATH, handler)
    return handler.allIDs()


def check_strings(args):
    returncode = 0

    def check_name_id_consistent(strings, filename):
        nonlocal returncode
        bad = [(name, id) for (name, id) in strings
               if id != f'IDS_{name.upper()}']
        if bad:
            print(f'{filename} includes string id with inconsistent name:')
            for (name, id) in bad:
                print(f'    {name}: Expect IDS_{name.upper()}, got {id}')
            returncode = 1

    def check_all_ids_exist(all_ids, ids, filename):
        nonlocal returncode
        missing = all_ids.difference(ids)
        if missing:
            print(f'{filename} is missing the following string id:')
            print(f'    {", ".join(sorted(missing))}')
            returncode = 1

    def check_all_name_lower_case(names, filename):
        nonlocal returncode
        hasUpper = [name for name in names if not name.islower()]
        if hasUpper:
            print(f'{filename} includes string name with upper case:')
            for name in hasUpper:
                print(f'    Incorrect name: {name}')
            returncode = 1

    def check_unused(i18n_string_ts_dict: dict):
        nonlocal returncode
        cca_root = os.getcwd()
        name_set_from_html_files = set()
        id_set_from_ts_files = set()

        with open(os.path.join(cca_root, 'views/main.html')) as f:
            # Find all values of i18n-xxx attributes such as `i18n-text="name"`.
            name_set_from_html_files.update(
                re.findall(r"i18n-[\w-]+=\"(\w+)\"", f.read()))

        for dirpath, _dirnames, filenames in os.walk(
                os.path.join(cca_root, 'js')):
            for filename in filenames:
                if not filename.endswith('.ts'):
                    continue
                with open(os.path.join(dirpath, filename)) as f:
                    id_set_from_ts_files.update(
                        re.findall(r"I18nString\.(\w+)", f.read()))

        unused_ids = [
            id for (id, name) in i18n_string_ts_dict.items()
            if id not in id_set_from_ts_files
            and name not in name_set_from_html_files
        ]

        unused_ids = []
        for (id, name) in i18n_string_ts_dict.items():
            if id in id_set_from_ts_files or name in name_set_from_html_files:
                continue
            unused_ids.append(id)

        if len(unused_ids) > 0:
            print('The following strings are defined in i18n_string.ts but '
                  'unused. Please remove them:')
            for id in unused_ids:
                print(f'    {id}')
            returncode = 1

    resources_h_strings = parse_resources_h()
    check_name_id_consistent(resources_h_strings, RESOURCES_H_PATH)
    resources_h_ids = set([id for (name, id) in resources_h_strings])

    i18n_string_ts_dict = parse_i18n_string_ts()
    check_unused(i18n_string_ts_dict)

    i18n_string_ts_name_id_set = set([
        (name, f'IDS_{id}') for (id, name) in i18n_string_ts_dict.items()
    ])
    check_name_id_consistent(i18n_string_ts_name_id_set, I18N_STRING_TS_PATH)
    i18n_string_ts_ids = set([id for (name, id) in i18n_string_ts_name_id_set])

    resources_h_names = set([name for (name, id) in resources_h_strings])
    check_all_name_lower_case(resources_h_names, RESOURCES_H_PATH)

    i18n_string_ts_names = set(
        [name for (name, id) in i18n_string_ts_name_id_set])
    check_all_name_lower_case(i18n_string_ts_names, I18N_STRING_TS_PATH)

    camera_strings_grd_ids = parse_camera_strings_grd()

    all_ids = resources_h_ids.union(i18n_string_ts_ids, camera_strings_grd_ids)

    check_all_ids_exist(all_ids, resources_h_ids, RESOURCES_H_PATH)
    check_all_ids_exist(all_ids, i18n_string_ts_ids, I18N_STRING_TS_PATH)
    check_all_ids_exist(all_ids, camera_strings_grd_ids,
                        CAMERA_STRINGS_GRD_PATH)

    return returncode


def parse_args(args):
    parser = argparse.ArgumentParser(description='CCA developer tools.')
    parser.add_argument('--debug', action='store_true')
    subparsers = parser.add_subparsers()

    deploy_parser = subparsers.add_parser('deploy',
                                          help='deploy to device',
                                          description='''Deploy CCA to device.
            This script only works if there is no file added/deleted.
            And please build Chrome at least once before running the command.'''
                                          )
    deploy_parser.add_argument('board')
    deploy_parser.add_argument('device')
    deploy_parser.add_argument('--force',
                               help="Don't prompt for restarting Chrome.",
                               action='store_true')
    deploy_parser.set_defaults(func=deploy)

    test_parser = subparsers.add_parser('test',
                                        help='run tests',
                                        description='Run CCA tests on device.')
    test_parser.add_argument('device')
    test_parser.add_argument('pattern',
                             nargs='*',
                             default=['camera.CCAUI*'],
                             help='test patterns. (default: camera.CCAUI*)')
    test_parser.set_defaults(func=test)

    lint_parser = subparsers.add_parser(
        'lint',
        help='check code with eslint',
        description='Check coding styles with eslint.')
    lint_parser.add_argument('--fix', action='store_true')
    lint_parser.add_argument('--eslintrc', help='use alternative eslintrc')
    lint_parser.set_defaults(func=lint)

    tsc_parser = subparsers.add_parser('tsc',
                                       help='check code with tsc',
                                       description='''Check types with tsc.
            Please build Chrome at least once before running the command.''')
    tsc_parser.set_defaults(func=tsc)
    tsc_parser.add_argument('board')

    # TODO(pihsun): Add argument to automatically generate / fix the files to a
    # consistent state.
    check_strings_parser = subparsers.add_parser(
        'check-strings',
        help='check string resources',
        description='''Ensure files related to string resources are having the
            same strings. This includes resources.h,
            resources/strings/camera_strings.grd and
            resources/js/i18n_string.ts.''')
    check_strings_parser.set_defaults(func=check_strings)

    parser.set_defaults(func=lambda _args: parser.print_help())

    return parser.parse_args(args)


def main(args):
    cca_root = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    assert os.path.basename(cca_root) == 'resources'
    os.chdir(cca_root)

    args = parse_args(args)

    log_level = logging.DEBUG if args.debug else logging.INFO
    log_format = '%(asctime)s - %(levelname)s - %(funcName)s: %(message)s'
    logging.basicConfig(level=log_level, format=log_format)

    logging.debug(f'args = {args}')
    return args.func(args)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
