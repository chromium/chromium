# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common methods and variables used by Cr-Fuchsia testing infrastructure."""

import ipaddress
import json
import logging
import os
import signal
import shutil
import socket
import subprocess
import sys
import time

from argparse import ArgumentParser
from typing import Iterable, List, Optional, Tuple
from dataclasses import dataclass

from compatible_utils import get_ssh_prefix, get_host_arch


def _find_src_root() -> str:
    """Find the root of the src folder."""
    if os.environ.get('SRC_ROOT'):
        return os.environ['SRC_ROOT']
    return os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                        os.pardir)


# The absolute path of the root folder to work on. It may not always be the
# src folder since there may not be source code at all, but it's expected to
# have folders like third_party/fuchsia-sdk in it.
DIR_SRC_ROOT = os.path.abspath(_find_src_root())


def _find_fuchsia_images_root() -> str:
    """Define the root of the fuchsia images."""
    if os.environ.get('FUCHSIA_IMAGES_ROOT'):
        return os.environ['FUCHSIA_IMAGES_ROOT']
    return os.path.join(DIR_SRC_ROOT, 'third_party', 'fuchsia-sdk', 'images')


IMAGES_ROOT = os.path.abspath(_find_fuchsia_images_root())


def _find_fuchsia_internal_images_root() -> str:
    """Define the root of the fuchsia images."""
    if os.environ.get('FUCHSIA_INTERNAL_IMAGES_ROOT'):
        return os.environ['FUCHSIA_INTERNAL_IMAGES_ROOT']
    return IMAGES_ROOT + '-internal'


INTERNAL_IMAGES_ROOT = os.path.abspath(_find_fuchsia_internal_images_root())

REPO_ALIAS = 'fuchsia.com'


def _find_fuchsia_sdk_root() -> str:
    """Define the root of the fuchsia sdk."""
    if os.environ.get('FUCHSIA_SDK_ROOT'):
        return os.environ['FUCHSIA_SDK_ROOT']
    return os.path.join(DIR_SRC_ROOT, 'third_party', 'fuchsia-sdk', 'sdk')


SDK_ROOT = os.path.abspath(_find_fuchsia_sdk_root())


def _find_fuchsia_gn_sdk_root() -> str:
    """Define the root of the fuchsia sdk."""
    if os.environ.get('FUCHSIA_GN_SDK_ROOT'):
        return os.environ['FUCHSIA_GN_SDK_ROOT']
    return os.path.join(DIR_SRC_ROOT, 'third_party', 'fuchsia-gn-sdk', 'src')


GN_SDK_ROOT = os.path.abspath(_find_fuchsia_gn_sdk_root())

SDK_TOOLS_DIR = os.path.join(SDK_ROOT, 'tools', get_host_arch())
_FFX_TOOL = os.path.join(SDK_TOOLS_DIR, 'ffx')
_FFX_ISOLATE_DIR = 'FFX_ISOLATE_DIR'


def set_ffx_isolate_dir(isolate_dir: str) -> None:
    """Sets the global environment so the following ffx calls will have the
    isolate dir being carried."""
    assert not has_ffx_isolate_dir(), 'The isolate dir is already set.'
    os.environ[_FFX_ISOLATE_DIR] = isolate_dir


def get_ffx_isolate_dir() -> str:
    """Returns the global environment of the isolate dir of ffx. This function
    should only be called after set_ffx_isolate_dir."""
    return os.environ[_FFX_ISOLATE_DIR]


def has_ffx_isolate_dir() -> bool:
    """Returns whether the isolate dir of ffx is set."""
    return _FFX_ISOLATE_DIR in os.environ


def get_hash_from_sdk():
    """Retrieve version info from the SDK."""

    version_file = os.path.join(SDK_ROOT, 'meta', 'manifest.json')
    assert os.path.exists(version_file), \
           'Could not detect version file. Make sure the SDK is downloaded.'
    with open(version_file, 'r') as f:
        return json.load(f)['id']


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
    os.makedirs(directory_name)


def _get_daemon_status():
    """Determines daemon status via `ffx daemon socket`.

    Returns:
      dict of status of the socket. Status will have a key Running or
      NotRunning to indicate if the daemon is running.
    """
    status = json.loads(
        run_ffx_command(cmd=('daemon', 'socket'),
                        capture_output=True,
                        json_out=True).stdout.strip())
    return status.get('pid', {}).get('status', {'NotRunning': True})


