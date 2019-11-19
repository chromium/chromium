# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

from aemu_target import AemuTarget
from device_target import DeviceTarget
from qemu_target import QemuTarget

def AddCommonArgs(arg_parser):
  """Adds command line arguments to |arg_parser| for options which are shared
  across test and executable target types."""

  common_args = arg_parser.add_argument_group('common', 'Common arguments')
  common_args.add_argument('--package', action='append', required=True,
                           help='Paths of the packages to install, including '
                                'all dependencies.')
  common_args.add_argument('--package-name', required=True,
                           help='Name of the package to execute, defined in ' +
                                'package metadata.')
  common_args.add_argument('--output-directory',
                           type=os.path.realpath, required=True,
                           help=('Path to the directory in which build files '
                                 'are located (must include build type).'))
  common_args.add_argument('--target-cpu', required=True,
                           help='GN target_cpu setting for the build.')
  common_args.add_argument('--target-staging-path',
                           help='target path under which to stage packages '
                           'during deployment.', default='/data')
  common_args.add_argument('--device', default=None,
                           choices=['aemu','qemu','device'],
                           help='Choose to run on aemu|qemu|device. ' +
                                'By default, Fuchsia will run in QEMU.')
  common_args.add_argument('-d', action='store_const', dest='device',
                           const='device',
                           help='Run on device instead of emulator.')
  common_args.add_argument('--host', help='The IP of the target device. ' +
                           'Optional.')
  common_args.add_argument('--node-name',
                           help='The node-name of the device to boot or deploy '
                                'to. Optional, will use the first discovered '
                                'device if omitted.')
  common_args.add_argument('--port', '-p', type=int, default=22,
                           help='The port of the SSH service running on the ' +
                                'device. Optional.')
  common_args.add_argument('--ssh-config', '-F',
                           help='The path to the SSH configuration used for '
                                'connecting to the target device.')
  common_args.add_argument('--fuchsia-out-dir',
                           help='Path to a Fuchsia build output directory. '
                                'Equivalent to setting --ssh_config and '
                                '---os-check=ignore')
  common_args.add_argument('--system-log-file',
                           help='File to write system logs to. Specify - to '
                                'log to stdout.')
  common_args.add_argument('--exclude-system-logs',
                           action='store_false',
                           dest='include_system_logs',
                           help='Do not show system log data.')
  common_args.add_argument('--verbose', '-v', default=False,
                           action='store_true',
                           help='Enable debug-level logging.')
  common_args.add_argument('--qemu-cpu-cores', type=int, default=4,
                           help='Sets the number of CPU cores to provide if '
                           'launching in a VM.'),
  common_args.add_argument('--memory', type=int, default=2048,
                           help='Sets the RAM size (MB) if launching in a VM'),
  common_args.add_argument('--no-kvm', action='store_true', default=False,
                           help='Disable KVM virtualization'),
  common_args.add_argument(
      '--os_check', choices=['check', 'update', 'ignore'],
      default='update',
      help='Sets the OS version enforcement policy. If \'check\', then the '
           'deployment process will halt if the target\'s version doesn\'t '
           'match. If \'update\', then the target device will automatically '
           'be repaved. If \'ignore\', then the OS version won\'t be checked.')


def ConfigureLogging(args):
  """Configures the logging level based on command line |args|."""

  logging.basicConfig(level=(logging.DEBUG if args.verbose else logging.INFO),
                      format='%(asctime)s:%(levelname)s:%(name)s:%(message)s')

  # The test server spawner is too noisy with INFO level logging, so tweak
  # its verbosity a bit by adjusting its logging level.
  logging.getLogger('chrome_test_server_spawner').setLevel(
      logging.DEBUG if args.verbose else logging.WARN)

  # Verbose SCP output can be useful at times but oftentimes is just too noisy.
  # Only enable it if -vv is passed.
  logging.getLogger('ssh').setLevel(
      logging.DEBUG if args.verbose else logging.WARN)


def GetDeploymentTargetForArgs(args, require_kvm=False):
  """Constructs a deployment target object using parameters taken from
  command line arguments."""
  if args.system_log_file == '-':
    system_log_file = sys.stdout
  elif args.system_log_file:
    system_log_file = open(args.system_log_file, 'w')
  else:
    system_log_file = None

  # Allow fuchsia to run on qemu if device not explicitly chosen.
  if not args.device:
    args.device = 'qemu'

  target_args = { 'output_dir':args.output_directory,
                  'target_cpu':args.target_cpu,
                  'system_log_file':system_log_file }
  if args.device == 'device':
    target_args.update({ 'host':args.host,
                         'node_name':args.node_name,
                         'port':args.port,
                         'ssh_config':args.ssh_config,
                         'fuchsia_out_dir':args.fuchsia_out_dir,
                         'os_check':args.os_check })
    return DeviceTarget(**target_args)
  else:
    target_args.update({ 'cpu_cores':args.qemu_cpu_cores,
                         'require_kvm':not args.no_kvm,
                         'emu_type':args.device,
                         'ram_size_mb':args.memory })
    if args.device == 'qemu':
      return QemuTarget(**target_args)
    else:
      return AemuTarget(**target_args)
