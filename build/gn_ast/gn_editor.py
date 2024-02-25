#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import dataclasses
import functools
import json
import logging
import multiprocessing
import os
import pathlib
import subprocess
import sys
from typing import List, Optional, Set

import json_gn_editor
import utils

_SRC_PATH = pathlib.Path(__file__).resolve().parents[2]

_BUILD_ANDROID_PATH = _SRC_PATH / 'build/android'
if str(_BUILD_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_BUILD_ANDROID_PATH))
from pylib import constants

_BUILD_ANDROID_GYP_PATH = _SRC_PATH / 'build/android/gyp'
if str(_BUILD_ANDROID_GYP_PATH) not in sys.path:
    sys.path.append(str(_BUILD_ANDROID_GYP_PATH))

from util import build_utils

_GIT_IGNORE_STR = '(git ignored file) '

NO_VALID_GN_STR = 'No valid GN files found after filtering.'


@dataclasses.dataclass
class OperationResult:
    path: str
    git_ignored: bool = False
    dryrun: bool = False
    skipped: bool = False
    skip_reason: str = ''

    def __str__(self):
        msg = f'Skipped ' if self.skipped else 'Updated '
        dryrun = '[DRYRUN] ' if self.dryrun else ''
        ignore = _GIT_IGNORE_STR if self.git_ignored else ''
        skip = f' ({self.skip_reason})' if self.skipped else ''
        return f'{dryrun}{msg}{ignore}{self.path}{skip}'


def _add_deps(target: str, deps: List[str], root: pathlib.Path, path: str):
    with json_gn_editor.BuildFile(path, root) as build_file:
        build_file.add_deps(target, deps)


def _search_deps(name_query: Optional[str], path_query: Optional[str],
                 root: pathlib.Path, path: str):
    with json_gn_editor.BuildFile(path, root) as build_file:
        build_file.search_deps(name_query, path_query)


def _split_deps(existing_dep: str, new_deps: List[str], root: pathlib.Path,
                path: str, dryrun: bool) -> Optional[OperationResult]:
    with json_gn_editor.BuildFile(path, root, dryrun=dryrun) as build_file:
        if build_file.split_deps(existing_dep, new_deps):
            return OperationResult(path=os.path.relpath(path, start=root),
                                   git_ignored=utils.is_git_ignored(
                                       root, path),
                                   dryrun=dryrun)
    return None


def _remove_deps(
        *, deps: List[str], out_dir: str, root: pathlib.Path, path: str,
        dryrun: bool, targets: List[str], inline_mode: bool,
        target_name_filter: Optional[str]) -> Optional[OperationResult]:
    with json_gn_editor.BuildFile(path, root, dryrun=dryrun) as build_file:
        if build_file.remove_deps(deps, out_dir, targets, target_name_filter,
                                  inline_mode):
            return OperationResult(path=os.path.relpath(path, start=root),
                                   git_ignored=utils.is_git_ignored(
                                       root, path),
                                   dryrun=dryrun)
    return None


def _add(args: argparse.Namespace, build_filepaths: List[str],
         root: pathlib.Path):
    deps = args.deps
    target = args.target
    with multiprocessing.Pool() as pool:
        pool.map(
            functools.partial(_add_deps, target, deps, root),
            build_filepaths,
        )


def _search(args: argparse.Namespace, build_filepaths: List[str],
            root: pathlib.Path):
    name_query = args.name
    path_query = args.path
    if name_query:
        logging.info(f'Searching dep names using: {name_query}')
    if path_query:
        logging.info(f'Searching paths using: {path_query}')
    with multiprocessing.Pool() as pool:
        pool.map(
            functools.partial(_search_deps, name_query, path_query, root),
            build_filepaths,
        )


