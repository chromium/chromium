# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import importlib
import logging
import os
import sys

from common import GetHostArchFromPlatform


def _AddTargetSpecificationArgs(arg_parser):
  """Returns a parser that handles the target type used for the test run."""

  device_args = arg_parser.add_argument_group(
      'target',
      'Arguments specifying the Fuchsia target type. To see a list of '
      'arguments available for a specific target type, specify the desired '
      'target to use and add the --help flag.')
  device_args.add_argument('--target-cpu',
                           default=GetHostArchFromPlatform(),
                           help='GN target_cpu setting for the build. Defaults '
                           'to the same architecture as host cpu.')
  device_args.add_argument('--device',
                           default=None,
                           choices=['aemu', 'qemu', 'device', 'custom'],
                           help='Choose to run on aemu|qemu|device. '
                           'By default, Fuchsia will run on AEMU on x64 '
                           'hosts and QEMU on arm64 hosts. Alternatively, '
                           'setting to custom will require specifying the '
                           'subclass of Target class used via the '
                           '--custom-device-target flag.')
  device_args.add_argument('-d',
                           action='store_const',
                           dest='device',
                           const='device',
                           help='Run on device instead of emulator.')
  device_args.add_argument('--custom-device-target',
                           default=None,
                           help='Specify path to file that contains the '
                           'subclass of Target that will be used. Only '
                           'needed if device specific operations such as '
                           'paving is required.')
  device_args.add_argument('--fuchsia-out-dir',
                           help='Path to a Fuchsia build output directory. '
                           'Setting the GN arg '
                           '"default_fuchsia_build_dir_for_installation" '
                           'will cause it to be passed here.')


def _GetTargetClass(args):
  """Gets the target class to be used for the test run."""

  if args.device == 'custom':
    if not args.custom_device_target:
      raise Exception('--custom-device-target flag must be set when device '
                      'flag set to custom.')
    target_path = args.custom_device_target
  else:
    if not args.device:
      args.device = 'aemu' if args.target_cpu == 'x64' else 'qemu'
    target_path = '%s_target' % args.device

  try:
    loaded_target = importlib.import_module(target_path)
  except ImportError:
    logging.error('Cannot import from %s. Make sure that --ext-device-path '
                  'is pointing to a file containing a target '
                  'module.' % target_path)
    raise
  return loaded_target.GetTargetType()


def AddCommonArgs(arg_parser):
  """Adds command line arguments to |arg_parser| for options which are shared
  across test and executable target types.

  Args:
    arg_parser: an ArgumentParser object."""

  _AddTargetSpecificationArgs(arg_parser)

  # Parse the args used to specify target
  module_args, _ = arg_parser.parse_known_args()

  # Determine the target class and register target specific args.
  target_class = _GetTargetClass(module_args)
  target_class.RegisterArgs(arg_parser)

  package_args = arg_parser.add_argument_group('package', 'Fuchsia Packages')
  package_args.add_argument(
      '--package',
      action='append',
      help='Paths of the packages to install, including '
      'all dependencies.')
  package_args.add_argument(
      '--package-name',
      help='Name of the package to execute, defined in ' + 'package metadata.')

  common_args = arg_parser.add_argument_group('common', 'Common arguments')
  common_args.add_argument('--runner-logs-dir',
                           help='Directory to write test runner logs to.')
  common_args.add_argument('--exclude-system-logs',
                           action='store_false',
                           dest='include_system_logs',
                           help='Do not show system log data.')
  common_args.add_argument('--verbose', '-v', default=False,
                           action='store_true',
                           help='Enable debug-level logging.')


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


# TODO(crbug.com/1121763): remove the need for additional_args
def GetDeploymentTargetForArgs(additional_args=None):
  """Constructs a deployment target object using command line arguments.
     If needed, an additional_args dict can be used to supplement the
     command line arguments."""

  # Determine target type from command line arguments.
  device_type_parser = argparse.ArgumentParser()
  _AddTargetSpecificationArgs(device_type_parser)
  module_args, _ = device_type_parser.parse_known_args()
  target_class = _GetTargetClass(module_args)

  # Process command line args needed to initialize target in separate arg
  # parser.
  target_arg_parser = argparse.ArgumentParser()
  target_class.RegisterArgs(target_arg_parser)
  known_args, _ = target_arg_parser.parse_known_args()
  target_args = vars(known_args)

  # target_cpu is needed to determine target type, and fuchsia_out_dir
  # is needed for devices with Fuchsia built from source code.
  target_args.update({'target_cpu': module_args.target_cpu})
  target_args.update({'fuchsia_out_dir': module_args.fuchsia_out_dir})

  if additional_args:
    target_args.update(additional_args)

  return target_class(**target_args)
