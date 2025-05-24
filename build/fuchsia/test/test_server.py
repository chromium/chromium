# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test server set up."""

import logging
import os
import sys
import subprocess

from typing import List, Optional, Tuple

from common import DIR_SRC_ROOT, get_free_local_port, get_ssh_address
from compatible_utils import get_ssh_prefix

sys.path.append(os.path.join(DIR_SRC_ROOT, 'build', 'util', 'lib', 'common'))
# pylint: disable=import-error,wrong-import-position
import chrome_test_server_spawner
# pylint: enable=import-error,wrong-import-position


def _run_ssh_tunnel(target_addr: str, command: str,
                    port_maps: List[str]) -> subprocess.CompletedProcess:
    assert port_maps

    ssh_prefix = get_ssh_prefix(target_addr)

    # Allow a tunnel / control path to be established for the first time.
    # The sshconfig https://crsrc.org/c/build/fuchsia/test/sshconfig used here
    # persists the connection.
    subprocess.run(ssh_prefix + ['echo', 'true'], check=True)

    forward_proc = subprocess.run(
        ssh_prefix + [
            '-O',
            command,  # Send SSH mux control signal.
            '-NT'  # Don't execute command; don't allocate terminal.
        ] + port_maps,
        capture_output=True,
        check=True,
        text=True)
    return forward_proc


def _forward_command(fuchsia_port: int, host_port: int,
                     port_forwarding: bool) -> List[str]:
    max_port = 65535
    assert fuchsia_port is not None and 0 <= fuchsia_port <= max_port
    assert host_port is not None and 0 < host_port <= max_port
    if port_forwarding:
        return ['-R', f'{fuchsia_port}:localhost:{host_port}']
    assert fuchsia_port != 0
    return ['-L', f'{host_port}:localhost:{fuchsia_port}']


def _forward_commands(ports: List[Tuple[int, int]],
                      port_forwarding: bool) -> List[str]:
    assert ports
    forward_cmd = []
    for port in ports:
        assert port is not None
        forward_cmd.extend(_forward_command(port[0], port[1], port_forwarding))
    return forward_cmd


def ports_forward(target_addr: str,
                  ports: List[Tuple[int, int]]) -> subprocess.CompletedProcess:
    """Establishes a port forwarding SSH task to forward ports from the host to
    the fuchsia endpoints specified by tuples of port numbers in format of
    [fuchsia-port, host-port]. Setting fuchsia-port to 0 would allow the fuchsia
    selecting a free port; host-port shouldn't be 0.

    Blocks until port forwarding is established.

    Returns the CompletedProcess of the SSH task."""
    return _run_ssh_tunnel(target_addr, 'forward',
                           _forward_commands(ports, True))


def ports_backward(
        target_addr: str,
        ports: List[Tuple[int, int]]) -> subprocess.CompletedProcess:
    """Establishes a reverse port forwarding SSH task to forward ports from the
    fuchsia to the host endpoints specified by tuples of port numbers in format
    of [fuchsia-port, host-port]. Both host-port and fuchsia-port shouldn't be
    0.

    Blocks until port forwarding is established.

    Returns the CompletedProcess of the SSH task."""
    return _run_ssh_tunnel(target_addr, 'forward',
                           _forward_commands(ports, False))


def port_forward(target_addr: str, host_port: int) -> int:
    """Establishes a port forwarding SSH task to a host TCP endpoint at port
    |host_port|. Blocks until port forwarding is established.

    Returns the fuchsia port number."""

    forward_proc = ports_forward(target_addr, [(0, host_port)])
    parsed_port = int(forward_proc.stdout.splitlines()[0].strip())
    logging.debug('Port forwarding established (local=%d, device=%d)',
                  host_port, parsed_port)
    return parsed_port


def port_backward(target_addr: str,
                  fuchsia_port: int,
                  host_port: int = 0) -> int:
    """Establishes a reverse port forwarding SSH task to a fuchsia TCP endpoint
    at port |fuchsia_port| from the host at port |host_port|. If |host_port| is
    None or 0, a local free port will be selected.
    Blocks until reverse port forwarding is established.

    Returns the local port number."""

    if not host_port:
        host_port = get_free_local_port()
    ports_backward(target_addr, [(fuchsia_port, host_port)])
    logging.debug('Reverse port forwarding established (local=%d, device=%d)',
                  host_port, fuchsia_port)
    return host_port


def cancel_port_forwarding(target_addr: str, fuchsia_port: int, host_port: int,
                           port_forwarding: bool) -> None:
    """Cancels an existing port forwarding, if port_forwarding is false, it will
    be treated as reverse port forwarding.
    Note, the ports passing in here need to exactly match the ports used to
    setup the port forwarding, i.e. if ports_forward([0, 8080]) was issued, even
    it returned an allocated port, cancel_port_forwarding(..., 0, 8080, ...)
    should still be used to cancel the port forwarding."""
    _run_ssh_tunnel(target_addr, 'cancel',
                    _forward_command(fuchsia_port, host_port, port_forwarding))


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
            fuchsia_port, host_port = p
            assert fuchsia_port == 0, \
                'Port forwarding with a fixed fuchsia-port is unsupported yet.'
            self._port_mapping[host_port] = \
                port_forward(self._target_addr, host_port)

    def GetDevicePortForHostPort(self, host_port: int) -> int:
        return self._port_mapping[host_port]

    def Unmap(self, device_port: int) -> None:
        for host_port, fuchsia_port in self._port_mapping.items():
            if fuchsia_port == device_port:
                cancel_port_forwarding(self._target_addr, 0, host_port, True)
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
