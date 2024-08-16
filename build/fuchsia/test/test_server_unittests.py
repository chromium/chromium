#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing test_server.py."""

import unittest
import unittest.mock as mock

from subprocess import CalledProcessError

import test_server

# TODO(crbug.com/40935291): Specifying fuchsia-port is unsupported yet.
_FUCHSIA_PORT = 0
_TARGET_ADDR = '127.0.0.1:33333'
_HOST_PORT = 55555

port_pair = (_FUCHSIA_PORT, _HOST_PORT)

# Test names should be self-explained.
# pylint: disable=missing-function-docstring

class TestServerTest(unittest.TestCase):
    """Unittests for test_server.py."""

    def setUp(self) -> None:
        self._subprocess_patcher = mock.patch('test_server.subprocess.run')
        self._log_patcher = mock.patch('test_server.logging.debug')
        self._subprocess_mock = self._subprocess_patcher.start()
        self._log_mock = self._log_patcher.start()
        self.addCleanup(self._log_mock.stop)
        self.addCleanup(self._subprocess_mock.stop)

        self._cmd_mock = mock.Mock()
        self._cmd_mock.returncode = 0
        self._cmd_mock.stdout = str(port_pair[0])
        self._subprocess_mock.return_value = self._cmd_mock

        self._forwarder = test_server.SSHPortForwarder(_TARGET_ADDR)
        self._forwarder.Map([port_pair])

    def test_ssh_port_forwarder(self) -> None:
        self.assertEqual(self._subprocess_mock.call_count, 2)
        self.assertEqual(
            self._forwarder.GetDevicePortForHostPort(port_pair[1]),
            port_pair[0])

    def test_ssh_port_forwarder_unmapped(self) -> None:
        self._forwarder.Unmap(port_pair[0])
        # Unmap should raise an exception if no ports are mapped.
        with self.assertRaises(Exception):
            self._forwarder.Unmap(port_pair[0])

    def test_ssh_port_forwarder_ssh_exception(self) -> None:
        # Unmap should also raise an exception if the unmap command fails.
        self._subprocess_mock.side_effect = [
            self._cmd_mock,
            CalledProcessError(cmd='ssh', returncode=1)
        ]
        with self.assertRaises(Exception):
            self._forwarder.Unmap(port_pair[0])
        self.assertEqual(self._subprocess_mock.call_count, 4)

    def test_ssh_port_forward_unmap(self) -> None:
        self._cmd_mock.returncode = 0
        self._forwarder.Unmap(port_pair[0])
        self.assertEqual(self._subprocess_mock.call_count, 4)

    def test_port_forward_exception(self) -> None:
        """Tests that exception is raised if |port_forward| command fails."""
        cmd_mock = mock.Mock()
        cmd_mock.returncode = 1
        self._subprocess_mock.return_value = cmd_mock
        with self.assertRaises(Exception):
            test_server.port_forward(_TARGET_ADDR, _FUCHSIA_PORT)

    @mock.patch('test_server.chrome_test_server_spawner.SpawningServer')
    @mock.patch('test_server.port_forward')
    def test_setup_test_server(self, forward_mock, server_mock) -> None:
        """Test |setup_test_server|."""

        forward_mock.return_value = _FUCHSIA_PORT
        server = test_server.chrome_test_server_spawner.SpawningServer
        server.Start = mock.Mock()
        server_mock.return_value = server
        with mock.patch('test_server.get_ssh_address'):
            _, url = test_server.setup_test_server(_TARGET_ADDR, 4)
        self.assertTrue(str(_FUCHSIA_PORT) in url)


if __name__ == '__main__':
    unittest.main()
