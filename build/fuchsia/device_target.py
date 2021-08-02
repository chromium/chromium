# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements commands for running and interacting with Fuchsia on devices."""

import boot_data
import filecmp
import logging
import os
import pkg_repo
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
BOOT_DISCOVERY_ATTEMPTS = 30

# Number of failed connection attempts before redirecting system logs to stdout.
CONNECT_RETRY_COUNT_BEFORE_LOGGING = 10

# Number of seconds to wait for device discovery.
BOOT_DISCOVERY_TIMEOUT_SECS = 2 * 60

# The timeout limit for one call to the device-finder tool.
_DEVICE_FINDER_TIMEOUT_LIMIT_SECS = \
    BOOT_DISCOVERY_TIMEOUT_SECS / BOOT_DISCOVERY_ATTEMPTS

# Time between a reboot command is issued and when connection attempts from the
# host begin.
_REBOOT_SLEEP_PERIOD = 20


def GetTargetType():
  return DeviceTarget


class DeviceTarget(target.Target):
  """Prepares a device to be used as a deployment target. Depending on the
  command line parameters, it automatically handling a number of preparatory
  steps relating to address resolution.

  If |_node_name| is unset:
    If there is one running device, use it for deployment and execution.

    If there are more than one running devices, then abort and instruct the
    user to re-run the command with |_node_name|

  If |_node_name| is set:
    If there is a running device with a matching nodename, then it is used
    for deployment and execution.

  If |_host| is set:
    Deploy to a device at the host IP address as-is."""

  def __init__(self,
               out_dir,
               target_cpu,
               host=None,
               node_name=None,
               port=None,
               ssh_config=None,
               fuchsia_out_dir=None,
               os_check='update',
               system_log_file=None):
    """out_dir: The directory which will contain the files that are
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

    super(DeviceTarget, self).__init__(out_dir, target_cpu)

    self._system_log_file = system_log_file
    self._host = host
    self._port = port
    self._fuchsia_out_dir = None
    self._node_name = node_name
    self._os_check = os_check
    self._pkg_repo = None

    if self._host and self._node_name:
      raise Exception('Only one of "--host" or "--name" can be specified.')

    if fuchsia_out_dir:
      if ssh_config:
        raise Exception('Only one of "--fuchsia-out-dir" or "--ssh_config" can '
                        'be specified.')

      self._fuchsia_out_dir = os.path.expanduser(fuchsia_out_dir)
      # Use SSH keys from the Fuchsia output directory.
      self._ssh_config_path = os.path.join(self._fuchsia_out_dir, 'ssh-keys',
                                           'ssh_config')
      self._os_check = 'ignore'

    elif ssh_config:
      # Use the SSH config provided via the commandline.
      self._ssh_config_path = os.path.expanduser(ssh_config)

    else:
      # Default to using an automatically generated SSH config and keys.
      boot_data.ProvisionSSH(out_dir)
      self._ssh_config_path = boot_data.GetSSHConfigPath(out_dir)

  @staticmethod
  def CreateFromArgs(args):
    return DeviceTarget(args.out_dir, args.target_cpu, args.host,
                        args.node_name, args.port, args.ssh_config,
                        args.fuchsia_out_dir, args.os_check,
                        args.system_log_file)

  @staticmethod
  def RegisterArgs(arg_parser):
    device_args = arg_parser.add_argument_group(
        'device', 'External device deployment arguments')
    device_args.add_argument('--host',
                             help='The IP of the target device. Optional.')
    device_args.add_argument('--node-name',
                             help='The node-name of the device to boot or '
                             'deploy to. Optional, will use the first '
                             'discovered device if omitted.')
    device_args.add_argument('--port',
                             '-p',
                             type=int,
                             default=None,
                             help='The port of the SSH service running on the '
                             'device. Optional.')
    device_args.add_argument('--ssh-config',
                             '-F',
                             help='The path to the SSH configuration used for '
                             'connecting to the target device.')
    device_args.add_argument(
        '--os-check',
        choices=['check', 'update', 'ignore'],
        default='update',
        help="Sets the OS version enforcement policy. If 'check', then the "
        "deployment process will halt if the target\'s version doesn\'t "
        "match. If 'update', then the target device will automatically "
        "be repaved. If 'ignore', then the OS version won\'t be checked.")

  def _ProvisionDeviceIfNecessary(self):
    if self._Discover():
      self._WaitUntilReady()
    else:
      raise Exception('Could not find device. If the device is connected '
                      'to the host remotely, make sure that --host flag is '
                      'set and that remote serving is set up.')

  def _Discover(self):
    """Queries mDNS for the IP address of a booted Fuchsia instance whose name
    matches |_node_name| on the local area network. If |_node_name| isn't
    specified, and there is only one device on the network, then returns the
    IP address of that advice.

    Sets |_host_name| and returns True if the device was found,
    or waits up to |timeout| seconds and returns False if the device couldn't
    be found."""

    dev_finder_path = GetHostToolPathFromPlatform('device-finder')

    if self._node_name:
      command = [
          dev_finder_path,
          'resolve',
          '-timeout',
          "%ds" % _DEVICE_FINDER_TIMEOUT_LIMIT_SECS,
          '-device-limit',
          '1',  # Exit early as soon as a host is found.
          self._node_name
      ]
    else:
      command = [
          dev_finder_path, 'list', '-full', '-timeout',
          "%ds" % _DEVICE_FINDER_TIMEOUT_LIMIT_SECS
      ]

    proc = subprocess.Popen(command,
                            stdout=subprocess.PIPE,
                            stderr=open(os.devnull, 'w'))

    output = set(proc.communicate()[0].strip().split('\n'))
    if proc.returncode != 0:
      return False

    if self._node_name:
      # Handle the result of "device-finder resolve".
      self._host = output.pop().strip()

    else:
      name_host_pairs = [x.strip().split(' ') for x in output]

      # Handle the output of "device-finder list".
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
      self._ProvisionDeviceIfNecessary()

  def GetPkgRepo(self):
    if not self._pkg_repo:
      if self._fuchsia_out_dir:
        # Deploy to an already-booted device running a local Fuchsia build.
        self._pkg_repo = pkg_repo.ExternalPkgRepo(
            os.path.join(self._fuchsia_out_dir, 'amber-files'))
      else:
        # Create an ephemeral package repository, then start both "pm serve" as
        # well as the bootserver.
        self._pkg_repo = pkg_repo.ManagedPkgRepo(self)

    return self._pkg_repo

  def _ParseNodename(self, output):
    # Parse the nodename from bootserver stdout.
    m = re.search(r'.*Proceeding with nodename (?P<nodename>.*)$', output,
                  re.MULTILINE)
    if not m:
      raise Exception('Couldn\'t parse nodename from bootserver output.')
    self._node_name = m.groupdict()['nodename']
    logging.info('Booted device "%s".' % self._node_name)

    # Repeatdly query mDNS until we find the device, or we hit the timeout of
    # DISCOVERY_TIMEOUT_SECS.
    logging.info('Waiting for device to join network.')
    for _ in xrange(BOOT_DISCOVERY_ATTEMPTS):
      if self._Discover():
        break

    if not self._host:
      raise Exception('Device %s couldn\'t be discovered via mDNS.' %
                      self._node_name)

    self._WaitUntilReady();

  def _GetEndpoint(self):
    return (self._host, self._port)

  def _GetSshConfigPath(self):
    return self._ssh_config_path

  def Restart(self):
    """Restart the device."""

    self.RunCommandPiped('dm reboot')
    time.sleep(_REBOOT_SLEEP_PERIOD)
    self.Start()
