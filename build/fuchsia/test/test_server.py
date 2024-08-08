# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test server set up."""

import logging
import os
import sys
import subprocess

from typing import List, Optional, Tuple

from common import DIR_SRC_ROOT, get_ssh_address
from compatible_utils import get_ssh_prefix

sys.path.append(os.path.join(DIR_SRC_ROOT, 'build', 'util', 'lib', 'common'))
# pylint: disable=import-error,wrong-import-position
import chrome_test_server_spawner
# pylint: enable=import-error,wrong-import-position


def _run_ssh_tunnel(target_addr: str,
                    port_maps: List[str]) -> subprocess.CompletedProcess:
    assert len(port_maps) > 0

    ssh_prefix = get_ssh_prefix(target_addr)

    # Allow a tunnel / control path to be established for the first time.
    # The sshconfig https://crsrc.org/c/build/fuchsia/test/sshconfig used here
    # persistents the connection.
    subprocess.run(ssh_prefix + ['echo', 'true'], check=True)

    forward_proc = subprocess.run(
        ssh_prefix + [
            '-O',
            'forward',  # Send SSH mux control signal.
            '-NT'  # Don't execute command; don't allocate terminal.
        ] + port_maps,
        capture_output=True,
        check=False,
        text=True)
    if forward_proc.returncode != 0:
        raise Exception(
            'Got an error code when requesting port forwarding: %d' %
            forward_proc.returncode)
    return forward_proc


def ports_forward(target_addr: str,
                  ports: List[Tuple[int, int]]) -> subprocess.CompletedProcess:
    """Establishes a port forwarding SSH task to forward ports from the host to
    the fuchsia endpoints specified by tuples of port numbers in format of
    [fuchsia-port, host-port]. Setting fuchsia-port to 0 would allow the fuchsia
    selecting a free port; host-port shouldn't be 0.

    Blocks until port forwarding is established.

    Returns the CompletedProcess of the SSH task."""
    assert len(ports) > 0
    forward_cmd = []
    for port in ports:
        assert port[1] > 0 and port[1] <= 65535
        forward_cmd.extend(['-R', f'{port[0]}:localhost:{port[1]}'])
    return _run_ssh_tunnel(target_addr, forward_cmd)


def ports_backward(
        target_addr: str,
        ports: List[Tuple[int, int]]) -> subprocess.CompletedProcess:
    """Establishes a reverse port forwarding SSH task to forward ports from the
    fuchsia to the host endpoints specified by tuples of port numbers in format
    of [fuchsia-port, host-port]. Setting host-port to 0 would allow the host
    selecting a free port; fuchsia-port shouldn't be 0.

    Blocks until port forwarding is established.

    Returns the CompletedProcess of the SSH task."""
    assert len(ports) > 0
    forward_cmd = []
    for port in ports:
        assert port[0] > 0 and port[0] <= 65535
        forward_cmd.extend(['-L', f'{port[1]}:localhost:{port[0]}'])
    return _run_ssh_tunnel(target_addr, forward_cmd)


def port_forward(target_addr: str, host_port: int) -> int:
    """Establishes a port forwarding SSH task to a host TCP endpoint at port
    |host_port|. Blocks until port forwarding is established.

    Returns the fuchsia port number."""

    forward_proc = ports_forward(target_addr, [(0, host_port)])
    parsed_port = int(forward_proc.stdout.splitlines()[0].strip())
    logging.debug('Port forwarding established (local=%d, device=%d)',
                  host_port, parsed_port)
    return parsed_port


def port_backward(target_addr: str, fuchsia_port: int) -> int:
    """Establishes a reverse port forwarding SSH task to a fuchsia TCP endpoint
    at port |fuchsia_port|. Blocks until reverse port forwarding is established.

    Returns the local port number."""

    forward_proc = ports_backward(target_addr, [(fuchsia_port, 0)])
    parsed_port = int(forward_proc.stdout.splitlines()[0].strip())
    logging.debug('Reverse port forwarding established (local=%d, device=%d)',
                  parsed_port, fuchsia_port)
    return parsed_port


# Disable pylint errors since the subclass is not from this directory.
# pylint: disable=invalid-name,missing-function-docstring
class SSHPortForwarder(chrome_test_server_spawner.PortForwarder):
    """Implementation of chrome_test_server_spawner.PortForwarder that uses
    SSH's remote port forwarding feature to forward ports."""

    def __init__(self, target_addr: str) -> None:
        self._target_addr = target_addr

        # Maps the host (server) port to the device port number.
        self._port_mapping = {}

    def Map(self, port_pairs: List[Tuple[int, int]]) -> None:
        for p in port_pairs:
            _, host_port = p
            self._port_mapping[host_port] = \
                port_forward(self._target_addr, host_port)

    def GetDevicePortForHostPort(self, host_port: int) -> int:
        return self._port_mapping[host_port]

    def Unmap(self, device_port: int) -> None:
        for host_port, entry in self._port_mapping.items():
            if entry == device_port:
                ssh_prefix = get_ssh_prefix(self._target_addr)
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

    target_addr = get_ssh_address(target_id)

    # The TestLauncher can launch more jobs than the limit specified with
    # --test-launcher-jobs so the max number of spawned test servers is set to
    # twice that limit here. See https://crbug.com/913156#c19.
    spawning_server = chrome_test_server_spawner.SpawningServer(
        0, SSHPortForwarder(target_addr), test_concurrency * 2)

    forwarded_port = port_forward(target_addr, spawning_server.server_port)
    spawning_server.Start()

    logging.debug('Test server listening for connections (port=%d)',
                  spawning_server.server_port)
    logging.debug('Forwarded port is %d', forwarded_port)

    return (spawning_server, 'http://localhost:%d' % forwarded_port)
