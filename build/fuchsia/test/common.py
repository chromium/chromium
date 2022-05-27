# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common methods and variables used by Cr-Fuchsia testing infrastructure."""

import json
import logging
import os
import platform
import re
import subprocess

from argparse import ArgumentParser
from typing import List, Optional

DIR_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
REPO_ALIAS = 'chromium-test-package-server'
SDK_ROOT = os.path.join(DIR_SRC_ROOT, 'third_party', 'fuchsia-sdk', 'sdk')


def get_host_arch() -> str:
    """Retrieve CPU architecture of the host machine. """
    host_arch = platform.machine()
    # platform.machine() returns AMD64 on 64-bit Windows.
    if host_arch in ['x86_64', 'AMD64']:
        return 'x64'
    if host_arch == 'aarch64':
        return 'arm64'
    raise Exception('Unsupported host architecture: %s' % host_arch)


SDK_TOOLS_DIR = os.path.join(SDK_ROOT, 'tools', get_host_arch())


def _run_repair_command(output):
    """Scans |output| for a self-repair command to run and, if found, runs it.

    Returns:
      True if a repair command was found and ran successfully. False otherwise.
    """
    # Check for a string along the lines of:
    # "Run `ffx doctor --restart-daemon` for further diagnostics."
    match = re.search('`ffx ([^`]+)`', output)
    if not match or len(match.groups()) != 1:
        return False  # No repair command found.
    args = match.groups()[0].split()

    try:
        run_ffx_command(args, suppress_repair=True)
    except subprocess.CalledProcessError:
        return False  # Repair failed.
    return True  # Repair succeeded.


def run_ffx_command(cmd: List[str],
                    check=True,
                    suppress_repair=False,
                    **kwargs) -> subprocess.CompletedProcess:
    """Runs `ffx` with the given arguments, waiting for it to exit.

    If `ffx` exits with a non-zero exit code, the output is scanned for a
    recommended repair command (e.g., "Run `ffx doctor --restart-daemon` for
    further diagnostics."). If such a command is found, it is run and then the
    original command is retried. This behavior can be suppressed via the
    `suppress_repair` argument.

    Args:
        cmd: A sequence of arguments to ffx.
        check: If True, CalledProcessError is raised if ffx returns a non-zero
            exit code.
        suppress_repair: If True, do not attempt to find and run a repair
            command.
    Returns:
        A CompletedProcess instance
    Raises:
        CalledProcessError if |check| is true.
    """
    _ffx_tool = os.path.join(SDK_TOOLS_DIR, 'ffx')
    try:
        proc = subprocess.run([_ffx_tool] + cmd, check=check, **kwargs)
        if check and proc.returncode != 0:
            raise subprocess.CalledProcessError(proc.returncode, cmd,
                                                proc.stdout)
        return proc
    except subprocess.CalledProcessError as cpe:
        if suppress_repair or not _run_repair_command(cpe.output):
            raise
        repair_succeeded = True
        return run_ffx_command(cmd, suppress_repair=True, **kwargs)

    # If the original command failed but a repair command was found and
    # succeeded, try one more time with the original command.
    if repair_succeeded:
        return run_ffx_command(cmd, check, suppress_repair=True)


def run_ffx_target_command(cmd: List[str], target: Optional[str] = None
                           ) -> subprocess.CompletedProcess:
    """Runs an ffx target command, optionally specifying the target to use."""
    prefix = []
    if target:
        prefix.extend(['--target', target])
    return run_ffx_command(prefix + ['target'] + cmd)


def read_package_paths(out_dir: str, pkg_name: str) -> List[str]:
    """
    Returns:
        A list of the absolute path to all FAR files the package depends on.
    """
    with open(os.path.join(DIR_SRC_ROOT, out_dir, 'gen',
                           f'{pkg_name}.meta')) as meta_file:
        data = json.load(meta_file)
    packages = []
    for package in data['packages']:
        packages.append(os.path.join(DIR_SRC_ROOT, out_dir, package))
    return packages


def register_common_args(parser: ArgumentParser) -> None:
    """Register commonly used arguments."""
    common_args = parser.add_argument_group('common', 'common arguments')
    common_args.add_argument(
        '--out-dir',
        '-C',
        type=os.path.realpath,
        help=('Path to the directory in which build files are located. '
              'Defaults to current directory.'))


def resolve_packages(packages: List[str]) -> None:
    """Ensure that all |packages| are installed on a device."""
    for package in packages:
        # Try destroying the component to force an update.
        try:
            run_ffx_command(
                ['component', 'destroy', f'/core/ffx-laboratory:{package}'],
                check=False)
        except subprocess.CalledProcessError:
            logging.warning('Creating new component')

        run_ffx_command([
            'component', 'create', f'/core/ffx-laboratory:{package}',
            f'fuchsia-pkg://{REPO_ALIAS}/{package}#meta/{package}.cm'
        ])
        run_ffx_command(
            ['component', 'resolve', f'/core/ffx-laboratory:{package}'])
