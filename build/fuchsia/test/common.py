# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common methods and variables used by Cr-Fuchsia testing infrastructure."""

import enum
import json
import logging
import os
import re
import signal
import shutil
import subprocess
import sys
import time

from argparse import ArgumentParser
from typing import Iterable, List, Optional, Tuple

from compatible_utils import get_ssh_prefix, get_host_arch

DIR_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
IMAGES_ROOT = os.path.join(DIR_SRC_ROOT, 'third_party', 'fuchsia-sdk',
                           'images')
REPO_ALIAS = 'fuchsia.com'
SDK_ROOT = os.path.join(DIR_SRC_ROOT, 'third_party', 'fuchsia-sdk', 'sdk')
SDK_TOOLS_DIR = os.path.join(SDK_ROOT, 'tools', get_host_arch())
_FFX_TOOL = os.path.join(SDK_TOOLS_DIR, 'ffx')

# This global variable is used to set the environment variable
# |FFX_ISOLATE_DIR| when running ffx commands in E2E testing scripts.
_FFX_ISOLATE_DIR = None


class TargetState(enum.Enum):
    """State of a target."""
    UNKNOWN = enum.auto()
    DISCONNECTED = enum.auto()
    PRODUCT = enum.auto()
    FASTBOOT = enum.auto()
    ZEDBOOT = enum.auto()


class BootMode(enum.Enum):
    """Specifies boot mode for device."""
    REGULAR = enum.auto()
    RECOVERY = enum.auto()
    BOOTLOADER = enum.auto()


_STATE_TO_BOOTMODE = {
    TargetState.PRODUCT: BootMode.REGULAR,
    TargetState.FASTBOOT: BootMode.BOOTLOADER,
    TargetState.ZEDBOOT: BootMode.RECOVERY
}

_BOOTMODE_TO_STATE = {value: key for key, value in _STATE_TO_BOOTMODE.items()}


class StateNotFoundError(Exception):
    """Raised when target's state cannot be found."""


class StateTransitionError(Exception):
    """Raised when target does not transition to desired state."""


def _state_string_to_state(state_str: str) -> TargetState:
    state_str = state_str.strip().lower()
    if state_str == 'product':
        return TargetState.PRODUCT
    if state_str == 'zedboot (r)':
        return TargetState.ZEDBOOT
    if state_str == 'fastboot':
        return TargetState.FASTBOOT
    if state_str == 'unknown':
        return TargetState.UNKNOWN
    if state_str == 'disconnected':
        return TargetState.DISCONNECTED

    raise NotImplementedError(f'State {state_str} not supported')


def get_target_state(target_id: Optional[str],
                     serial_num: Optional[str],
                     num_attempts: int = 1) -> TargetState:
    """Return state of target or the default target.

    Args:
        target_id: Optional nodename of the target. If not given, default target
        is used.
        serial_num: Optional serial number of target. Only usable if device is
        in fastboot.
        num_attempts: Optional number of times to attempt getting status.

    Returns:
        TargetState of the given node, if found.

    Raises:
        StateNotFoundError: If target cannot be found, or default target is not
            defined if |target_id| is not given.
    """
    for i in range(num_attempts):
        targets = json.loads(
            run_ffx_command(('target', 'list'),
                            check=True,
                            capture_output=True,
                            json_out=True).stdout.strip())
        for target in targets:
            if target_id is None and target['is_default']:
                return _state_string_to_state(target['target_state'])
            if target_id == target['nodename']:
                return _state_string_to_state(target['target_state'])
            if serial_num == target['serial']:
                # Should only return Fastboot.
                return _state_string_to_state(target['target_state'])
        # Do not sleep for last attempt.
        if i < num_attempts - 1:
            time.sleep(10)

    # Could not find a state for given target.
    error_target = target_id
    if target_id is None:
        error_target = 'default target'

    raise StateNotFoundError(f'Could not find state for {error_target}.')


def set_ffx_isolate_dir(isolate_dir: str) -> None:
    """Overwrites |_FFX_ISOLATE_DIR|."""

    global _FFX_ISOLATE_DIR  # pylint: disable=global-statement
    _FFX_ISOLATE_DIR = isolate_dir


def get_host_tool_path(tool):
    """Get a tool from the SDK."""

    return os.path.join(SDK_TOOLS_DIR, tool)


