# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))
from compatible_utils import install_symbols

# Maximum amount of time to block while waiting for "pm serve" to come up.
_PM_SERVE_LISTEN_TIMEOUT_SECS = 10

# Amount of time to sleep in between busywaits for "pm serve"'s port file.
_PM_SERVE_POLL_INTERVAL = 0.1

_MANAGED_REPO_NAME = 'chromium-test-package-server'

_HOSTS = ['fuchsia.com', 'chrome.com', 'chromium.org']


class PkgRepo(object):
  """Abstract interface for a repository used to serve packages to devices."""

  def __init__(self):
    pass

  def PublishPackage(self, package_path):
    pm_tool = common.GetHostToolPathFromPlatform('pm')
    # Flags for `pm publish`:
    # https://fuchsia.googlesource.com/fuchsia/+/refs/heads/main/src/sys/pkg/bin/pm/cmd/pm/publish/publish.go
    # https://fuchsia.googlesource.com/fuchsia/+/refs/heads/main/src/sys/pkg/bin/pm/repo/config.go
    # -a: Publish archived package
    # -f <path>: Path to packages
    # -r <path>: Path to repository
    # -vt: Repo versioning based on time rather than monotonic version number
    #      increase
    # -v: Verbose output
    subprocess.check_call([
        pm_tool, 'publish', '-a', '-f', package_path, '-r',
        self.GetPath(), '-vt', '-v'
    ], stderr=subprocess.STDOUT)

  def GetPath(self):
    pass


class ManagedPkgRepo(PkgRepo):
  """Creates and serves packages from an ephemeral repository."""

  def __init__(self, target):
    super(ManagedPkgRepo, self).__init__()
    self._with_count = 0
    self._target = target

    self._pkg_root = tempfile.mkdtemp()
    pm_tool = common.GetHostToolPathFromPlatform('pm')
    subprocess.check_call([pm_tool, 'newrepo', '-repo', self._pkg_root])
    logging.debug('Creating and serving temporary package root: {}.'.format(
        self._pkg_root))

    with tempfile.NamedTemporaryFile() as pm_port_file:
      # Flags for `pm serve`:
      # https://fuchsia.googlesource.com/fuchsia/+/refs/heads/main/src/sys/pkg/bin/pm/cmd/pm/serve/serve.go
      self._pm_serve_task = subprocess.Popen([
          pm_tool, 'serve',
          '-d', os.path.join(self._pkg_root, 'repository'),
          '-c', '2',  # Use config.json format v2, the default for pkgctl.
          '-q',  # Don't log transfer activity.
          '-l', ':0',  # Bind to ephemeral port.
          '-f', pm_port_file.name  # Publish port number to |pm_port_file|.
      ]) # yapf: disable

      # Busywait until 'pm serve' starts the server and publishes its port to
      # a temporary file.
      timeout = time.time() + _PM_SERVE_LISTEN_TIMEOUT_SECS
      serve_port = None
      while not serve_port:
        if time.time() > timeout:
          raise Exception(
              'Timeout waiting for \'pm serve\' to publish its port.')

        with open(pm_port_file.name, 'r', encoding='utf8') as serve_port_file:
          serve_port = serve_port_file.read()

        time.sleep(_PM_SERVE_POLL_INTERVAL)

      serve_port = int(serve_port)
      logging.debug('pm serve is active on port {}.'.format(serve_port))

    remote_port = common.ConnectPortForwardingTask(target, serve_port, 0)
    self._RegisterPkgRepository(self._pkg_root, remote_port)

  def __enter__(self):
    self._with_count += 1
    return self

  def __exit__(self, type, value, tb):
    # Allows the repository to delete itself when it leaves the scope of a
    # 'with' block.
    self._with_count -= 1
    if self._with_count > 0:
      return

    self._UnregisterPkgRepository()
    self._pm_serve_task.kill()
    self._pm_serve_task = None

    logging.info('Cleaning up package root: ' + self._pkg_root)
    shutil.rmtree(self._pkg_root)
    self._pkg_root = None

  def GetPath(self):
    return self._pkg_root

  def _RegisterPkgRepository(self, tuf_repo, remote_port):
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
          'type':
          root_json['signed']['keys'][root_key_id]['keytype'],
          'value':
          root_json['signed']['keys'][root_key_id]['keyval']['public']
      })

    # "pm serve" can automatically generate a "config.json" file at query time,
    # but the file is unusable because it specifies URLs with port
    # numbers that are unreachable from across the port forwarding boundary.
    # So instead, we generate our own config file with the forwarded port
    # numbers instead.
    config_file = open(os.path.join(tuf_repo, 'repository', 'repo_config.json'),
                       'w')
    json.dump(
        {
            'repo_url':
            'fuchsia-pkg://{}'.format(_MANAGED_REPO_NAME),
            'root_keys':
            root_keys,
            'mirrors': [{
                "mirror_url": 'http://127.0.0.1:{}'.format(remote_port),
                "subscribe": True
            }],
            'root_threshold':
            1,
            'root_version':
            1
        }, config_file)
    config_file.close()

    # Register the repo.
    return_code = self._target.RunCommand([
        ('pkgctl repo rm fuchsia-pkg://{}; ' +
         'pkgctl repo add url http://127.0.0.1:{}/repo_config.json; ').format(
             _MANAGED_REPO_NAME, remote_port)
    ])
    if return_code != 0:
      raise Exception(
          'Error code {} when running pkgctl repo add.'.format(return_code))

    self._AddHostReplacementRule(_MANAGED_REPO_NAME)

  def _UnregisterPkgRepository(self):
    """Unregisters the package repository."""

    logging.debug('Unregistering package repository.')
    self._target.RunCommand(
        ['pkgctl', 'repo', 'rm', 'fuchsia-pkg://{}'.format(_MANAGED_REPO_NAME)])

    # Re-enable 'devhost' repo if it's present. This is useful for devices that
    # were booted with 'fx serve'.
    self._AddHostReplacementRule('devhost', silent=True)

  def _AddHostReplacementRule(self, host_replacement, silent=False):
    rule = json.dumps({
        'version':
        '1',
        'content': [{
            'host_match': host,
            'host_replacement': host_replacement,
            'path_prefix_match': '/',
            'path_prefix_replacement': '/'
        } for host in _HOSTS]
    })

    return_code = self._target.RunCommand(
        ['pkgctl', 'rule', 'replace', 'json', "'{}'".format(rule)])
    if not silent and return_code != 0:
      raise Exception(
          'Error code {} when running pkgctl rule replace with {}'.format(
              return_code, rule))


class ExternalPkgRepo(PkgRepo):
  """Publishes packages to a package repository located and served externally
  (ie. located under a Fuchsia build directory and served by "fx serve"."""

  def __init__(self, fuchsia_out_dir):
    super(PkgRepo, self).__init__()

    self._fuchsia_out_dir = fuchsia_out_dir
    self._pkg_root = os.path.join(fuchsia_out_dir, 'amber-files')

    logging.info('Using existing package root: {}'.format(self._pkg_root))
    logging.info('ATTENTION: This will not start a package server. ' +
                 'Please run "fx serve" manually.')

  def GetPath(self):
    return self._pkg_root

  def PublishPackage(self, package_path):
    super(ExternalPkgRepo, self).PublishPackage(package_path)

    install_symbols((package_path), self._fuchsia_out_dir)

  def __enter__(self):
    return self

  def __exit__(self, type, value, tb):
    pass
