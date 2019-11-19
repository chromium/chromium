# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements commands for running and interacting with Fuchsia on devices."""

from __future__ import print_function

import amber_repo
import boot_data
import filecmp
import logging
import os
import re
import subprocess
import sys
import target
import tempfile
import time
import uuid

from common import SDK_ROOT, EnsurePathExists, GetHostToolPathFromPlatform

# The maximum times to attempt mDNS resolution when connecting to a freshly
# booted Fuchsia instance before aborting.
_BOOT_DISCOVERY_ATTEMPTS = 30

# Number of seconds to wait when querying a list of all devices over mDNS.
_LIST_DEVICES_TIMEOUT_SECS = 3

#Number of failed connection attempts before redirecting system logs to stdout.
CONNECT_RETRY_COUNT_BEFORE_LOGGING = 10

TARGET_HASH_FILE_PATH = '/data/.hash'

class DeviceTarget(target.Target):
  """Prepares a device to be used as a deployment target. Depending on the
  command line parameters, it automatically handling a number of preparatory
  steps relating to address resolution, device provisioning, and SDK
  versioning.

  If |_node_name| is unset:
    If there is one running device, use it for deployment and execution. The
    device's SDK version is checked unless --os-check=ignore is set.
    If --os-check=update is set, then the target device is repaved if the SDK
    version doesn't match.

    If there are more than one running devices, then abort and instruct the
    user to re-run the command with |_node_name|

    Otherwise, if there are no running devices, then search for a device
    running Zedboot, and pave it.


  If |_node_name| is set:
    If there is a running device with a matching nodename, then it is used
    for deployment and execution.

    Otherwise, attempt to pave a device with a matching nodename, and use it
    for deployment and execution.

  If |_host| is set:
    Deploy to a device at the host IP address as-is."""

  def __init__(self, output_dir, target_cpu, host=None, node_name=None,
               port=None, ssh_config=None, fuchsia_out_dir=None,
               os_check='update', system_log_file=None):
    """output_dir: The directory which will contain the files that are
                   generated to support the deployment.
    target_cpu: The CPU architecture of the deployment target. Can be
                "x64" or "arm64".
    host: The address of the deployment target device.
    node_name: The node name of the deployment target device.
    port: The port of the SSH service on the deployment target device.
    ssh_config: The path to SSH configuration data.
    fuchsia_out_dir: The path to a Fuchsia build output directory, for
                     deployments to devices paved with local Fuchsia builds.
    os_check: If 'check', the target's SDK version must match.
              If 'update', the target will be repaved if the SDK versions
                  mismatch.
              If 'ignore', the target's SDK version is ignored."""

    super(DeviceTarget, self).__init__(output_dir, target_cpu)

    self._port = port if port else 22
    self._system_log_file = system_log_file
    self._loglistener = None
    self._host = host
    self._fuchsia_out_dir = os.path.expanduser(fuchsia_out_dir)
    self._node_name = node_name
    self._os_check = os_check,

    if self._host and self._node_name:
      raise Exception('Only one of "--host" or "--name" can be specified.')

    if self._fuchsia_out_dir:
      if ssh_config:
        raise Exception('Only one of "--fuchsia-out-dir" or "--ssh_config" can '
                        'be specified.')

      # Use SSH keys from the Fuchsia output directory.
      self._ssh_config_path = os.path.join(self._fuchsia_out_dir, 'ssh-keys',
                                           'ssh_config')
      self._os_check = 'ignore'

    elif ssh_config:
      # Use the SSH config provided via the commandline.
      self._ssh_config_path = os.path.expanduser(ssh_config)

    else:
      # Default to using an automatically generated SSH config and keys.
      boot_data.ProvisionSSH(output_dir)
      self._ssh_config_path = boot_data.GetSSHConfigPath(output_dir)

  def __exit__(self, exc_type, exc_val, exc_tb):
    if self._loglistener:
      self._loglistener.kill()

  def _SDKHashMatches(self):
    """Checks if /data/.hash on the device matches SDK_ROOT/.hash.

    Returns True if the files are identical, or False otherwise.
    """
    with tempfile.NamedTemporaryFile() as tmp:
      try:
        self.GetFile(TARGET_HASH_FILE_PATH, tmp.name)
      except subprocess.CalledProcessError:
        # If the file is unretrievable for whatever reason, assume mismatch.
        return False

      return filecmp.cmp(tmp.name, os.path.join(SDK_ROOT, '.hash'), False)

  def __Discover(self):
    """Queries mDNS for the IP address of a booted Fuchsia instance whose name
    matches |_node_name| on the local area network. If |_node_name| isn't
    specified, and there is only one device on the network, then returns the
    IP address of that advice.

    Sets |_host_name| and returns True if the device was found,
    or waits up to |timeout| seconds and returns False if the device couldn't
    be found."""

    dev_finder_path = GetHostToolPathFromPlatform('dev_finder')

    if self._node_name:
      command = [dev_finder_path, 'resolve',
                 '-device-limit', '1',  # Exit early as soon as a host is found.
                 self._node_name]
    else:
      command = [dev_finder_path, 'list', '-full',
                 '-timeout', str(_LIST_DEVICES_TIMEOUT_SECS * 1000)]

    proc = subprocess.Popen(command,
                            stdout=subprocess.PIPE,
                            stderr=open(os.devnull, 'w'))

    output = set(proc.communicate()[0].strip().split('\n'))

    if proc.returncode != 0:
      return False

    if self._node_name:
      # Handle the result of "dev_finder resolve".
      self._host = output.pop().strip()

    else:
      name_host_pairs = [x.strip().split(' ') for x in output]

      # Handle the output of "dev_finder list".
      if len(name_host_pairs) > 1:
        print('More than one device was discovered on the network.')
        print('Use --node-name <name> to specify the device to use.')
        print('\nList of devices:')
        for pair in name_host_pairs:
          print('  ' + pair[1])
        print()
        raise Exception('Ambiguous target device specification.')

      assert len(name_host_pairs) == 1
      self._host, self._node_name = name_host_pairs[0]

    logging.info('Found device "%s" at address %s.' % (self._node_name,
                                                       self._host))

    return True

  def Start(self):
    if self._host:
      self._WaitUntilReady()

    else:
      should_provision = False

      if self.__Discover():
        self._WaitUntilReady()

        if self._os_check != 'ignore':
          if self._SDKHashMatches():
            if self._os_check == 'update':
              logging.info( 'SDK hash does not match; rebooting and repaving.')
              self.RunCommand(['dm', 'reboot'])
              should_provision = True
            elif self._os_check == 'check':
              raise Exception('Target device SDK version does not match.')

      else:
        should_provision = True

      if should_provision:
        boot_data.AssertBootImagesExist(self._GetTargetSdkArch(), 'generic')
        self.__ProvisionDevice()

      assert self._node_name
      assert self._host


  def _GetAmberRepo(self):
    if self._fuchsia_out_dir:
      # Deploy to an already-booted device running a local Fuchsia build.
      return amber_repo.ExternalAmberRepo(
          os.path.join(self._fuchsia_out_dir, 'amber-files'))
    else:
      # Pave a Zedbootable device.
      return amber_repo.ManagedAmberRepo(self)

  def __ProvisionDevice(self):
    """Netboots a device with Fuchsia. If |_node_name| is set, then only a
    device with a matching node name is used.

    The device is up and reachable via SSH when the function is successfully
    completes."""

    bootserver_path = GetHostToolPathFromPlatform('bootserver')
    bootserver_command = [
        bootserver_path,
        '-1',
        '--fvm',
        EnsurePathExists(
            boot_data.GetTargetFile('storage-sparse.blk',
                                    self._GetTargetSdkArch(),
                                    boot_data.TARGET_TYPE_GENERIC)),
        EnsurePathExists(boot_data.GetBootImage(self._output_dir,
                                                self._GetTargetSdkArch(),
                                                boot_data.TARGET_TYPE_GENERIC))]

    if self._node_name:
      bootserver_command += ['-n', self._node_name]

    bootserver_command += ['--']
    bootserver_command += boot_data.GetKernelArgs(self._output_dir)

    logging.debug(' '.join(bootserver_command))
    stdout = subprocess.check_output(bootserver_command,
                                     stderr=subprocess.STDOUT)

    # Parse the nodename from bootserver stdout.
    m = re.search(r'.*Proceeding with nodename (?P<nodename>.*)$', stdout,
                  re.MULTILINE)
    if not m:
      raise Exception('Couldn\'t parse nodename from bootserver output.')
    self._node_name = m.groupdict()['nodename']
    logging.info('Booted device "%s".' % self._node_name)

    # Start loglistener to save system logs.
    if self._system_log_file:
      loglistener_path = GetHostToolPathFromPlatform('loglistener')
      self._loglistener = subprocess.Popen(
          [loglistener_path, self._node_name],
          stdout=self._system_log_file,
          stderr=subprocess.STDOUT, stdin=open(os.devnull))

    # Repeatdly query mDNS until we find the device, or we hit the timeout of
    # DISCOVERY_TIMEOUT_SECS.
    logging.info('Waiting for device to join network.')
    for _ in xrange(_BOOT_DISCOVERY_ATTEMPTS):
      if self.__Discover():
        break

    if not self._host:
      raise Exception('Device %s couldn\'t be discovered via mDNS.' %
                      self._node_name)

    self._WaitUntilReady();

    # Update the target's hash to match the current tree's.
    self.PutFile(os.path.join(SDK_ROOT, '.hash'), TARGET_HASH_FILE_PATH)

  def _GetEndpoint(self):
    return (self._host, self._port)

  def _GetSshConfigPath(self):
    return self._ssh_config_path
