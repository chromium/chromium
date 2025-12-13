#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""MCP server for providing information relevant to Chromium builds."""

import enum
import glob
import os
import platform
import re
import sys

# vpython-provided modules
# pylint: disable=import-error
from mcp.server import fastmcp
# pylint: enable=import-error

# pylint: disable=wrong-import-position
sys.path.insert(0,
                os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import gn_helpers
# pylint: enable=wrong-import-position

CHROMIUM_ROOT = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..'))

GN_ARGS_FILE = 'args.gn'

TARGET_OS_REGEX = re.compile(r'^\s*target_os\s*=\s*\"(\w*)\"\s*$')
TARGET_CPU_REGEX = re.compile(r'^\s*target_cpu\s*=\s*\"((?:\w|\d)*)\"\s*$')
IMPORT_REGEX = re.compile(r'^\s*import\(\"//([^\"]+)\"\)$')


class Architecture(enum.Enum):
    UNKNOWN = 0
    INTEL = 1
    ARM = 2


class Bitness(enum.Enum):
    UNKNOWN = 0
    THIRTY_TWO = 1
    SIXTY_FOUR = 2


class ValidOs(enum.StrEnum):
    UNKNOWN = 'unknown'
    LINUX = 'linux'
    MAC = 'mac'
    WIN = 'win'


class ValidArch(enum.StrEnum):
    UNKNOWN = 'unknown'
    ARM = 'arm'
    ARM64 = 'arm64'
    X86 = 'x86'
    X64 = 'x64'


mcp = fastmcp.FastMCP(name="Chromium Build Information")


# This tool can be moved to a resource once Gemini CLI supports them.
@mcp.tool()
def get_host_os() -> str:
    """Retrieves the operating system used by the current host. The return
    value is directly comparable with the target_os GN arg, except in the case
    of 'unknown'."""
    current_platform = sys.platform
    if current_platform in ('linux', 'cygwin'):
        return ValidOs.LINUX
    if current_platform == 'win32':
        return ValidOs.WIN
    if current_platform == 'darwin':
        return ValidOs.MAC
    return ValidOs.UNKNOWN


# This tool can be moved to a resource once Gemini CLI supports them.
@mcp.tool()
def get_host_arch() -> str:
    """Retrieves the CPU architecture used by the current host. The return
    value is directly comparable with the target_cpu GN arg, except in the
    case of 'unknown'."""
    arch = _get_host_architecture()
    bits = _get_host_bits()

    match (arch, bits):
        case (Architecture.INTEL, Bitness.THIRTY_TWO):
            return ValidArch.X86
        case (Architecture.INTEL, Bitness.SIXTY_FOUR):
            return ValidArch.X64
        case (Architecture.ARM, Bitness.THIRTY_TWO):
            return ValidArch.ARM
        case (Architecture.ARM, Bitness.SIXTY_FOUR):
            return ValidArch.ARM64
        case _:
            return ValidArch.UNKNOWN


def _get_host_architecture() -> Architecture:
    """Helper to retrieve the primary CPU architecture for the host.

    Does not include any bitness information.

    Returns:
        An Architecture enum value corresponding to the found architecture.
    """
    native_arm = platform.machine().lower() in ('arm', 'arm64')
    # This is necessary for the case of running x86 Python on arm devices via
    # an emulator. In this case, platform.machine() will show up as an x86
    # processor.
    emulated_x86 = 'armv8' in platform.processor().lower()
    if native_arm or emulated_x86:
        return Architecture.ARM

    native_x86 = platform.machine().lower() in ('x86', 'x86_64', 'amd64')
    if native_x86:
        return Architecture.INTEL

    return Architecture.UNKNOWN


def _get_host_bits() -> Bitness:
    """Helper to retrieve the CPU bitness for the host.

    Returns:
        A Bitness enum value corresponding to the found bitness.
    """
    # Per the Python documentation for platform.architecture(), the most
    # reliable to get the bitness of the Python interpreter is to check
    # sys.maxsize.
    is_64bits = sys.maxsize > 2**32
    if is_64bits:
        return Bitness.SIXTY_FOUR
    return Bitness.THIRTY_TWO


# This tool can be moved to a resource once Gemini CLI supports them.
@mcp.tool()
def get_all_build_directories() -> list[str]:
    """Retrieves a list of all valid build/output directories within the repo.
    Returned paths are relative to the chromium/src root directory."""
    return _get_standard_build_directories() + _get_cros_build_directories()


# This tool can be moved to a resource once Gemini CLI supports them.
@mcp.tool()
def get_valid_build_directories_for_config(target_os: str,
                                           target_cpu: str) -> list[str]:
    """Retrieves a list of all valid build/output directories within the repo
    that can be used to compile for the provided operatying system and
    architecture. Returned paths are relative to the chromium/src root
    directory."""
    valid_directories = []
    for d in get_all_build_directories():
        abspath = os.path.join(CHROMIUM_ROOT, d)
        if _directory_builds_for_config(abspath, target_os, target_cpu):
            valid_directories.append(d)
    return valid_directories


# This tool can be moved to a resource once Gemini CLI supports them.
@mcp.tool()
def get_valid_build_directories_for_current_host() -> list[str]:
    """Retrieves a list of all valid build/output directories within the repo
    that can be used to compile for the current host. This is equivalent to
    running the get_valid_build_directories_for_config tool with the host's
    information. Returned paths are relative to the chromium/src root
    directory."""
    host_os = get_host_os()
    if host_os == ValidOs.UNKNOWN:
        return []
    host_arch = get_host_arch()
    if host_arch == ValidArch.UNKNOWN:
        return []
    return get_valid_build_directories_for_config(host_os, host_arch)


def _get_standard_build_directories() -> list[str]:
    """Gets all valid output directories under out/.

    Returns:
        A list of strings, each element containing a relative path from the
        Chromium root directory to a valid output directory.
    """
    return _get_build_directories_under_dir('out')


def _get_cros_build_directories() -> list[str]:
    """Gets all valid CrOS output directories.

    By convention, CrOS builds in out_<board name> directories instead of in
    the out/ directory.

    Returns:
        A list of strings, each element containing a relative path from the
        Chromium root directory to a valid output directory.
    """
    return _get_build_directories_under_dir('out_*')


def _get_build_directories_under_dir(directory: str) -> list[str]:
    """Gets all valid output directories under the specified |directory|.

    Args:
        directory: A relative path to a directory under the Chromium root
            directory.

    Returns:
        A list of strings, each element containing a relative path from the
        Chromium root directory to a valid output directory.
    """
    valid_directories = []
    valid_args_files = glob.glob(
        os.path.join(CHROMIUM_ROOT, directory, '*', GN_ARGS_FILE))
    for vaf in valid_args_files:
        valid_directories.append(
            os.path.relpath(os.path.dirname(vaf), CHROMIUM_ROOT))
    return valid_directories


def _directory_builds_for_config(directory: str, target_os: str,
                                 target_cpu: str) -> bool:
    """Checks whether the specified directory builds for the specified config.

    Args:
        directory: The output directory to check.
        target_os: The target_os GN arg value to look for.
        target_cpu: The target_cpu GN arg value to look for.

    Returns:
        True if |directory| builds for |target_os| and |target_cpu|, otherwise
        False.
    """
    args_file = os.path.join(directory, GN_ARGS_FILE)
    try:
        with open(args_file, encoding='utf-8') as infile:
            contents = infile.read()
    except OSError:
        return False

    try:
        gn_args = gn_helpers.FromGNArgs(contents)
    except (gn_helpers.GNError, OSError):
        return False

    if gn_args.get('target_os', get_host_os()) != target_os:
        return False
    if gn_args.get('target_cpu', get_host_arch()) != target_cpu:
        return False

    return True


if __name__ == '__main__':
    mcp.run()