def get_host_os():
    """Get host operating system."""

    host_platform = sys.platform
    if host_platform.startswith('linux'):
        return 'linux'
    if host_platform.startswith('darwin'):
        return 'mac'
    raise Exception('Unsupported host platform: %s' % host_platform)


def make_clean_directory(directory_name):
    """If the directory exists, delete it and remake with no contents."""

    if os.path.exists(directory_name):
        shutil.rmtree(directory_name)
    os.mkdir(directory_name)


def _get_daemon_status():
    """Determines daemon status via `ffx daemon socket`.

    Returns:
      dict of status of the socket. Status will have a key Running or
      NotRunning to indicate if the daemon is running.
    """
    status = json.loads(
        run_ffx_command(('daemon', 'socket'),
                        check=True,
                        capture_output=True,
                        json_out=True,
                        suppress_repair=True).stdout.strip())
    return status.get('pid', {}).get('status', {'NotRunning': True})


def _is_daemon_running():
    return 'Running' in _get_daemon_status()


def check_ssh_config_file() -> None:
    """Checks for ssh keys and generates them if they are missing."""

    script_path = os.path.join(SDK_ROOT, 'bin', 'fuchsia-common.sh')
    check_cmd = ['bash', '-c', f'. {script_path}; check-fuchsia-ssh-config']
    subprocess.run(check_cmd, check=True)


def _wait_for_daemon(start=True, timeout_seconds=100):
    """Waits for daemon to reach desired state in a polling loop.

    Sleeps for 5s between polls.

    Args:
      start: bool. Indicates to wait for daemon to start up. If False,
        indicates waiting for daemon to die.
      timeout_seconds: int. Number of seconds to wait for the daemon to reach
        the desired status.
    Raises:
      TimeoutError: if the daemon does not reach the desired state in time.
    """
    wanted_status = 'start' if start else 'stop'
    sleep_period_seconds = 5
    attempts = int(timeout_seconds / sleep_period_seconds)
    for i in range(attempts):
        if _is_daemon_running() == start:
            return
        if i != attempts:
            logging.info('Waiting for daemon to %s...', wanted_status)
            time.sleep(sleep_period_seconds)

    raise TimeoutError(f'Daemon did not {wanted_status} in time.')


def _run_repair_command(output):
    """Scans |output| for a self-repair command to run and, if found, runs it.

    Returns:
      True if a repair command was found and ran successfully. False otherwise.
    """
    # Check for a string along the lines of:
    # "Run `ffx doctor --restart-daemon` for further diagnostics."
    match = re.search('`ffx ([^`]+)`', output)
    if not match or len(match.groups()) != 1:
        return False  # No repair command found.
    args = match.groups()[0].split()

    try:
        run_ffx_command(args, suppress_repair=True)
        # Need the daemon to be up at the end of this.
        _wait_for_daemon(start=True)
    except subprocess.CalledProcessError:
        return False  # Repair failed.
    return True  # Repair succeeded.


# The following two functions are the temporary work around before
# https://fxbug.dev/92296 and https://fxbug.dev/125873 are being fixed.
def start_ffx_daemon():
    """Starts the ffx daemon by using doctor --restart-daemon since daemon start
    blocks the current shell.

    Note, doctor --restart-daemon usually fails since the timeout in ffx is
    short and won't be sufficient to wait for the daemon to really start.

    Also, doctor --restart-daemon always restarts the daemon, so this function
    should be used with caution unless it's really needed to "restart" the
    daemon by explicitly calling stop daemon first.
    """
    assert not _is_daemon_running(), "Call stop_ffx_daemon first."
    run_ffx_command(('doctor', '--restart-daemon'), check=False)
    _wait_for_daemon(start=True)


def stop_ffx_daemon():
    """Stops the ffx daemon"""
    run_ffx_command(('daemon', 'stop'))
    _wait_for_daemon(start=False)


