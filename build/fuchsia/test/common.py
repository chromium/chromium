# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common methods and variables used by Cr-Fuchsia testing infrastructure."""

import json
import os
import platform
import re
import subprocess

from argparse import ArgumentParser
from typing import Iterable, List, Optional

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


def run_ffx_command(cmd: Iterable[str],
                    node_name: Optional[str] = None,
                    check: bool = True,
                    suppress_repair: bool = False,
                    **kwargs) -> subprocess.CompletedProcess:
    """Runs `ffx` with the given arguments, waiting for it to exit.

    If `ffx` exits with a non-zero exit code, the output is scanned for a
    recommended repair command (e.g., "Run `ffx doctor --restart-daemon` for
    further diagnostics."). If such a command is found, it is run and then the
    original command is retried. This behavior can be suppressed via the
    `suppress_repair` argument.

    Args:
        cmd: A sequence of arguments to ffx.
        node_name: Whether to execute the command for a specific target.
        check: If True, CalledProcessError is raised if ffx returns a non-zero
            exit code.
        suppress_repair: If True, do not attempt to find and run a repair
            command.
    Returns:
        A CompletedProcess instance
    Raises:
        CalledProcessError if |check| is true.
    """

    ffx_cmd = [os.path.join(SDK_TOOLS_DIR, 'ffx')]
    if node_name:
        ffx_cmd.extend(('--target', node_name))
    ffx_cmd.extend(cmd)
    try:
        return subprocess.run(ffx_cmd, check=check, encoding='utf-8', **kwargs)
    except subprocess.CalledProcessError as cpe:
        if suppress_repair or not _run_repair_command(cpe.output):
            raise

    # If the original command failed but a repair command was found and
    # succeeded, try one more time with the original command.
    return run_ffx_command(cmd, node_name, check, True, **kwargs)


def run_continuous_ffx_command(cmd: Iterable[str],
                               node_name: Optional[str] = None,
                               **kwargs) -> subprocess.Popen:
    """Runs an ffx command asynchronously."""
    ffx_cmd = [os.path.join(SDK_TOOLS_DIR, 'ffx')]
    if node_name:
        ffx_cmd.extend(('--target', node_name))
    ffx_cmd.extend(cmd)
    return subprocess.Popen(ffx_cmd, encoding='utf-8', **kwargs)


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


def get_component_uri(package: str) -> str:
    """Retrieve the uri for a package."""
    return f'fuchsia-pkg://{REPO_ALIAS}/{package}#meta/{package}.cm'


def resolve_packages(packages: List[str]) -> None:
    """Ensure that all |packages| are installed on a device."""
    for package in packages:
        # Try destroying the component to force an update.
        run_ffx_command(
            ['component', 'destroy', f'/core/ffx-laboratory:{package}'],
            check=False)

        run_ffx_command([
            'component', 'create', f'/core/ffx-laboratory:{package}',
            f'fuchsia-pkg://{REPO_ALIAS}/{package}#meta/{package}.cm'
        ])
        run_ffx_command(
            ['component', 'resolve', f'/core/ffx-laboratory:{package}'])
