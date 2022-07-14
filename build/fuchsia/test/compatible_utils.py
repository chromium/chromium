# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions used in both v1 and v2 scripts."""

from typing import Tuple

_FILTER_DIR = 'testing/buildbot/filters'


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


# TODO(crbug.com/1279803): Until one can send files to the device when running
# a test, filter files must be read from the test package.
def map_filter_file_to_package_file(filter_file: str) -> str:
    """Returns the path to |filter_file| within the test component's package."""

    if not _FILTER_DIR in filter_file:
        raise ValueError('CFv2 tests only support registered filter files '
                         'present in the test package')
    return '/pkg/' + filter_file[filter_file.index(_FILTER_DIR):]