def run_ffx_command(cmd: Iterable[str],
                    target_id: Optional[str] = None,
                    check: bool = True,
                    suppress_repair: bool = False,
                    configs: Optional[List[str]] = None,
                    json_out: bool = False,
                    **kwargs) -> subprocess.CompletedProcess:
    """Runs `ffx` with the given arguments, waiting for it to exit.

    If `ffx` exits with a non-zero exit code, the output is scanned for a
    recommended repair command (e.g., "Run `ffx doctor --restart-daemon` for
    further diagnostics."). If such a command is found, it is run and then the
    original command is retried. This behavior can be suppressed via the
    `suppress_repair` argument.

    Args:
        cmd: A sequence of arguments to ffx.
        target_id: Whether to execute the command for a specific target. The
            target_id could be in the form of a nodename or an address.
        check: If True, CalledProcessError is raised if ffx returns a non-zero
            exit code.
        suppress_repair: If True, do not attempt to find and run a repair
            command.
        configs: A list of configs to be applied to the current command.
        json_out: Have command output returned as JSON. Must be parsed by
            caller.
    Returns:
        A CompletedProcess instance
    Raises:
        CalledProcessError if |check| is true.
    """

    ffx_cmd = [_FFX_TOOL]
    if json_out:
        ffx_cmd.extend(('--machine', 'json'))
    if target_id:
        ffx_cmd.extend(('--target', target_id))
    if configs:
        for config in configs:
            ffx_cmd.extend(('--config', config))
    ffx_cmd.extend(cmd)
    env = os.environ
    if _FFX_ISOLATE_DIR:
        env['FFX_ISOLATE_DIR'] = _FFX_ISOLATE_DIR

    try:
        if not suppress_repair:
            # If we want to repair, we need to capture output in STDOUT and
            # STDERR. This could conflict with expectations of the caller.
            output_captured = kwargs.get('capture_output') or (
                kwargs.get('stdout') and kwargs.get('stderr'))
            if not output_captured:
                # Force output to combine into STDOUT.
                kwargs['stdout'] = subprocess.PIPE
                kwargs['stderr'] = subprocess.STDOUT
        return subprocess.run(ffx_cmd,
                              check=check,
                              encoding='utf-8',
                              env=env,
                              **kwargs)
    except subprocess.CalledProcessError as cpe:
        logging.error('%s %s failed with returncode %s.',
                      os.path.relpath(_FFX_TOOL),
                      subprocess.list2cmdline(ffx_cmd[1:]), cpe.returncode)
        if cpe.output:
            logging.error('stdout of the command: %s', cpe.output)
        if suppress_repair or (cpe.output
                               and not _run_repair_command(cpe.output)):
            raise

    # If the original command failed but a repair command was found and
    # succeeded, try one more time with the original command.
    return run_ffx_command(cmd, target_id, check, True, configs, json_out,
                           **kwargs)


def run_continuous_ffx_command(cmd: Iterable[str],
                               target_id: Optional[str] = None,
                               encoding: Optional[str] = 'utf-8',
                               **kwargs) -> subprocess.Popen:
    """Runs an ffx command asynchronously."""
    ffx_cmd = [_FFX_TOOL]
    if target_id:
        ffx_cmd.extend(('--target', target_id))
    ffx_cmd.extend(cmd)
    return subprocess.Popen(ffx_cmd, encoding=encoding, **kwargs)


def read_package_paths(out_dir: str, pkg_name: str) -> List[str]:
    """
    Returns:
        A list of the absolute path to all FAR files the package depends on.
    """
    with open(
            os.path.join(DIR_SRC_ROOT, out_dir, 'gen', 'package_metadata',
                         f'{pkg_name}.meta')) as meta_file:
        data = json.load(meta_file)
    packages = []
    for package in data['packages']:
        packages.append(os.path.join(DIR_SRC_ROOT, out_dir, package))
    return packages


def register_common_args(parser: ArgumentParser) -> None:
    """Register commonly used arguments."""
    common_args = parser.add_argument_group('common', 'common arguments')
    common_args.add_argument(
        '--out-dir',
        '-C',
        type=os.path.realpath,
        help='Path to the directory in which build files are located. ')


def register_device_args(parser: ArgumentParser) -> None:
    """Register device arguments."""
    device_args = parser.add_argument_group('device', 'device arguments')
    device_args.add_argument('--target-id',
                             default=os.environ.get('FUCHSIA_NODENAME'),
                             help=('Specify the target device. This could be '
                                   'a node-name (e.g. fuchsia-emulator) or an '
                                   'an ip address along with an optional port '
                                   '(e.g. [fe80::e1c4:fd22:5ee5:878e]:22222, '
                                   '1.2.3.4, 1.2.3.4:33333). If unspecified, '
                                   'the default target in ffx will be used.'))


