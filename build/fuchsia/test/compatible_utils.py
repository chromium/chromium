# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions used in both v1 and v2 scripts."""

import json
import os
import platform
import stat

from typing import Iterable, List, Tuple


_FILTER_DIR = 'testing/buildbot/filters'
_SSH_KEYS = os.path.expanduser('~/.ssh/fuchsia_authorized_keys')


class VersionNotFoundError(Exception):
    """Thrown when version info cannot be retrieved from device."""


def get_ssh_keys() -> str:
    """Returns path of Fuchsia ssh keys."""

    return _SSH_KEYS


def running_unattended() -> bool:
    """Returns true if running non-interactively.

    When running unattended, confirmation prompts and the like are suppressed.
    """

    # TODO(crbug/1401387): Change to mixin based approach.
    return 'SWARMING_SERVER' in os.environ


def get_host_arch() -> str:
    """Retrieve CPU architecture of the host machine. """
    host_arch = platform.machine()
    # platform.machine() returns AMD64 on 64-bit Windows.
    if host_arch in ['x86_64', 'AMD64']:
        return 'x64'
    if host_arch in ['aarch64', 'arm64']:
        return 'arm64'
    raise NotImplementedError('Unsupported host architecture: %s' % host_arch)


def add_exec_to_file(file: str) -> None:
    """Add execution bits to a file.

    Args:
        file: path to the file.
    """
    file_stat = os.stat(file)
    os.chmod(file, file_stat.st_mode | stat.S_IXUSR)


def parse_host_port(host_port_pair: str) -> Tuple[str, int]:
    """Parses a host name or IP address and a port number from a string of
    any of the following forms:
    - hostname:port
    - IPv4addy:port
    - [IPv6addy]:port

    Returns:
        A tuple of the string host name/address and integer port number.

    Raises:
        ValueError if `host_port_pair` does not contain a colon or if the
        substring following the last colon cannot be converted to an int.
    """

    host, port = host_port_pair.rsplit(':', 1)

    # Strip the brackets if the host looks like an IPv6 address.
    if len(host) >= 4 and host[0] == '[' and host[-1] == ']':
        host = host[1:-1]
    return (host, int(port))


def get_ssh_prefix(host_port_pair: str) -> List[str]:
    """Get the prefix of a barebone ssh command."""

    ssh_addr, ssh_port = parse_host_port(host_port_pair)
    sshconfig = os.path.join(os.path.dirname(__file__), 'sshconfig')
    return ['ssh', '-F', sshconfig, ssh_addr, '-p', str(ssh_port)]


def install_symbols(package_paths: Iterable[str],
                    fuchsia_out_dir: str) -> None:
    """Installs debug symbols for a package into the GDB-standard symbol
    directory located in fuchsia_out_dir."""

    symbol_root = os.path.join(fuchsia_out_dir, '.build-id')
    for path in package_paths:
        package_dir = os.path.dirname(path)
        ids_txt_path = os.path.join(package_dir, 'ids.txt')
        with open(ids_txt_path, 'r') as f:
            for entry in f:
                build_id, binary_relpath = entry.strip().split(' ')
                binary_abspath = os.path.abspath(
                    os.path.join(package_dir, binary_relpath))
                symbol_dir = os.path.join(symbol_root, build_id[:2])
                symbol_file = os.path.join(symbol_dir, build_id[2:] + '.debug')
                if not os.path.exists(symbol_dir):
                    os.makedirs(symbol_dir)

                if os.path.islink(symbol_file) or os.path.exists(symbol_file):
                    # Clobber the existing entry to ensure that the symlink's
                    # target is up to date.
                    os.unlink(symbol_file)
                os.symlink(os.path.relpath(binary_abspath, symbol_dir),
                           symbol_file)


# TODO(crbug.com/1279803): Until one can send files to the device when running
# a test, filter files must be read from the test package.
def map_filter_file_to_package_file(filter_file: str) -> str:
    """Returns the path to |filter_file| within the test component's package."""

    if not _FILTER_DIR in filter_file:
        raise ValueError('CFv2 tests only support registered filter files '
                         'present in the test package')
    return '/pkg/' + filter_file[filter_file.index(_FILTER_DIR):]


# TODO(crbug.com/1496426): Rename to get_product_version.
def get_sdk_hash(system_image_dir: str) -> Tuple[str, str]:
    """Read version of hash in pre-installed package directory.
    Returns:
        Tuple of (product, version) of image to be installed.
    """

    with open(os.path.join(system_image_dir,
                           'product_bundle.json')) as product:
        # The product_name in the json file does not match the name of the image
        # flashed to the device.
        return (os.path.basename(os.path.normpath(system_image_dir)),
                json.load(product)['product_version'])
