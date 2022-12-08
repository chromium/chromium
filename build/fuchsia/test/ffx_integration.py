# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provide helpers for running Fuchsia's `ffx`."""

import ast
import logging
import os
import json
import random
import subprocess
import sys
import tempfile

from contextlib import AbstractContextManager
from typing import Iterable, Optional

from common import check_ssh_config_file, run_ffx_command, \
                   run_continuous_ffx_command, SDK_ROOT
from compatible_utils import get_host_arch

_EMU_COMMAND_RETRIES = 3
RUN_SUMMARY_SCHEMA = \
    'https://fuchsia.dev/schema/ffx_test/run_summary-8d1dd964.json'


def get_config(name: str) -> Optional[str]:
    """Run a ffx config get command to retrieve the config value."""

    try:
        return run_ffx_command(['config', 'get', name],
                               capture_output=True).stdout.strip()
    except subprocess.CalledProcessError as cpe:
        # A return code of 2 indicates no previous value set.
        if cpe.returncode == 2:
            return None
        raise


class ScopedFfxConfig(AbstractContextManager):
    """Temporarily overrides `ffx` configuration. Restores the previous value
    upon exit."""

    def __init__(self, name: str, value: str) -> None:
        """
        Args:
            name: The name of the property to set.
            value: The value to associate with `name`.
        """
        self._old_value = None
        self._new_value = value
        self._name = name

    def __enter__(self):
        """Override the configuration."""

        # Cache the old value.
        self._old_value = get_config(self._name)
        if self._new_value != self._old_value:
            run_ffx_command(['config', 'set', self._name, self._new_value])
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        if self._new_value != self._old_value:
            run_ffx_command(['config', 'remove', self._name])
            if self._old_value is not None:
                # Explicitly set the value back only if removing the new value
                # doesn't already restore the old value.
                if  self._old_value != \
                    run_ffx_command(['config', 'get', self._name],
                                    capture_output=True).stdout.strip():
                    run_ffx_command(
                        ['config', 'set', self._name, self._old_value])

        # Do not suppress exceptions.
        return False


def test_connection(target_id: Optional[str]) -> None:
    """Run an echo test to verify that the device can be connected to."""

    run_ffx_command(('target', 'echo'), target_id)


