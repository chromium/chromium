# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
import json
import logging
import os
import shutil
import subprocess
import tempfile
import time
import urllib2


# Maximum amount of time to block while waiting for "pm serve" to come up.
_PM_SERVE_LIVENESS_TIMEOUT_SECS = 10

_MANAGED_REPO_NAME = 'chrome_runner'


class AmberRepo(object):
  """Abstract interface for a repository used to serve packages to devices."""

  def __init__(self, target):
    self._target = target

  def PublishPackage(self, package_path):
    pm_tool = common.GetHostToolPathFromPlatform('pm')
    subprocess.check_call(
        [pm_tool, 'publish', '-a', '-f', package_path, '-r', self.GetPath(),
         '-vt', '-v'],
        stderr=subprocess.STDOUT)

  def GetPath(self):
    pass


class ManagedAmberRepo(AmberRepo):
  """Creates and serves packages from an ephemeral repository."""

  def __init__(self, target):
    AmberRepo.__init__(self, target)

    self._amber_root = tempfile.mkdtemp()
    pm_tool = common.GetHostToolPathFromPlatform('pm')
    subprocess.check_call([pm_tool, 'newrepo', '-repo', self._amber_root])
    logging.info('Creating and serving temporary Amber root: {}.'.format(
        self._amber_root))

    serve_port = common.GetAvailableTcpPort()
    self._pm_serve_task = subprocess.Popen(
        [pm_tool, 'serve', '-d', os.path.join(self._amber_root, 'repository'),
         '-l', ':%d' % serve_port, '-q'])

    # Block until "pm serve" starts serving HTTP traffic at |serve_port|.
    timeout = time.time() + _PM_SERVE_LIVENESS_TIMEOUT_SECS
    while True:
      try:
        urllib2.urlopen('http://localhost:%d' % serve_port, timeout=1).read()
        break
      except urllib2.URLError:
        logging.info('Waiting until \'pm serve\' is up...')

      if time.time() >= timeout:
        raise Exception('Timed out while waiting for \'pm serve\'.')

      time.sleep(1)

    remote_port = common.ConnectPortForwardingTask(target, serve_port, 0)
    self._RegisterAmberRepository(self._amber_root, remote_port)

  def __enter__(self):
    return self

  def __exit__(self, type, value, tb):
    """Allows the repository to delete itself when it leaves the scope of a
    'with' block."""
    if self._amber_root:
      logging.info('Cleaning up Amber root: ' + self._amber_root)
      shutil.rmtree(self._amber_root)

    self._UnregisterAmberRepository()
    if self._pm_serve_task:
      self._pm_serve_task.kill()

  def GetPath(self):
    return self._amber_root

  def _RegisterAmberRepository(self, tuf_repo, remote_port):
    """Configures a device to use a local TUF repository as an installation
    source for packages.
    |tuf_repo|: The host filesystem path to the TUF repository.
    |remote_port|: The reverse-forwarded port used to connect to instance of
                   `pm serve` that is serving the contents of |tuf_repo|."""

# Extract the public signing key for inclusion in the config file.
    root_keys = []
    root_json_path = os.path.join(tuf_repo, 'repository', 'root.json')
    root_json = json.load(open(root_json_path, 'r'))
    for root_key_id in root_json['signed']['roles']['root']['keyids']:
      root_keys.append({
          'Type': root_json['signed']['keys'][root_key_id]['keytype'],
          'Value': root_json['signed']['keys'][root_key_id]['keyval']['public']
      })

    # "pm serve" can automatically generate a "config.json" file at query time,
    # but the file is unusable because it specifies URLs with port
    # numbers that are unreachable from across the port forwarding boundary.
    # So instead, we generate our own config file with the forwarded port
    # numbers instead.
    config_file = open(os.path.join(tuf_repo, 'repository', 'repo_config.json'),
                       'w')
    json.dump({
        'ID': _MANAGED_REPO_NAME,
        'RepoURL': "http://127.0.0.1:%d" % remote_port,
        'BlobRepoURL': "http://127.0.0.1:%d/blobs" % remote_port,
        'RatePeriod': 10,
        'RootKeys': root_keys,
        'StatusConfig': {
            'Enabled': True
        },
        'Auto': True
    }, config_file)
    config_file.close()

    # Register the repo.
    return_code = self._target.RunCommand(
        [('amberctl rm_src -n %s; ' +
          'amberctl add_src -f http://127.0.0.1:%d/repo_config.json')
         % (_MANAGED_REPO_NAME, remote_port)])
    if return_code != 0:
      raise Exception('Error code %d when running amberctl.' % return_code)


  def _UnregisterAmberRepository(self):
    """Unregisters the Amber repository."""

    logging.debug('Unregistering Amber repository.')
    self._target.RunCommand(['amberctl', 'rm_src', '-n', _MANAGED_REPO_NAME])

    # Re-enable 'devhost' repo if it's present. This is useful for devices that
    # were booted with 'fx serve'.
    self._target.RunCommand(['amberctl', 'enable_src', '-n', 'devhost'],
                            silent=True)


class ExternalAmberRepo(AmberRepo):
  """Publishes packages to an Amber repository located and served externally
  (ie. located under a Fuchsia build directory and served by "fx serve"."""

  def __init__(self, amber_root):
    self._amber_root = amber_root
    logging.info('Using existing Amber root: {}'.format(amber_root))

#TODO(kmarshall) : Find a way to programatically check if "fx serve" is running.
    logging.info('Ensure that "fx serve" is running.')

  def GetPath(self):
    return self._amber_root

  def __enter__(self):
    return self

  def __exit__(self, type, value, tb):
    pass
