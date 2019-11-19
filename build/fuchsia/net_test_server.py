# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
import json
import logging
import os
import re
import socket
import sys
import subprocess
import tempfile

DIR_SOURCE_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
sys.path.append(os.path.join(DIR_SOURCE_ROOT, 'build', 'util', 'lib', 'common'))
import chrome_test_server_spawner


# Implementation of chrome_test_server_spawner.PortForwarder that uses SSH's
# remote port forwarding feature to forward ports.
class SSHPortForwarder(chrome_test_server_spawner.PortForwarder):
  def __init__(self, target):
    self._target = target

    # Maps the host (server) port to the device port number.
    self._port_mapping = {}

  def Map(self, port_pairs):
    for p in port_pairs:
      _, host_port = p
      self._port_mapping[host_port] = \
          common.ConnectPortForwardingTask(self._target, host_port)

  def GetDevicePortForHostPort(self, host_port):
    return self._port_mapping[host_port]

  def Unmap(self, device_port):
    for host_port, entry in self._port_mapping.iteritems():
      if entry == device_port:
        forwarding_args = [
            '-NT', '-O', 'cancel', '-R', '0:localhost:%d' % host_port]
        task = self._target.RunCommandPiped([],
                                            ssh_args=forwarding_args,
                                            stderr=subprocess.PIPE)
        task.wait()
        if task.returncode != 0:
          raise Exception(
              'Error %d when unmapping port %d' % (task.returncode,
                                                   device_port))
        del self._port_mapping[host_port]
        return

    raise Exception('Unmap called for unknown port: %d' % device_port)


def SetupTestServer(target, test_concurrency, for_package):
  """Provisions a forwarding test server and configures |target| to use it.

  Returns a Popen object for the test server process."""

  logging.debug('Starting test server.')
  # The TestLauncher can launch more jobs than the limit specified with
  # --test-launcher-jobs so the max number of spawned test servers is set to
  # twice that limit here. See https://crbug.com/913156#c19.
  spawning_server = chrome_test_server_spawner.SpawningServer(
      0, SSHPortForwarder(target), test_concurrency * 2)
  forwarded_port = common.ConnectPortForwardingTask(
      target, spawning_server.server_port)
  spawning_server.Start()

  logging.debug('Test server listening for connections (port=%d)' %
                spawning_server.server_port)
  logging.debug('Forwarded port is %d' % forwarded_port)

  config_file = tempfile.NamedTemporaryFile(delete=True)

  # Clean up the config JSON to only pass ports. See https://crbug.com/810209 .
  config_file.write(json.dumps({
    'name': 'testserver',
    'address': '127.0.0.1',
    'spawner_url_base': 'http://localhost:%d' % forwarded_port
  }))

  config_file.flush()
  target.PutFile(config_file.name, '/tmp/net-test-server-config',
                 for_package=for_package)

  return spawning_server
