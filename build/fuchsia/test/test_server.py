# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test server set up."""

import logging
import os
import sys
import subprocess

from typing import List, Optional, Tuple

from common import DIR_SRC_ROOT, run_ffx_command
from compatible_utils import get_ssh_prefix

sys.path.append(os.path.join(DIR_SRC_ROOT, 'build', 'util', 'lib', 'common'))
# pylint: disable=import-error,wrong-import-position
import chrome_test_server_spawner
# pylint: enable=import-error,wrong-import-position


def port_forward(host_port_pair: str, host_port: int) -> int:
    """Establishes a port forwarding SSH task to a localhost TCP endpoint
    hosted at port |local_port|. Blocks until port forwarding is established.

    Returns the remote port number."""

    ssh_prefix = get_ssh_prefix(host_port_pair)

    # Allow a tunnel to be established.
    subprocess.run(ssh_prefix + ['echo', 'true'], check=True)

    forward_cmd = [
        '-O',
        'forward',  # Send SSH mux control signal.
        '-R',
        '0:localhost:%d' % host_port,
        '-v',  # Get forwarded port info from stderr.
        '-NT'  # Don't execute command; don't allocate terminal.
    ]
    forward_proc = subprocess.run(ssh_prefix + forward_cmd,
                                  capture_output=True,
                                  check=False,
                                  text=True)
    if forward_proc.returncode != 0:
        raise Exception(
            'Got an error code when requesting port forwarding: %d' %
            forward_proc.returncode)

    output = forward_proc.stdout
    parsed_port = int(output.splitlines()[0].strip())
    logging.debug('Port forwarding established (local=%d, device=%d)',
                  host_port, parsed_port)
    return parsed_port


# Disable pylint errors since the subclass is not from this directory.
# pylint: disable=invalid-name,missing-function-docstring
class SSHPortForwarder(chrome_test_server_spawner.PortForwarder):
    """Implementation of chrome_test_server_spawner.PortForwarder that uses
    SSH's remote port forwarding feature to forward ports."""

    def __init__(self, host_port_pair: str) -> None:
        self._host_port_pair = host_port_pair

        # Maps the host (server) port to the device port number.
        self._port_mapping = {}

    def Map(self, port_pairs: List[Tuple[int, int]]) -> None:
        for p in port_pairs:
            _, host_port = p
            self._port_mapping[host_port] = \
                port_forward(self._host_port_pair, host_port)

    def GetDevicePortForHostPort(self, host_port: int) -> int:
        return self._port_mapping[host_port]

    def Unmap(self, device_port: int) -> None:
        for host_port, entry in self._port_mapping.items():
            if entry == device_port:
                ssh_prefix = get_ssh_prefix(self._host_port_pair)
                unmap_cmd = [
                    '-NT', '-O', 'cancel', '-R',
                    '0:localhost:%d' % host_port
                ]
                ssh_proc = subprocess.run(ssh_prefix + unmap_cmd, check=False)
                if ssh_proc.returncode != 0:
                    raise Exception('Error %d when unmapping port %d' %
                                    (ssh_proc.returncode, device_port))
                del self._port_mapping[host_port]
                return

        raise Exception('Unmap called for unknown port: %d' % device_port)


# pylint: enable=invalid-name,missing-function-docstring


def setup_test_server(target_id: Optional[str], test_concurrency: int)\
         -> Tuple[chrome_test_server_spawner.SpawningServer, str]:
    """Provisions a test server and configures |target_id| to use it.

    Args:
        target_id: The target to which port forwarding to the test server will
            be established.
        test_concurrency: The number of parallel test jobs that will be run.

    Returns a tuple of a SpawningServer object and the local url to use on
    |target_id| to reach the test server."""

    logging.debug('Starting test server.')

    host_port_pair = run_ffx_command(('target', 'get-ssh-address'),
                                     target_id,
                                     capture_output=True).stdout.strip()

    # The TestLauncher can launch more jobs than the limit specified with
    # --test-launcher-jobs so the max number of spawned test servers is set to
    # twice that limit here. See https://crbug.com/913156#c19.
    spawning_server = chrome_test_server_spawner.SpawningServer(
        0, SSHPortForwarder(host_port_pair), test_concurrency * 2)

    forwarded_port = port_forward(host_port_pair, spawning_server.server_port)
    spawning_server.Start()

    logging.debug('Test server listening for connections (port=%d)',
                  spawning_server.server_port)
    logging.debug('Forwarded port is %d', forwarded_port)

    return (spawning_server, 'http://localhost:%d' % forwarded_port)
