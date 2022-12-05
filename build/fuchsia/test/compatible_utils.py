# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions used in both v1 and v2 scripts."""

import os
import platform
import re
import stat
import subprocess

from typing import Iterable, List, Optional, Tuple


# File indicating version of an image downloaded to the host
_BUILD_ARGS = "buildargs.gn"
_ARGS_FILE = 'args.gn'

_FILTER_DIR = 'testing/buildbot/filters'
_SSH_KEYS = os.path.expanduser('~/.ssh/fuchsia_authorized_keys')


class VersionNotFoundError(Exception):
    """Thrown when version info cannot be retrieved from device."""


def get_ssh_keys() -> str:
    """Returns path of Fuchsia ssh keys."""

    return _SSH_KEYS


def running_unattended() -> bool:
    """Returns true if running non-interactively.

    When running unattended, confirmation prompts and the like are suppressed.
    """

    # Chromium tests only for the presence of the variable, so match that here.
    return 'CHROME_HEADLESS' in os.environ


def get_host_arch() -> str:
    """Retrieve CPU architecture of the host machine. """
    host_arch = platform.machine()
    # platform.machine() returns AMD64 on 64-bit Windows.
    if host_arch in ['x86_64', 'AMD64']:
        return 'x64'
    if host_arch == 'aarch64':
        return 'arm64'
    raise NotImplementedError('Unsupported host architecture: %s' % host_arch)


def add_exec_to_file(file: str) -> None:
    """Add execution bits to a file.

    Args:
        file: path to the file.
    """
    file_stat = os.stat(file)
    os.chmod(file, file_stat.st_mode | stat.S_IXUSR)


def _add_exec_to_pave_binaries(system_image_dir: str):
    """Add exec to required pave files.

    The pave files may vary depending if a product-bundle or a prebuilt images
    directory is being used.
    Args:
      system_image_dir: string path to the directory containing the pave files.
    """
    pb_files = [
        'pave.sh',
        os.path.join(f'host_{get_host_arch()}', 'bootserver')
    ]
    image_files = [
        'pave.sh',
        os.path.join(f'bootserver.exe.linux-{get_host_arch()}')
    ]
    use_pb_files = os.path.exists(os.path.join(system_image_dir, pb_files[1]))
    for f in pb_files if use_pb_files else image_files:
        add_exec_to_file(os.path.join(system_image_dir, f))


def pave(image_dir: str, target_id: Optional[str])\
        -> subprocess.CompletedProcess:
    """"Pave a device using the pave script inside |image_dir|."""
    _add_exec_to_pave_binaries(image_dir)
    pave_command = [
        os.path.join(image_dir, 'pave.sh'), '--authorized-keys',
        get_ssh_keys(), '-1'
    ]
    if target_id:
        pave_command.extend(['-n', target_id])
    return subprocess.run(pave_command, check=True, text=True, timeout=300)


def parse_host_port(host_port_pair: str) -> Tuple[str, int]:
    """Parses a host name or IP address and a port number from a string of
    any of the following forms:
    - hostname:port
    - IPv4addy:port
    - [IPv6addy]:port

    Returns:
        A tuple of the string host name/address and integer port number.

    Raises:
        ValueError if `host_port_pair` does not contain a colon or if the
        substring following the last colon cannot be converted to an int.
    """

    host, port = host_port_pair.rsplit(':', 1)

    # Strip the brackets if the host looks like an IPv6 address.
    if len(host) >= 4 and host[0] == '[' and host[-1] == ']':
        host = host[1:-1]
    return (host, int(port))


def get_ssh_prefix(host_port_pair: str) -> List[str]:
    """Get the prefix of a barebone ssh command."""

    ssh_addr, ssh_port = parse_host_port(host_port_pair)
    return [
        'ssh', '-F',
        os.path.expanduser('~/.fuchsia/sshconfig'), ssh_addr, '-p',
        str(ssh_port)
    ]