def is_daemon_running() -> bool:
    """Returns if the daemon is running."""
    return 'Running' in _get_daemon_status()


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
        if is_daemon_running() == start:
            return
        if i != attempts:
            logging.info('Waiting for daemon to %s...', wanted_status)
            time.sleep(sleep_period_seconds)

    raise TimeoutError(f'Daemon did not {wanted_status} in time.')


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
    assert not is_daemon_running(), "Call stop_ffx_daemon first."
    run_ffx_command(cmd=('doctor', '--restart-daemon'), check=False)
    _wait_for_daemon(start=True)


def stop_ffx_daemon():
    """Stops the ffx daemon"""
    run_ffx_command(cmd=('daemon', 'stop', '-t', '10000'))
    _wait_for_daemon(start=False)


def run_ffx_command(check: bool = True,
                    capture_output: Optional[bool] = None,
                    timeout: Optional[int] = None,
                    **kwargs) -> subprocess.CompletedProcess:
    """Runs `ffx` with the given arguments, waiting for it to exit.

    **
    The arguments below are named after |subprocess.run| arguments. They are
    overloaded to avoid them from being forwarded to |subprocess.Popen|.
    **
    See run_continuous_ffx_command for additional arguments.
    Args:
        check: If True, CalledProcessError is raised if ffx returns a non-zero
            exit code.
        capture_output: Whether to capture both stdout/stderr.
        timeout: Optional timeout (in seconds). Throws TimeoutError if process
            does not complete in timeout period.
    Returns:
        A CompletedProcess instance
    Raises:
        CalledProcessError if |check| is true.
    """
    if capture_output:
        kwargs['stdout'] = subprocess.PIPE
        kwargs['stderr'] = subprocess.PIPE
    proc = None
    try:
        proc = run_continuous_ffx_command(**kwargs)
        stdout, stderr = proc.communicate(input=kwargs.get('stdin'),
                                          timeout=timeout)
        completed_proc = subprocess.CompletedProcess(
            args=proc.args,
            returncode=proc.returncode,
            stdout=stdout,
            stderr=stderr)
        if check:
            completed_proc.check_returncode()
        return completed_proc
    except subprocess.CalledProcessError as cpe:
        logging.error('%s %s failed with returncode %s.',
                      os.path.relpath(_FFX_TOOL),
                      subprocess.list2cmdline(proc.args[1:]), cpe.returncode)
        if cpe.stdout:
            logging.error('stdout of the command: %s', cpe.stdout)
        if cpe.stderr:
            logging.error('stderr or the command: %s', cpe.stderr)
        raise