def _split(args: argparse.Namespace, build_filepaths: List[str],
           root: pathlib.Path) -> List[OperationResult]:
    num_total = len(build_filepaths)
    results = []
    with multiprocessing.Pool() as pool:
        tasks = {
            filepath: pool.apply_async(
                _split_deps,
                (args.existing, args.new, root, filepath, args.dryrun))
            for filepath in build_filepaths
        }
        for idx, filepath in enumerate(tasks.keys()):
            relpath = os.path.relpath(filepath, start=root)
            logging.info('[%d/%d] Checking %s', idx, num_total, relpath)
            operation_result = tasks[filepath].get()
            if operation_result:
                logging.info(operation_result)
                results.append(operation_result)
    return results


def _get_project_json_contents(out_dir: str) -> str:
    project_json_path = os.path.join(out_dir, 'project.json')
    with open(project_json_path) as f:
        return f.read()


def _calculate_targets_for_file(relpath: str, arg_extra_targets: List[str],
                                all_targets: Set[str]) -> Optional[List[str]]:
    if os.path.basename(relpath) != 'BUILD.gn':
        # Build all targets when we are dealing with build files that might be
        # imported by other build files (e.g. config.gni or other_name.gn).
        return []
    dirpath = os.path.dirname(relpath)
    file_extra_targets = []
    for full_target_name in all_targets:
        target_dir, short_target_name = full_target_name.split(':', 1)
        # __ is used for sub-targets in GN, only focus on top-level ones. Also
        # skip targets using other toolchains, e.g.
        # base:feature_list_buildflags(//build/toolchain/linux:clang_x64)
        if (target_dir == dirpath and '__' not in short_target_name
                and '(' not in short_target_name):
            file_extra_targets.append(full_target_name)
    targets = arg_extra_targets + file_extra_targets
    return targets or None


def _remove(args: argparse.Namespace, build_filepaths: List[str],
            root: pathlib.Path) -> List[OperationResult]:
    num_total = len(build_filepaths)

    if args.output_directory:
        constants.SetOutputDirectory(args.output_directory)
    constants.CheckOutputDirectory()
    out_dir: str = constants.GetOutDirectory()

    args_gn_path = os.path.join(out_dir, 'args.gn')
    if not os.path.exists(args_gn_path):
        raise Exception(f'No args.gn in out directory {out_dir}')
    with open(args_gn_path) as f:
        # Although the target may compile fine, bytecode checks are necessary
        # for correctness at runtime.
        assert 'android_static_analysis = "on"' in f.read(), (
            'Static analysis must be on to ensure correctness.')
        # TODO: Ensure that the build server is not running.

    logging.info(f'Running "gn gen" in output directory: {out_dir}')
    build_utils.CheckOutput(['gn', 'gen', '-C', out_dir, '--ide=json'])

    if args.all_java_deps:
        assert not args.dep, '--all-java-target does not support passing deps.'
        assert args.file, '--all-java-target requires passing --file.'
        logging.info(f'Finding java deps under {out_dir}.')
        all_java_deps = build_utils.CheckOutput([
            str(_SRC_PATH / 'build' / 'android' / 'list_java_targets.py'),
            '--gn-labels', '-C', out_dir
        ]).split('\n')
        logging.info(f'Found {len(all_java_deps)} java deps.')
        args.dep += all_java_deps
    else:
        assert args.dep, 'At least one explicit dep is required.'

    project_json_contents = _get_project_json_contents(out_dir)
    project_json = json.loads(project_json_contents)
    # The input file names have a // prefix. (e.g. //android_webview/BUILD.gn)
    known_build_files = set(
        name[2:] for name in project_json['build_settings']['gen_input_files'])
    # Remove the // prefix for target names so ninja can build them.
    known_target_names = set(name[2:]
                             for name in project_json['targets'].keys())

    unknown_targets = [
        t for t in args.extra_build_targets if t not in known_target_names
    ]
    assert not unknown_targets, f'Cannot build {unknown_targets} in {out_dir}.'

    logging.info('Building all targets in preparation for removing deps')
    # Avoid capturing stdout/stderr to see the progress of the full build.
    subprocess.run(['autoninja', '-C', out_dir], check=True)

    results = []
    for idx, filepath in enumerate(build_filepaths):
        # Since removal can take a long time, provide an easy way to resume the
        # command if something fails.
        try:
            # When resuming, the first build file is the one that is being
            # resumed. Avoid inline mode skipping it since it's already started
            # to be processed and the first dep may already have been removed.
            if args.resume_from and idx == 0 and args.inline_mode:
                logging.info(f'Resuming: skipping inline mode for {filepath}.')
                should_inline = False
            else:
                should_inline = args.inline_mode
            relpath = os.path.relpath(filepath, start=root)
            logging.info('[%d/%d] Checking %s', idx, num_total, relpath)
            if relpath not in known_build_files:
                operation_result = OperationResult(
                    path=relpath,
                    skipped=True,
                    skip_reason='Not in the list of known build files.')
            else:
                targets = _calculate_targets_for_file(relpath,
                                                      args.extra_build_targets,
                                                      known_target_names)
                if targets is None:
                    operation_result = OperationResult(
                        path=relpath,
                        skipped=True,
                        skip_reason='Could not find any valid targets.')
                else:
                    operation_result = _remove_deps(
                        deps=args.dep,
                        out_dir=out_dir,
                        root=root,
                        path=filepath,
                        dryrun=args.dryrun,
                        targets=targets,
                        inline_mode=should_inline,
                        target_name_filter=args.target_name_filter)
            if operation_result:
                logging.info(operation_result)
                results.append(operation_result)
        # Use blank except: to show this for KeyboardInterrupt as well.
        except:
            logging.error(
                f'Encountered error while processing {filepath}. Append the '
                'following args to resume from this file once the error is '
                f'fixed:\n\n--resume-from {filepath}\n')
            raise
    return results


