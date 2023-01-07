# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions used in both v1 and v2 scripts."""

import os
import re
import subprocess

from typing import List, Optional, Tuple


# File indicating version of an image downloaded to the host
_BUILD_ARGS = "buildargs.gn"

_FILTER_DIR = 'testing/buildbot/filters'


class VersionNotFoundError(Exception):
    """Thrown when version info cannot be retrieved from device."""


def pave(image_dir: str, target_id: Optional[str])\
        -> subprocess.CompletedProcess:
    """"Pave a device using the pave script inside |image_dir|."""

    pave_command = [
        os.path.join(image_dir, 'pave.sh'), '--authorized-keys',
        os.path.expanduser('~/.ssh/fuchsia_authorized_keys'), '-1'
    ]
    if target_id:
        pave_command.extend(['-n', target_id])
    return subprocess.run(pave_command, check=True, text=True, timeout=300)


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
    return [
        'ssh', '-F',
        os.path.expanduser('~/.fuchsia/sshconfig'), ssh_addr, '-p',
        str(ssh_port)
    ]


# TODO(crbug.com/1279803): Until one can send files to the device when running
# a test, filter files must be read from the test package.
def map_filter_file_to_package_file(filter_file: str) -> str:
    """Returns the path to |filter_file| within the test component's package."""

    if not _FILTER_DIR in filter_file:
        raise ValueError('CFv2 tests only support registered filter files '
                         'present in the test package')
    return '/pkg/' + filter_file[filter_file.index(_FILTER_DIR):]


def get_sdk_hash(system_image_dir: str) -> Tuple[str, str]:
    """Read version of hash in pre-installed package directory.
    Returns:
        Tuple of (product, version) of image to be installed.
    Raises:
        VersionNotFoundError: if contents of buildargs.gn cannot be found or the
        version number cannot be extracted.
    """

    # TODO(crbug.com/1261961): Stop processing buildargs.gn directly.
    with open(os.path.join(system_image_dir, _BUILD_ARGS)) as f:
        contents = f.readlines()
    if not contents:
        raise VersionNotFoundError('Could not retrieve %s' % _BUILD_ARGS)
    version_key = 'build_info_version'
    product_key = 'build_info_product'
    info_keys = [product_key, version_key]
    version_info = {}
    for line in contents:
        for key in info_keys:
            match = re.match(r'%s = "(.*)"' % key, line)
            if match:
                version_info[key] = match.group(1)
    if not (version_key in version_info and product_key in version_info):
        raise VersionNotFoundError(
            'Could not extract version info from %s. Contents: %s' %
            (_BUILD_ARGS, contents))

    return (version_info[product_key], version_info[version_key])