def run_continuous_ffx_command(cmd: Iterable[str],
                               target_id: Optional[str] = None,
                               configs: Optional[List[str]] = None,
                               json_out: bool = False,
                               encoding: Optional[str] = 'utf-8',
                               **kwargs) -> subprocess.Popen:
    """Runs `ffx` with the given arguments, returning immediately.

    Args:
        cmd: A sequence of arguments to ffx.
        target_id: Whether to execute the command for a specific target. The
            target_id could be in the form of a nodename or an address.
        configs: A list of configs to be applied to the current command.
        json_out: Have command output returned as JSON. Must be parsed by
            caller.
        encoding: Optional, desired encoding for output/stderr pipes.
    Returns:
        A subprocess.Popen instance
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
    # If the input is a full package already, do nothing
    if package.startswith('fuchsia-pkg://'):
        return package
    return f'fuchsia-pkg://{REPO_ALIAS}/{package}#meta/{package}.cm'


def ssh_run(cmd: List[str],
            target_id: Optional[str],
            check=True,
            **kwargs) -> subprocess.CompletedProcess:
    """Runs a command on the |target_id| via ssh."""
    ssh_prefix = get_ssh_prefix(get_ssh_address(target_id))
    return subprocess.run(ssh_prefix + ['--'] + cmd, check=check, **kwargs)


def resolve_packages(packages: List[str], target_id: Optional[str]) -> None:
    """Ensure that all |packages| are installed on a device."""

    ssh_run(['pkgctl', 'gc'], target_id, check=False)

    def _retry_command(cmd: List[str],
                       retries: int = 2,
                       **kwargs) -> Optional[subprocess.CompletedProcess]:
        """Helper function for retrying a subprocess.run command."""

        for i in range(retries):
            if i == retries - 1:
                proc = ssh_run(cmd, **kwargs, check=True)
                return proc
            proc = ssh_run(cmd, **kwargs, check=False)
            if proc.returncode == 0:
                return proc
            time.sleep(3)
        return None

    for package in packages:
        resolve_cmd = [
            'pkgctl', 'resolve',
            'fuchsia-pkg://%s/%s' % (REPO_ALIAS, package)
        ]
        _retry_command(resolve_cmd, target_id=target_id)


def get_ip_address(target_id: Optional[str], ipv4_only: bool = False):
    """Determines address of the given target; returns the value from
    ipaddress.ip_address."""
    return ipaddress.ip_address(get_ssh_address(target_id, ipv4_only)[0])


def get_ssh_address(target_id: Optional[str],
                    ipv4_only: bool = False) -> Tuple[str, int]:
    """Determines SSH address for given target."""
    cmd = ['target', 'list']
    if ipv4_only:
        cmd.append('--no-ipv6')
    if target_id:
        # target list does not respect -t / --target flag.
        cmd.append(target_id)
    target = json.loads(
        run_ffx_command(cmd=cmd, json_out=True,
                        capture_output=True).stdout.strip())
    addr = target[0]['addresses'][0]
    ssh_port = int(addr['ssh_port'])
    if ssh_port == 0:
        # Returning an unset ssh_port means the default port 22.
        ssh_port = 22
    return (addr['ip'], ssh_port)


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


def wait_for_sigterm(extra_msg: str = '') -> None:
    """
    Spin-wait for either ctrl+c or sigterm. Caller can use try-finally
    statement to perform extra cleanup.

    Args:
      extra_msg: The extra message to be logged.
    """
    try:
        while True:
            # We do expect receiving either ctrl+c or sigterm, so this line
            # literally means sleep forever.
            time.sleep(10000)
    except KeyboardInterrupt:
        logging.info('Ctrl-C received; %s', extra_msg)
    except SystemExit:
        logging.info('SIGTERM received; %s', extra_msg)


@dataclass
class BuildInfo:
    """A structure replica of the output of build section in `ffx target show`.
    """
    version: Optional[str] = None
    product: Optional[str] = None
    board: Optional[str] = None
    commit: Optional[str] = None


def get_build_info(target: Optional[str] = None) -> Optional[BuildInfo]:
    """Retrieves build info from the device.

    Returns:
        A BuildInfo struct, or None if anything goes wrong.
        Any field in BuildInfo can be None to indicate the missing of the field.
    """
    info_cmd = run_ffx_command(cmd=('--machine', 'json', 'target', 'show'),
                               target_id=target,
                               capture_output=True,
                               check=False)
    # If the information was not retrieved, return empty strings to indicate
    # unknown system info.
    if info_cmd.returncode != 0:
        logging.error('ffx target show returns %d', info_cmd.returncode)
        return None
    try:
        info_json = json.loads(info_cmd.stdout.strip())
    except json.decoder.JSONDecodeError as error:
        logging.error('Unexpected json string: %s, exception: %s',
                      info_cmd.stdout, error)
        return None
    if isinstance(info_json, dict) and 'build' in info_json and isinstance(
            info_json['build'], dict):
        return BuildInfo(**info_json['build'])
    return None


def get_system_info(target: Optional[str] = None) -> Tuple[str, str]:
    """Retrieves installed OS version from the device.

    Returns:
        Tuple of strings, containing {product, version number), or a pair of
        empty strings to indicate an error.
    """
    build_info = get_build_info(target)
    if not build_info:
        return ('', '')

    return (build_info.product or '', build_info.version or '')


def get_free_local_port() -> int:
    """Returns an ipv4 port available locally. It does not reserve the port and
    may cause race condition. Copied from catapult
    https://crsrc.org/c/third_party/catapult/telemetry/telemetry/core/util.py;drc=e3f9ae73db5135ad998108113af7ef82a47efc51;l=61"""
    # AF_INET restricts port to IPv4 addresses.
    # SOCK_STREAM means that it is a TCP socket.
    tmp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Setting SOL_SOCKET + SO_REUSEADDR to 1 allows the reuse of local
    # addresses, this is so sockets do not fail to bind for being in the
    # CLOSE_WAIT state.
    tmp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    tmp.bind(('', 0))
    port = tmp.getsockname()[1]
    tmp.close()

    return port