class FfxEmulator(AbstractContextManager):
    """A helper for managing emulators."""

    def __init__(self,
                 enable_graphics: bool,
                 hardware_gpu: bool,
                 product_bundle: Optional[str],
                 with_network: bool,
                 logs_dir: Optional[str] = None) -> None:
        if product_bundle:
            self._product_bundle = product_bundle
        else:
            target_cpu = get_host_arch()
            self._product_bundle = f'terminal.qemu-{target_cpu}'

        self._enable_graphics = enable_graphics
        self._hardware_gpu = hardware_gpu
        self._logs_dir = logs_dir
        self._with_network = with_network
        node_name_suffix = random.randint(1, 9999)
        self._node_name = f'fuchsia-emulator-{node_name_suffix}'

        # Set the download path parallel to Fuchsia SDK directory
        # permanently so that scripts can always find the product bundles.
        run_ffx_command(('config', 'set', 'pbms.storage.path',
                         os.path.join(SDK_ROOT, os.pardir, 'images')))

        override_file = os.path.join(os.path.dirname(__file__), os.pardir,
                                     'sdk_override.txt')
        self._scoped_pb_metadata = None
        if os.path.exists(override_file):
            with open(override_file) as f:
                pb_metadata = f.read().split('\n')
                pb_metadata.append('{sdk.root}/*.json')
                self._scoped_pb_metadata = ScopedFfxConfig(
                    'pbms.metadata', json.dumps((pb_metadata)))

    def __enter__(self) -> str:
        """Start the emulator.

        Returns:
            The node name of the emulator.
        """

        if self._scoped_pb_metadata:
            self._scoped_pb_metadata.__enter__()
        check_ssh_config_file()
        emu_command = [
            'emu', 'start', self._product_bundle, '--name', self._node_name
        ]
        if not self._enable_graphics:
            emu_command.append('-H')
        if self._hardware_gpu:
            emu_command.append('--gpu')
        if self._logs_dir:
            emu_command.extend(
                ('-l', os.path.join(self._logs_dir, 'emulator_log')))
        if self._with_network:
            emu_command.extend(('--net', 'tap'))

        # TODO(https://crbug.com/1336776): remove when ffx has native support
        # for starting emulator on arm64 host.
        if get_host_arch() == 'arm64':

            arm64_qemu_dir = os.path.join(SDK_ROOT, 'tools', 'arm64',
                                          'qemu_internal')

            # The arm64 emulator binaries are downloaded separately, so add
            # a symlink to the expected location inside the SDK.
            if not os.path.isdir(arm64_qemu_dir):
                os.symlink(
                    os.path.join(SDK_ROOT, '..', '..', 'qemu-linux-arm64'),
                    arm64_qemu_dir)

            # Add the arm64 emulator binaries to the SDK's manifest.json file.
            sdk_manifest = os.path.join(SDK_ROOT, 'meta', 'manifest.json')
            with open(sdk_manifest, 'r+') as f:
                data = json.load(f)
                for part in data['parts']:
                    if part['meta'] == 'tools/x64/qemu_internal-meta.json':
                        part['meta'] = 'tools/arm64/qemu_internal-meta.json'
                        break
                f.seek(0)
                json.dump(data, f)
                f.truncate()

            # Generate a meta file for the arm64 emulator binaries using its
            # x64 counterpart.
            qemu_arm64_meta_file = os.path.join(SDK_ROOT, 'tools', 'arm64',
                                                'qemu_internal-meta.json')
            qemu_x64_meta_file = os.path.join(SDK_ROOT, 'tools', 'x64',
                                              'qemu_internal-meta.json')
            with open(qemu_x64_meta_file) as f:
                data = str(json.load(f))
            qemu_arm64_meta = data.replace(r'tools/x64', 'tools/arm64')
            with open(qemu_arm64_meta_file, "w+") as f:
                json.dump(ast.literal_eval(qemu_arm64_meta), f)
            emu_command.extend(['--engine', 'qemu'])

        with ScopedFfxConfig('emu.start.timeout', '90'):
            for _ in range(_EMU_COMMAND_RETRIES):

                # If the ffx daemon fails to establish a connection with
                # the emulator after 85 seconds, that means the emulator
                # failed to be brought up and a retry is needed.
                # TODO(fxb/103540): Remove retry when start up issue is fixed.
                try:
                    run_ffx_command(emu_command, timeout=85)
                    break
                except (subprocess.TimeoutExpired,
                        subprocess.CalledProcessError):
                    run_ffx_command(('emu', 'stop'))
        return self._node_name

    def __exit__(self, exc_type, exc_value, traceback) -> bool:
        """Shutdown the emulator."""

        # The emulator might have shut down unexpectedly, so this command
        # might fail.
        run_ffx_command(('emu', 'stop', self._node_name), check=False)

        if self._scoped_pb_metadata:
            self._scoped_pb_metadata.__exit__(exc_type, exc_value, traceback)

        # Do not suppress exceptions.
        return False