def main():
    parser = argparse.ArgumentParser(
        prog='gn_editor', description='Add or remove deps programatically.')

    common_args_parser = argparse.ArgumentParser(add_help=False)
    common_args_parser.add_argument(
        '-n',
        '--dryrun',
        action='store_true',
        help='Show which files would be updated but avoid changing them.')
    common_args_parser.add_argument('-v',
                                    '--verbose',
                                    action='store_true',
                                    help='Used to print ninjalog.')
    common_args_parser.add_argument('-q',
                                    '--quiet',
                                    action='store_true',
                                    help='Used to print less logging.')
    common_args_parser.add_argument('--file',
                                    help='Run on a specific build file.')
    common_args_parser.add_argument(
        '--resume-from',
        help='Skip files before this build file path (debugging).')

    subparsers = parser.add_subparsers(
        required=True, help='Use subcommand -h to see full usage.')

    add_parser = subparsers.add_parser(
        'add',
        parents=[common_args_parser],
        help='Add one or more deps to a specific target (pass the path to the '
        'BUILD.gn via --file for faster results). The target **must** '
        'have a deps variable defined, even if it is an empty [].')
    add_parser.add_argument('--target', help='The name of the target.')
    add_parser.add_argument('--deps',
                            nargs='+',
                            help='The name(s) of the new dep(s).')
    add_parser.set_defaults(command=_add)

    search_parser = subparsers.add_parser(
        'search',
        parents=[common_args_parser],
        help='Search for strings in build files. Each query is a regex string.'
    )
    search_parser.add_argument('--name',
                               help='This is checked against dep names.')
    search_parser.add_argument(
        '--path', help='This checks the relative path of the build file.')
    search_parser.set_defaults(command=_search)

    split_parser = subparsers.add_parser(
        'split',
        parents=[common_args_parser],
        help='Split one or more deps from an existing dep.')
    split_parser.add_argument('existing', help='The dep to split from.')
    split_parser.add_argument('new',
                              nargs='+',
                              help='One of the new deps to be added.')
    split_parser.set_defaults(command=_split)

    remove_parser = subparsers.add_parser(
        'remove',
        parents=[common_args_parser],
        help='Remove one or more deps if the build still succeeds. Removing '
        'one dep at a time is recommended.')
    remove_parser.add_argument(
        'dep',
        nargs='*',
        help='One or more deps to be removed. Zero when other options are used.'
    )
    remove_parser.add_argument(
        '-C',
        '--output-directory',
        metavar='OUT',
        help='If outdir is not provided, will attempt to guess.')
    remove_parser.add_argument(
        '--target-name-filter',
        help='This will cause the script to only remove deps from targets that '
        'match the filter provided. The filter should be a valid python regex '
        'string and is used in a re.search on the full GN target names, e.g. '
        're.search(pattern, "//base:base_java").')
    remove_parser.add_argument(
        '--all-java-deps',
        action='store_true',
        help='This will attempt to remove all known java deps. This option '
        'requires no explicit deps to be passed.')
    remove_parser.add_argument(
        '--extra-build-targets',
        metavar='T',
        nargs='*',
        default=[],
        help='The set of extra targets to compile after each dep removal. This '
        'is in addition to file-based targets that are automatically added.')
    remove_parser.add_argument(
        '--inline-mode',
        action='store_true',
        help='Skip the build file if the first dep is not found and removed. '
        'This is especially useful when inlining deps so that a build file '
        'that does not contain the dep being inlined can be skipped. This '
        'mode assumes that the first dep is the one being inlined.')
    remove_parser.set_defaults(command=_remove)

    args = parser.parse_args()

    if args.quiet:
        level = logging.WARNING
    elif args.verbose:
        level = logging.DEBUG
    else:
        level = logging.INFO
    logging.basicConfig(
        level=level, format='%(levelname).1s %(relativeCreated)7d %(message)s')

    root = _SRC_PATH
    if args.file:
        build_filepaths = [os.path.relpath(args.file, root)]
    else:
        build_filepaths = []
        logging.info('Finding build files under %s', root)
        for dirpath, _, filenames in os.walk(root):
            for filename in filenames:
                filepath = os.path.join(dirpath, filename)
                if filename.endswith(('.gn', '.gni')):
                    build_filepaths.append(filepath)
        build_filepaths.sort()

    logging.info('Found %d build files.', len(build_filepaths))

    if args.resume_from:
        resume_idx = None
        for idx, path in enumerate(build_filepaths):
            if path.endswith(args.resume_from):
                resume_idx = idx
                break
        assert resume_idx is not None, f'Did not find {args.resume_from}.'
        logging.info('Skipping %d build files with --resume-from.', resume_idx)
        build_filepaths = build_filepaths[resume_idx:]

    filtered_build_filepaths = [
        p for p in build_filepaths if not utils.is_bad_gn_file(p, root)
    ]
    num_total = len(filtered_build_filepaths)
    if num_total == 0:
        logging.error(NO_VALID_GN_STR)
        sys.exit(1)
    logging.info('Running on %d valid build files.', num_total)

    operation_results: List[OperationResult] = args.command(
        args, filtered_build_filepaths, root)
    if operation_results is None:
        return
    ignored_operation_results = [r for r in operation_results if r.git_ignored]
    skipped_operation_results = [r for r in operation_results if r.skipped]
    num_ignored = len(ignored_operation_results)
    num_skipped = len(skipped_operation_results)
    num_updated = len(operation_results) - num_skipped
    print(f'Checked {num_total}, updated {num_updated} ({num_ignored} of '
          f'which are ignored by git under {root}), and skipped {num_skipped} '
          'build files.')
    if num_ignored:
        print(f'\nThe following {num_ignored} files were ignored by git and '
              'may need separate CLs in their respective repositories:')
        for result in ignored_operation_results:
            print('  ' + result.path)


if __name__ == '__main__':
    main()
