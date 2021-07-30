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

from six.moves import urllib

# Maximum amount of time to block while waiting for "pm serve" to come up.
_PM_SERVE_LIVENESS_TIMEOUT_SECS = 10

_MANAGED_REPO_NAME = 'chrome-runner'


class PkgRepo(object):
  """Abstract interface for a repository used to serve packages to devices."""

  def __init__(self, target):
    self._target = target

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
    ],
                          stderr=subprocess.STDOUT)

  def GetPath(self):
    pass


class ManagedPkgRepo(PkgRepo):
  """Creates and serves packages from an ephemeral repository."""

  def __init__(self, target):
    PkgRepo.__init__(self, target)
    self._with_count = 0

    self._pkg_root = tempfile.mkdtemp()
    pm_tool = common.GetHostToolPathFromPlatform('pm')
    subprocess.check_call([pm_tool, 'newrepo', '-repo', self._pkg_root])
    logging.info('Creating and serving temporary package root: {}.'.format(
        self._pkg_root))

    serve_port = common.GetAvailableTcpPort()
    # Flags for `pm serve`:
    # https://fuchsia.googlesource.com/fuchsia/+/refs/heads/main/src/sys/pkg/bin/pm/cmd/pm/serve/serve.go
    # -l <port>: Port to listen on
    # -c 2: Use config.json format v2, the default for pkgctl
    # -q: Don't print out information about requests
    self._pm_serve_task = subprocess.Popen([
        pm_tool, 'serve', '-d',
        os.path.join(self._pkg_root, 'repository'), '-l',
        ':%d' % serve_port, '-c', '2', '-q'
    ])

    # Block until "pm serve" starts serving HTTP traffic at |serve_port|.
    timeout = time.time() + _PM_SERVE_LIVENESS_TIMEOUT_SECS
    while True:
      try:
        urllib.request.urlopen('http://localhost:%d' % serve_port,
                               timeout=1).read()
        break
      except urllib.error.URLError:
        logging.info('Waiting until \'pm serve\' is up...')

      if time.time() >= timeout:
        raise Exception('Timed out while waiting for \'pm serve\'.')

      time.sleep(1)

    remote_port = common.ConnectPortForwardingTask(target, serve_port, 0)
    self._RegisterPkgRepository(self._pkg_root, remote_port)

  def __enter__(self):
    self._with_count += 1
    return self

  def __exit__(self, type, value, tb):
    # Allows the repository to delete itself when it leaves the scope of a 'with' block.
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
            "fuchsia-pkg://%s" % _MANAGED_REPO_NAME,
            'root_keys':
            root_keys,
            'mirrors': [{
                "mirror_url": "http://127.0.0.1:%d" % remote_port,
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
        ('pkgctl repo rm fuchsia-pkg://%s; ' +
         'pkgctl repo add url http://127.0.0.1:%d/repo_config.json; ') %
        (_MANAGED_REPO_NAME, remote_port)
    ])
    if return_code != 0:
      raise Exception('Error code %d when running pkgctl repo add.' %
                      return_code)

    rule_template = """'{"version":"1","content":[{"host_match":"fuchsia.com","host_replacement":"%s","path_prefix_match":"/","path_prefix_replacement":"/"}]}'"""
    return_code = self._target.RunCommand([
        ('pkgctl rule replace json %s') % (rule_template % (_MANAGED_REPO_NAME))
    ])
    if return_code != 0:
      raise Exception('Error code %d when running pkgctl rule replace.' %
                      return_code)

  def _UnregisterPkgRepository(self):
    """Unregisters the package repository."""

    logging.debug('Unregistering package repository.')
    self._target.RunCommand(
        ['pkgctl', 'repo', 'rm',
         'fuchsia-pkg://%s' % (_MANAGED_REPO_NAME)])

    # Re-enable 'devhost' repo if it's present. This is useful for devices that
    # were booted with 'fx serve'.
    self._target.RunCommand([
        'pkgctl', 'rule', 'replace', 'json',
        """'{"version":"1","content":[{"host_match":"fuchsia.com","host_replacement":"devhost","path_prefix_match":"/","path_prefix_replacement":"/"}]}'"""
    ],
                            silent=True)


class ExternalPkgRepo(PkgRepo):
  """Publishes packages to a package repository located and served externally
  (ie. located under a Fuchsia build directory and served by "fx serve"."""

  def __init__(self, pkg_root):
    self._pkg_root = pkg_root
    logging.info('Using existing package root: {}'.format(pkg_root))
    logging.info(
        'ATTENTION: This will not start a package server. Please run "fx serve" manually.'
    )

  def GetPath(self):
    return self._pkg_root

  def __enter__(self):
    return self

  def __exit__(self, type, value, tb):
    pass
