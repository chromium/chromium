#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""MCP server for providing host information relevant to building Chromium."""

import enum
import platform
import sys

from mcp.server import fastmcp


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


mcp = fastmcp.FastMCP(name="Chromium Host Information")


# This tool can be moved to a resource once Gemini CLI supports them.
@mcp.tool()
def get_host_os() -> str:
    """Retrieves the operating system used by the current host. The return
    value is directly comparable with the target_os GN arg, except in the case
    of 'unknown'."""
    platform = sys.platform
    if platform in ('linux', 'cygwin'):
        return ValidOs.LINUX
    if platform == 'win32':
        return ValidOs.WIN
    if platform == 'darwin':
        return ValidOs.MAC
    return ValidOs.UNKNOWN


# This tool can be moved to a resource once Gemini CLI supports them.
@mcp.tool()
def get_host_arch() -> str:
    """Retrieves the CPU architecture used by the current host. The return
    value is directly comparable with the target_arch GN arg, except in the
    case of 'unknown'."""
    arch = _get_host_architecture()
    if arch == Architecture.UNKNOWN:
        return ValidArch.UNKNOWN
    bits = _get_host_bits()
    if bits == Bitness.UNKNOWN:
        return ValidArch.UNKNOWN

    if arch == Architecture.INTEL:
        if bits == Bitness.THIRTY_TWO:
            return ValidArch.X86
        if bits == Bitness.SIXTY_FOUR:
            return ValidArch.X64
        return ValidArch.UNKNOWN

    if arch == Architecture.ARM:
        if bits == Bitness.THIRTY_TWO:
            return ValidArch.ARM
        if bits == Bitness.SIXTY_FOUR:
            return ValidArch.ARM64
        return ValidArch.UNKNOWN

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


if __name__ == '__main__':
    mcp.run()