def install_symbols(package_paths: Iterable[str],
                    fuchsia_out_dir: str) -> None:
    """Installs debug symbols for a package into the GDB-standard symbol
    directory located in fuchsia_out_dir."""

    symbol_root = os.path.join(fuchsia_out_dir, '.build-id')
    for path in package_paths:
        package_dir = os.path.dirname(path)
        ids_txt_path = os.path.join(package_dir, 'ids.txt')
        with open(ids_txt_path, 'r') as f:
            for entry in f:
                build_id, binary_relpath = entry.strip().split(' ')
                binary_abspath = os.path.abspath(
                    os.path.join(package_dir, binary_relpath))
                symbol_dir = os.path.join(symbol_root, build_id[:2])
                symbol_file = os.path.join(symbol_dir, build_id[2:] + '.debug')
                if not os.path.exists(symbol_dir):
                    os.makedirs(symbol_dir)

                if os.path.islink(symbol_file) or os.path.exists(symbol_file):
                    # Clobber the existing entry to ensure that the symlink's
                    # target is up to date.
                    os.unlink(symbol_file)
                os.symlink(os.path.relpath(binary_abspath, symbol_dir),
                           symbol_file)


# TODO(crbug.com/1279803): Until one can send files to the device when running
# a test, filter files must be read from the test package.
def map_filter_file_to_package_file(filter_file: str) -> str:
    """Returns the path to |filter_file| within the test component's package."""

    if not _FILTER_DIR in filter_file:
        raise ValueError('CFv2 tests only support registered filter files '
                         'present in the test package')
    return '/pkg/' + filter_file[filter_file.index(_FILTER_DIR):]


def get_sdk_hash(system_image_dir: str) -> Tuple[str, str]:
    """Read version of hash in pre-installed package directory.
    Returns:
        Tuple of (product, version) of image to be installed.
    Raises:
        VersionNotFoundError: if contents of buildargs.gn cannot be found or the
        version number cannot be extracted.
    """

    # TODO(crbug.com/1261961): Stop processing buildargs.gn directly.
    args_file = os.path.join(system_image_dir, _BUILD_ARGS)
    if not os.path.exists(args_file):
        args_file = os.path.join(system_image_dir, _ARGS_FILE)

    if not os.path.exists(args_file):
        raise VersionNotFoundError(
            f'Dir {system_image_dir} did not contain {_BUILD_ARGS} or '
            f'{_ARGS_FILE}')

    with open(args_file) as f:
        contents = f.readlines()
    if not contents:
        raise VersionNotFoundError('Could not retrieve %s' % args_file)
    version_key = 'build_info_version'
    product_key = 'build_info_product'
    info_keys = [product_key, version_key]
    version_info = {}
    for line in contents:
        for key in info_keys:
            match = re.match(r'%s = "(.*)"' % key, line)
            if match:
                version_info[key] = match.group(1)
    if not (version_key in version_info and product_key in version_info):
        raise VersionNotFoundError(
            'Could not extract version info from %s. Contents: %s' %
            (args_file, contents))

    return (version_info[product_key], version_info[version_key])


def find_in_dir(target_name: str,
                parent_dir: str,
                search_for_dir: bool = False) -> Optional[str]:
    """Finds path in SDK.

    Args:
      target_name: Name of target to find, as a string.
      parent_dir: Directory to start search in.
      search_for_dir: boolean, whether to search for a directory or file.

    Returns:
      Optional full path to the target, if found. None if not found.
    """
    # Doesn't make sense to look for a full path. Only extract the basename.
    target_name = os.path.basename(target_name)
    for root, dirs, files in os.walk(parent_dir):
        # Removing these parens causes the following equivalent operation order:
        # if (target_name in dirs) if search_for_dir else files, which is
        # incorrect.
        #pylint: disable=superfluous-parens
        if target_name in (dirs if search_for_dir else files):
            return os.path.abspath(os.path.join(root, target_name))
        #pylint: enable=superfluous-parens

    return None


def find_image_in_sdk(product_name: str, product_bundle: bool,
                      sdk_root: str) -> Optional[str]:
    """Finds image dir in SDK for product given.

    Args:
      product_name: Name of product's image directory to find.
      product_bundle: boolean, whether image will be in a product-bundle or not.
        Product bundle images use a different directory format.
      sdk_root: String path to root of SDK (third_party/fuchsia-sdk).

    Returns:
      Optional full path to the target, if found. None if not found.
    """
    if product_bundle:
        top_image_dir = os.path.join(sdk_root, 'images')
        path = find_in_dir(product_name,
                           parent_dir=top_image_dir,
                           search_for_dir=True)
        return find_in_dir('images', parent_dir=path, search_for_dir=True)

    # Non-product-bundle directories take some massaging.
    top_image_dir = os.path.join(sdk_root, 'images-internal')
    product, board = product_name.split('.')
    board_dir = find_in_dir(board,
                            parent_dir=top_image_dir,
                            search_for_dir=True)

    #  The board dir IS the images dir
    return find_in_dir(product, parent_dir=board_dir, search_for_dir=True)