def register_log_args(parser: ArgumentParser) -> None:
    """Register commonly used arguments."""

    log_args = parser.add_argument_group('logging', 'logging arguments')
    log_args.add_argument('--logs-dir',
                          type=os.path.realpath,
                          help=('Directory to write logs to.'))


def get_component_uri(package: str) -> str:
    """Retrieve the uri for a package."""
    return f'fuchsia-pkg://{REPO_ALIAS}/{package}#meta/{package}.cm'


def resolve_packages(packages: List[str], target_id: Optional[str]) -> None:
    """Ensure that all |packages| are installed on a device."""

    ssh_prefix = get_ssh_prefix(get_ssh_address(target_id))
    subprocess.run(ssh_prefix + ['--', 'pkgctl', 'gc'], check=False)

    for package in packages:
        resolve_cmd = [
            '--', 'pkgctl', 'resolve',
            'fuchsia-pkg://%s/%s' % (REPO_ALIAS, package)
        ]
        retry_command(ssh_prefix + resolve_cmd)


def retry_command(cmd: List[str], retries: int = 2,
                  **kwargs) -> Optional[subprocess.CompletedProcess]:
    """Helper function for retrying a subprocess.run command."""

    for i in range(retries):
        if i == retries - 1:
            proc = subprocess.run(cmd, **kwargs, check=True)
            return proc
        proc = subprocess.run(cmd, **kwargs, check=False)
        if proc.returncode == 0:
            return proc
        time.sleep(3)
    return None


def get_ssh_address(target_id: Optional[str]) -> str:
    """Determines SSH address for given target."""
    return run_ffx_command(('target', 'get-ssh-address'),
                           target_id,
                           capture_output=True).stdout.strip()


def find_in_dir(target_name: str, parent_dir: str) -> Optional[str]:
    """Finds path in SDK.

    Args:
      target_name: Name of target to find, as a string.
      parent_dir: Directory to start search in.

    Returns:
      Full path to the target, None if not found.
    """
    # Doesn't make sense to look for a full path. Only extract the basename.
    target_name = os.path.basename(target_name)
    for root, dirs, _ in os.walk(parent_dir):
        if target_name in dirs:
            return os.path.abspath(os.path.join(root, target_name))

    return None


def find_image_in_sdk(product_name: str) -> Optional[str]:
    """Finds image dir in SDK for product given.

    Args:
      product_name: Name of product's image directory to find.

    Returns:
      Full path to the target, None if not found.
    """
    top_image_dir = os.path.join(SDK_ROOT, os.pardir, 'images')
    path = find_in_dir(product_name, parent_dir=top_image_dir)
    if path:
        return find_in_dir('images', parent_dir=path)
    return path


def catch_sigterm() -> None:
    """Catches the kill signal and allows the process to exit cleanly."""
    def _sigterm_handler(*_):
        sys.exit(0)

    signal.signal(signal.SIGTERM, _sigterm_handler)


def get_system_info(target: Optional[str] = None) -> Tuple[str, str]:
    """Retrieves installed OS version frm device.

    Returns:
        Tuple of strings, containing {product, version number), or a pair of
        empty strings to indicate an error.
    """
    info_cmd = run_ffx_command(('target', 'show', '--json'),
                               target_id=target,
                               capture_output=True,
                               check=False)
    if info_cmd.returncode == 0:
        info_json = json.loads(info_cmd.stdout.strip())
        for info in info_json:
            if info['title'] == 'Build':
                return (info['child'][1]['value'], info['child'][0]['value'])

    # If the information was not retrieved, return empty strings to indicate
    # unknown system info.
    return ('', '')