class FfxTestRunner(AbstractContextManager):
    """A context manager that manages a session for running a test via `ffx`.

    Upon entry, an instance of this class configures `ffx` to retrieve files
    generated by a test and prepares a directory to hold these files either in a
    specified directory or in tmp. On exit, any previous configuration of
    `ffx` is restored and the temporary directory, if used, is deleted.

    The prepared directory is used when invoking `ffx test run`.
    """

    def __init__(self, results_dir: Optional[str] = None) -> None:
        """
        Args:
            results_dir: Directory on the host where results should be stored.
        """
        self._results_dir = results_dir
        self._custom_artifact_directory = None
        self._temp_results_dir = None
        self._debug_data_directory = None

    def __enter__(self):
        if self._results_dir:
            os.makedirs(self._results_dir, exist_ok=True)
        else:
            self._temp_results_dir = tempfile.TemporaryDirectory()
            self._results_dir = self._temp_results_dir.__enter__()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        if self._temp_results_dir:
            self._temp_results_dir.__exit__(exc_type, exc_val, exc_tb)
            self._temp_results_dir = None

        # Do not suppress exceptions.
        return False

    def run_test(self,
                 component_uri: str,
                 test_args: Optional[Iterable[str]] = None,
                 node_name: Optional[str] = None) -> subprocess.Popen:
        """Starts a subprocess to run a test on a target.
        Args:
            component_uri: The test component URI.
            test_args: Arguments to the test package, if any.
            node_name: The target on which to run the test.
        Returns:
            A subprocess.Popen object.
        """
        command = [
            'test', 'run', '--output-directory', self._results_dir,
            component_uri
        ]
        if test_args:
            command.append('--')
            command.extend(test_args)
        return run_continuous_ffx_command(command,
                                          node_name,
                                          stdout=subprocess.PIPE,
                                          stderr=subprocess.STDOUT)

    def _parse_test_outputs(self):
        """Parses the output files generated by the test runner.

        The instance's `_custom_artifact_directory` member is set to the
        directory holding output files emitted by the test.

        This function is idempotent, and performs no work if it has already been
        called.
        """
        if self._custom_artifact_directory:
            return

        run_summary_path = os.path.join(self._results_dir, 'run_summary.json')
        try:
            with open(run_summary_path) as run_summary_file:
                run_summary = json.load(run_summary_file)
        except IOError:
            logging.exception('Error reading run summary file.')
            return
        except ValueError:
            logging.exception('Error parsing run summary file %s',
                              run_summary_path)
            return

        assert run_summary['schema_id'] == RUN_SUMMARY_SCHEMA, \
            'Unsupported version found in %s' % run_summary_path

        run_artifact_dir = run_summary.get('data', {})['artifact_dir']
        for artifact_path, artifact in run_summary.get(
                'data', {})['artifacts'].items():
            if artifact['artifact_type'] == 'DEBUG':
                self._debug_data_directory = os.path.join(
                    self._results_dir, run_artifact_dir, artifact_path)
                break

        if run_summary['data']['outcome'] == "NOT_STARTED":
            logging.critical('Test execution was interrupted. Either the '
                             'emulator crashed while the tests were still '
                             'running or connection to the device was lost.')
            sys.exit(1)

        # There should be precisely one suite for the test that ran.
        suites_list = run_summary.get('data', {}).get('suites')
        if not suites_list:
            logging.error('Missing or empty list of suites in %s',
                          run_summary_path)
            return
        suite_summary = suites_list[0]

        # Get the top-level directory holding all artifacts for this suite.
        artifact_dir = suite_summary.get('artifact_dir')
        if not artifact_dir:
            logging.error('Failed to find suite\'s artifact_dir in %s',
                          run_summary_path)
            return

        # Get the path corresponding to artifacts
        for artifact_path, artifact in suite_summary['artifacts'].items():
            if artifact['artifact_type'] == 'CUSTOM':
                self._custom_artifact_directory = os.path.join(
                    self._results_dir, artifact_dir, artifact_path)
                break

    def get_custom_artifact_directory(self) -> str:
        """Returns the full path to the directory holding custom artifacts
        emitted by the test or None if the directory could not be discovered.
        """
        self._parse_test_outputs()
        return self._custom_artifact_directory

    def get_debug_data_directory(self):
        """Returns the full path to the directory holding debug data
        emitted by the test, or None if the path cannot be determined.
        """
        self._parse_test_outputs()
        return self._debug_data_directory
