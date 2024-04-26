#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers to reliably reboot the device via serial and fastboot.

Note, this file will be executed in docker instance without vpython3, so we use
python3 instead. The docker instance runs this file as a symbolic link of dmc
via the "main" function.
"""

import json
import logging
import os
import shutil
import signal
import subprocess
import sys
import time

from typing import List
from boot_device import BootMode
from compatible_utils import running_unattended

# pylint: disable=too-many-return-statements, too-many-branches


def _env_ready() -> bool:
    """Checks if the required environment is ready to support the functions in
    this file."""
    if shutil.which('fastboot') is None:
        logging.warning('fastboot is not accessible')
        return False
    if shutil.which('serialio') is None:
        logging.warning('serialio is not accessible')
        return False
    return True


def boot_device(node_id: str,
                serial_num: str,
                mode: BootMode,
                must_boot: bool = False) -> bool:
    """Boots device into desired mode via serial and fastboot.
    This function waits for at most 10 minutes for the transition.

    Args:
        node_id: The fuchsia node id of the device.
        serial_num: The fastboot serial number of the device.
        mode: Desired boot mode.
        must_boot: Forces device to reboot regardless the current state.

    Returns:
        a boolean value to indicate if the operation succeeded; missing
        dependencies like serialio (for serial access) and fastboot, or the
        device cannot be found may also introduce the error.
    """
    #TODO(crbug.com/40935296): Remove the default values once the use in
    # flash_device has been migrated.
    if node_id is None:
        node_id = os.getenv('FUCHSIA_NODENAME')
    if serial_num is None:
        serial_num = os.getenv('FUCHSIA_FASTBOOT_SERNUM')
    assert node_id is not None
    assert serial_num is not None

    assert mode in [BootMode.REGULAR, BootMode.BOOTLOADER
                    ], 'Unsupported BootMode %s for serial_boot_device.' % mode
    assert _env_ready()

    if is_in_fuchsia(node_id):
        if not must_boot and mode == BootMode.REGULAR:
            return True
        # pylint: disable=subprocess-run-check
        if subprocess.run([
                'serialio', node_id, 'send', 'dm', 'reboot' +
            ('' if mode == BootMode.REGULAR else '-bootloader')
        ]).returncode != 0:
            logging.error('Failed to send dm reboot[-bootloader] via serialio')
            return False
    elif is_in_fastboot(serial_num):
        # fastboot is stateless and there isn't a reason to reboot the device
        # again to go to the fastboot.
        if mode == BootMode.BOOTLOADER:
            return True
        if not _run_fastboot(['reboot'], serial_num):
            # Shouldn't return None here, unless the device was rebooting. In
            # the case, it would be safer to return false.
            return False
    else:
        logging.error('Cannot find node id %s or fastboot serial number %s',
                      node_id, serial_num)
        return False

    start_sec = time.time()
    while time.time() - start_sec < 600:
        assert mode in [BootMode.REGULAR, BootMode.BOOTLOADER]
        if mode == BootMode.REGULAR and is_in_fuchsia(node_id):
            return True
        if mode == BootMode.BOOTLOADER and is_in_fastboot(serial_num):
            return True
    logging.error(
        'Failed to transite node id %s or fastboot serial number %s '
        'to expected state %s', node_id, serial_num, mode)
    return False


def _serialio_send_and_wait(node_id: str, command: List[str],
                            waitfor: str) -> bool:
    """Continously sends the command to the device and waits for the waitfor
    string via serialio.
    This function asserts the existence of serialio and waits at most ~30
    seconds."""
    assert shutil.which('serialio') is not None
    start_sec = time.time()
    with subprocess.Popen(['serialio', node_id, 'wait', waitfor],
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL) as proc:
        while time.time() - start_sec < 28:
            send_command = ['serialio', node_id, 'send']
            send_command.extend(command)
            # pylint: disable=subprocess-run-check
            if subprocess.run(send_command).returncode != 0:
                logging.error('Failed to send %s via serialio to %s', command,
                              node_id)
                return False
            result = proc.poll()
            if result is not None:
                if result == 0:
                    return True
                logging.error(
                    'Failed to wait %s via serial to %s, '
                    'return code %s', waitfor, node_id, result)
                return False
            time.sleep(2)
        proc.kill()
    logging.error('Have not found %s via serialio to %s', waitfor, node_id)
    return False


def is_in_fuchsia(node_id: str) -> bool:
    """Checks if the device is running in fuchsia through serial.
    Note, this check goes through serial and does not guarantee the fuchsia os
    has a workable network or ssh connection.
    This function asserts the existence of serialio and waits at most ~60
    seconds."""
    if not _serialio_send_and_wait(
            node_id, ['echo', 'yes-i-am-healthy', '|', 'sha1sum'],
            '89d517b7db104aada669a83bc3c3a906e00671f7'):
        logging.error(
            'Device %s did not respond echo, '
            'it may not be running fuchsia', node_id)
        return False
    if not _serialio_send_and_wait(node_id, ['ps'], 'sshd'):
        logging.warning(
            'Cannot find sshd from ps on %s, the ssh '
            'connection may not be available.', node_id)
    return True


def is_in_fastboot(serial_num: str) -> bool:
    """Checks if the device is running in fastboot through fastboot command.
    Note, the fastboot may be impacted by the usb congestion and causes this
    function to return false.
    This function asserts the existence of fastboot and waits at most ~30
    seconds."""
    start_sec = time.time()
    while time.time() - start_sec < 28:
        result = _run_fastboot(['getvar', 'product'], serial_num)
        if result is None:
            return False
        if result:
            return True
        time.sleep(2)
    logging.error('Failed to wait for fastboot state of %s', serial_num)
    return False


def _run_fastboot(args: List[str], serial_num: str) -> bool:
    """Executes the fastboot command and kills the hanging process.
    The fastboot may be impacted by the usb congestion and causes the process to
    hang forever. So this command waits for 30 seconds before killing the
    process, and it's not good for flashing.
    Note, if this function detects the fastboot is waiting for the device, i.e.
    the device is not in the fastboot, it returns None instead, e.g. unknown.
    This function asserts the existence of fastboot."""
    assert shutil.which('fastboot') is not None
    args.insert(0, 'fastboot')
    args.extend(('-s', serial_num))
    try:
        # Capture output to ensure we can get '< waiting for serial-num >'
        # output.
        # pylint: disable=subprocess-run-check
        if subprocess.run(args, capture_output=True,
                          timeout=30).returncode == 0:
            return True
    except subprocess.TimeoutExpired as timeout:
        if timeout.stderr is not None and serial_num.lower(
        ) in timeout.stderr.decode().lower():
            logging.warning('fastboot is still waiting for %s', serial_num)
            return None
    logging.error('Failed to run %s against fastboot %s', args, serial_num)
    return False


def _shutdown_if_serial_is_unavailable(node_id: str) -> None:
    if not running_unattended():
        return
    # pylint: disable=subprocess-run-check
    if subprocess.run(['serialio', node_id, 'poll']).returncode != 0:
        logging.warning('shutting down the docker by killing the pid 1')
        # Before killing the process itself, force shutting down the logging to
        # flush everything.
        logging.shutdown()
        # In docker instance, killing root process will cause the instance to be
        # shut down and restarted by swarm_docker. So the updated tty can be
        # attached to the new docker instance.
        os.kill(1, signal.SIGTERM)


def main(action: str) -> int:
    """Main entry of serial_boot_device."""
    node_id = os.getenv('FUCHSIA_NODENAME')
    serial_num = os.getenv('FUCHSIA_FASTBOOT_SERNUM')
    assert node_id is not None
    assert serial_num is not None

    handlers = [logging.StreamHandler()]
    if os.path.isdir('/home/swarming/'):
        handlers.append(
            logging.FileHandler('/home/swarming/dmc.%s.log' % node_id))
    logging.basicConfig(format='%(levelname)s %(asctime)s %(message)s',
                        handlers=handlers,
                        level=logging.INFO)
    logging.info('Running command %s against %s %s', sys.argv, node_id,
                 serial_num)

    # Checks the environment after initializing the logging.
    if not _env_ready():
        logging.error('Missing environment setup, unable to perform action.')
        return 2

    if action == 'health-check':
        _shutdown_if_serial_is_unavailable(node_id)
        if is_in_fuchsia(node_id) or is_in_fastboot(serial_num):
            # Print out the json result without using logging to avoid any
            # potential formatting issue.
            print(
                json.dumps([{
                    'nodename': node_id,
                    'state': 'healthy',
                    'status_message': '',
                    'dms_state': ''
                }]))
            return 0
        logging.error('Cannot find node id %s or fastboot serial number %s',
                      node_id, serial_num)
        return 1
    if action in ['reboot', 'after-task']:
        if action == 'after-task':
            _shutdown_if_serial_is_unavailable(node_id)
        if boot_device(node_id, serial_num, BootMode.REGULAR, must_boot=True):
            return 0
        logging.error(
            'Cannot reboot the device with node id %s and fastboot '
            'serial number %s', node_id, serial_num)
        return 1
    if action == 'reboot-fastboot':
        if boot_device(node_id,
                       serial_num,
                       BootMode.BOOTLOADER,
                       must_boot=True):
            return 0
        logging.error(
            'Cannot reboot the device with node id %s and fastboot '
            'serial number %s into fastboot', node_id, serial_num)
        return 1
    if action == 'is-in-fuchsia':
        if is_in_fuchsia(node_id):
            return 0
        logging.error('Cannot find node id %s', node_id)
        return 1
    if action == 'is-in-fastboot':
        if is_in_fastboot(serial_num):
            return 0
        logging.error('Cannot find fastboot serial number %s', serial_num)
        return 1
    if action == 'server-version':
        # TODO(crbug.com/40935296): Implement the server-version.
        print('chromium')
        return 0
    if action == 'before-task':
        # TODO(crbug.com/40935296): fuchsia.py requires IMAGE_MANIFEST_PATH and
        # BOOTSERVER_PATH to support before-task call. So the following
        # statement does not work as it should be.
        _shutdown_if_serial_is_unavailable(node_id)
        return 0
    if action == 'set-power-state':
        # Do nothing. The device is always restarted during after-task.
        return 0
    logging.error('Unknown command %s', action)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