def boot_device(target_id: Optional[str],
                mode: BootMode,
                serial_num: Optional[str] = None,
                must_boot: bool = False) -> None:
    """Boot device into desired mode, with fallback to SSH on failure.

    Args:
        target_id: Optional target_id of device.
        mode: Desired boot mode.
        must_boot: Forces device to boot, regardless of current state.
    Raises:
        StateTransitionError: When final state of device is not desired.
    """
    # Skip boot call if already in the state and not skipping check.
    state = get_target_state(target_id, serial_num, num_attempts=3)
    wanted_state = _BOOTMODE_TO_STATE.get(mode)
    if not must_boot:
        logging.debug('Current state %s. Want state %s', str(state),
                      str(wanted_state))
        must_boot = state != wanted_state

    if not must_boot:
        logging.debug('Skipping boot - already in good state')
        return

    def _reboot(reboot_cmd, current_state: TargetState):
        reboot_cmd()
        local_state = None
        # Check that we transition out of current state.
        for _ in range(30):
            try:
                local_state = get_target_state(target_id, serial_num)
                if local_state != current_state:
                    # Changed states - can continue
                    break
            except StateNotFoundError:
                logging.debug('Device disconnected...')
                if current_state != TargetState.DISCONNECTED:
                    # Changed states - can continue
                    break
            finally:
                time.sleep(2)
        else:
            logging.warning(
                'Device did not change from initial state. Exiting early')
            return local_state or TargetState.DISCONNECTED

        # Now we want to transition to the new state.
        for _ in range(90):
            try:
                local_state = get_target_state(target_id, serial_num)
                if local_state == wanted_state:
                    return local_state
            except StateNotFoundError:
                logging.warning('Could not find target state.'
                                ' Sleeping then retrying...')
            finally:
                time.sleep(2)
        return local_state or TargetState.DISCONNECTED

    state = _reboot(
        (lambda: _boot_device_ffx(target_id, serial_num, state, mode)), state)

    if state == TargetState.DISCONNECTED:
        raise StateNotFoundError('Target could not be found!')

    if state == wanted_state:
        return

    logging.warning(
        'Booting with FFX to %s did not succeed. Attempting with DM', mode)

    # Fallback to SSH, with no retry if we tried with ffx.:
    state = _reboot(
        (lambda: _boot_device_dm(target_id, serial_num, state, mode)), state)

    if state != wanted_state:
        raise StateTransitionError(
            f'Could not get device to desired state. Wanted {wanted_state},'
            f' got {state}')
    logging.debug('Got desired state: %s', state)


def _boot_device_ffx(target_id: Optional[str], serial_num: Optional[str],
                     current_state: TargetState, mode: BootMode):
    cmd = ['target', 'reboot']
    if mode == BootMode.REGULAR:
        logging.info('Triggering regular boot')
    elif mode == BootMode.RECOVERY:
        cmd.append('-r')
    elif mode == BootMode.BOOTLOADER:
        cmd.append('-b')
    else:
        raise NotImplementedError(f'BootMode {mode} not supported')

    logging.debug('FFX reboot with command [%s]', ' '.join(cmd))
    if current_state == TargetState.FASTBOOT:
        run_ffx_command(cmd,
                        target_id=serial_num,
                        configs = ['product.reboot.use_dm=true'],
                        check=False)
    else:
        run_ffx_command(cmd,
                        target_id=target_id,
                        configs = ['product.reboot.use_dm=true'],
                        check=False)


def _boot_device_dm(target_id: Optional[str], serial_num: Optional[str],
                    current_state: TargetState, mode: BootMode):
    # Can only use DM if device is in regular boot.
    if current_state != TargetState.PRODUCT:
        if mode == BootMode.REGULAR:
            raise StateTransitionError('Cannot boot to Regular via DM - '
                                       'FFX already failed to do so.')
        # Boot to regular.
        _boot_device_ffx(target_id, serial_num, current_state,
                         BootMode.REGULAR)

    ssh_prefix = get_ssh_prefix(get_ssh_address(target_id))

    reboot_cmd = None

    if mode == BootMode.REGULAR:
        reboot_cmd = 'reboot'
    elif mode == BootMode.RECOVERY:
        reboot_cmd = 'reboot-recovery'
    elif mode == BootMode.BOOTLOADER:
        reboot_cmd = 'reboot-bootloader'
    else:
        raise NotImplementedError(f'BootMode {mode} not supported')

    # Boot commands can fail due to SSH connections timeout.
    full_cmd = ssh_prefix + ['--', 'dm', reboot_cmd]
    logging.debug('DM reboot with command [%s]', ' '.join(full_cmd))
    subprocess.run(full_cmd, check=False)
