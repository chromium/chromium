#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing test_server.py."""

import unittest
import unittest.mock as mock

import test_server

_HOST_PORT = 44444
_HOST_PORT_PAIR = '127.0.0.1:33333'
_SERVER_PORT = 55555


class TestServerTest(unittest.TestCase):
    """Unittests for test_server.py."""

    def setUp(self) -> None:
        self._subprocess_patcher = mock.patch('test_server.subprocess.run')
        self._log_patcher = mock.patch('test_server.logging.debug')
        self._subprocess_mock = self._subprocess_patcher.start()
        self._log_mock = self._log_patcher.start()
        self.addCleanup(self._log_mock.stop)
        self.addCleanup(self._subprocess_mock.stop)

    def test_ssh_port_forwarder(self) -> None:
        """Test SSHPortForwarder."""

        port_pair = (_HOST_PORT, _SERVER_PORT)
        cmd_mock = mock.Mock()
        cmd_mock.returncode = 0
        cmd_mock.stdout = str(port_pair[0])
        self._subprocess_mock.return_value = cmd_mock

        forwarder = test_server.SSHPortForwarder(_HOST_PORT_PAIR)

        # Unmap should raise an exception if no ports are mapped.
        with self.assertRaises(Exception):
            forwarder.Unmap(port_pair[0])

        forwarder.Map([port_pair])
        self.assertEqual(self._subprocess_mock.call_count, 2)
        self.assertEqual(forwarder.GetDevicePortForHostPort(port_pair[1]),
                         port_pair[0])

        # Unmap should also raise an exception if the unmap command fails.
        self._subprocess_mock.reset_mock()
        cmd_mock.returncode = 1
        with self.assertRaises(Exception):
            forwarder.Unmap(port_pair[0])
        self.assertEqual(self._subprocess_mock.call_count, 1)

        self._subprocess_mock.reset_mock()
        cmd_mock.returncode = 0
        forwarder.Unmap(port_pair[0])
        self.assertEqual(self._subprocess_mock.call_count, 1)

    def test_port_forward_exception(self) -> None:
        """Tests that exception is raised if |port_forward| command fails."""

        cmd_mock = mock.Mock()
        cmd_mock.returncode = 1
        self._subprocess_mock.return_value = cmd_mock
        with self.assertRaises(Exception):
            test_server.port_forward(_HOST_PORT_PAIR, _HOST_PORT)

    @mock.patch('test_server.chrome_test_server_spawner.SpawningServer')
    @mock.patch('test_server.port_forward')
    def test_setup_test_server(self, forward_mock, server_mock) -> None:
        """Test |setup_test_server|."""

        forward_mock.return_value = _HOST_PORT
        server = test_server.chrome_test_server_spawner.SpawningServer
        server.Start = mock.Mock()
        server_mock.return_value = server
        with mock.patch('test_server.run_ffx_command'):
            _, url = test_server.setup_test_server(_HOST_PORT_PAIR, 4)
        self.assertTrue(str(_HOST_PORT) in url)


if __name__ == '__main__':
    unittest.main()
