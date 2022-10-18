# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import importlib
import logging
import multiprocessing
import os
import sys

from common import GetHostArchFromPlatform

BUILTIN_TARGET_NAMES = ['qemu', 'device', 'fvdl']


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
                           choices=BUILTIN_TARGET_NAMES + ['custom'],
                           help='Choose to run on fvdl|qemu|device. '
                           'By default, Fuchsia will run on Fvdl on x64 '
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
                           'needed if device specific operations is '
                           'required.')


def _GetPathToBuiltinTarget(target_name):
  return '%s_target' % target_name


def _LoadTargetClass(target_path):
  try:
    loaded_target = importlib.import_module(target_path)
  except ImportError:
    logging.error(
        'Cannot import from %s. Make sure that --custom-device-target '
        'is pointing to a file containing a target '
        'module.' % target_path)
    raise
  return loaded_target.GetTargetType()


def _GetDefaultEmulatedCpuCoreCount():
  # Revise the processor count on arm64, the trybots on arm64 are in
  # dockers and cannot use all processors.
  # For x64, fvdl always assumes hyperthreading is supported by intel
  # processors, but the cpu_count returns the number regarding if the core
  # is a physical one or a hyperthreading one, so the number should be
  # divided by 2 to avoid creating more threads than the processor
  # supports.
  if GetHostArchFromPlatform() == 'x64':
    return max(int(multiprocessing.cpu_count() / 2) - 1, 4)
  return 4


def AddCommonArgs(arg_parser):
  """Adds command line arguments to |arg_parser| for options which are shared
  across test and executable target types.

  Args:
    arg_parser: an ArgumentParser object."""

  common_args = arg_parser.add_argument_group('common', 'Common arguments')
  common_args.add_argument('--logs-dir', help='Directory to write logs to.')
  common_args.add_argument('--verbose',
                           '-v',
                           default=False,
                           action='store_true',
                           help='Enable debug-level logging.')
  common_args.add_argument(
      '--out-dir',
      type=os.path.realpath,
      help=('Path to the directory in which build files are located. '
            'Defaults to current directory.'))
  common_args.add_argument('--fuchsia-out-dir',
                           default=None,
                           help='Path to a Fuchsia build output directory. '
                           'Setting the GN arg '
                           '"default_fuchsia_build_dir_for_installation" '
                           'will cause it to be passed here.')

  package_args = arg_parser.add_argument_group('package', 'Fuchsia Packages')
  package_args.add_argument(
      '--package',
      action='append',
      help='Paths of the packages to install, including '
      'all dependencies.')
  package_args.add_argument(
      '--package-name',
      help='Name of the package to execute, defined in ' + 'package metadata.')

  emu_args = arg_parser.add_argument_group('emu', 'General emulator arguments')
  emu_args.add_argument('--cpu-cores',
                        type=int,
                        default=_GetDefaultEmulatedCpuCoreCount(),
                        help='Sets the number of CPU cores to provide.')
  emu_args.add_argument('--ram-size-mb',
                        type=int,
                        default=8192,
                        help='Sets the emulated RAM size (MB).'),
  emu_args.add_argument('--allow-no-kvm',
                        action='store_false',
                        dest='require_kvm',
                        default=True,
                        help='Do not require KVM acceleration for '
                        'emulators.')


# Register the arguments for all known target types and the optional custom
# target type (specified on the commandline).
def AddTargetSpecificArgs(arg_parser):
  # Parse the minimal set of arguments to determine if custom targets need to
  # be loaded so that their arguments can be registered.
  target_spec_parser = argparse.ArgumentParser(add_help=False)
  _AddTargetSpecificationArgs(target_spec_parser)
  target_spec_args, _ = target_spec_parser.parse_known_args()
  _AddTargetSpecificationArgs(arg_parser)

  for target in BUILTIN_TARGET_NAMES:
    _LoadTargetClass(_GetPathToBuiltinTarget(target)).RegisterArgs(arg_parser)
  if target_spec_args.custom_device_target:
    _LoadTargetClass(
        target_spec_args.custom_device_target).RegisterArgs(arg_parser)


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


def InitializeTargetArgs():
  """Set args for all targets to default values. This is used by test scripts
     that have their own parser but still uses the target classes."""
  parser = argparse.ArgumentParser()
  AddCommonArgs(parser)
  AddTargetSpecificArgs(parser)
  return parser.parse_args([])


def GetDeploymentTargetForArgs(args):
  """Constructs a deployment target object using command line arguments.
     If needed, an additional_args dict can be used to supplement the
     command line arguments."""

  if args.device == 'custom':
    return _LoadTargetClass(args.custom_device_target).CreateFromArgs(args)

  if args.device:
    device = args.device
  else:
    device = 'fvdl' if args.target_cpu == 'x64' else 'qemu'

  return _LoadTargetClass(_GetPathToBuiltinTarget(device)).CreateFromArgs(args)
