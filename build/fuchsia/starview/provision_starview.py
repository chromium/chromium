# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provisions Starview / Cuttlefish Android devices for Chromium test runs."""

import logging
import os
import re
import shlex
import subprocess
import time
from typing import Optional, Sequence, Union

DIR_SRC_ROOT = os.path.abspath(
    os.environ.get(
        'SRC_ROOT',
        os.path.join(
            os.path.dirname(__file__), os.pardir, os.pardir, os.pardir
        ),
    )
)

DEFAULT_ADB_PATH = os.path.join(
    DIR_SRC_ROOT,
    'third_party',
    'android_sdk',
    'public',
    'platform-tools',
    'adb',
)


def run_adb(
    adb_path: str,
    args: Sequence[str],
    serial: Optional[str] = None,
    check: bool = True,
    timeout: Optional[int] = None,
) -> subprocess.CompletedProcess:
    """Runs an adb command with ADB_LIBUSB=0."""
    return subprocess.run(
        [adb_path, *(['-s', serial] if serial else []), *args],
        capture_output=True,
        text=True,
        check=check,
        timeout=timeout,
        env={**os.environ, 'ADB_LIBUSB': '0'},
    )


def run_shell(
    adb_path: str,
    serial: str,
    cmd: Union[str, Sequence[str]],
    check: bool = True,
    timeout: Optional[int] = None,
) -> str:
    """Runs an adb shell command on |serial| and returns stdout."""
    return run_adb(
        adb_path,
        ['shell', cmd if isinstance(cmd, str) else shlex.join(cmd)],
        serial=serial,
        check=check,
        timeout=timeout,
    ).stdout


def wait_adb_shell(
    adb_path: str,
    serial: str,
    cmd: Union[str, Sequence[str]],
    pattern: str,
    timeout: float = 480,
    poll_interval: float = 5.0,
) -> None:
    """Polls an adb shell command until stdout matches |pattern|."""
    deadline = time.monotonic() + timeout
    while True:
        try:
            if ':' in serial:
                run_adb(adb_path, ['connect', serial], check=False, timeout=10)
            if re.fullmatch(
                pattern,
                run_shell(adb_path, serial, cmd, timeout=15).strip(),
                re.DOTALL,
            ):
                return
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            pass
        if time.monotonic() >= deadline:
            raise TimeoutError(
                f'Timed out waiting for adb shell {cmd!r} on {serial}'
            )
        time.sleep(poll_interval)


def ensure_root(adb_path: str, serial: str, timeout: int = 480) -> None:
    """Ensures ADB is connected and running as root on |serial|."""
    wait_adb_shell(
        adb_path, serial, ['echo', 'ready'], 'ready', timeout=timeout
    )
    if 'uid=0(root)' not in run_shell(adb_path, serial, ['id'], check=False):
        try:
            run_adb(adb_path, ['root'], serial=serial, check=False, timeout=15)
        except subprocess.TimeoutExpired:
            pass
        wait_adb_shell(
            adb_path, serial, ['id'], r'uid=0\(root\).*', timeout=timeout
        )


def _ensure_mmap_capable_test_data_root(adb_path: str, serial: str) -> None:
    """Ensures test data root is bind-mounted with media_rw_data_file."""
    run_shell(
        adb_path,
        serial,
        'for i in $(seq 1 60); do '
        'grep -q /storage/emulated /proc/mounts && '
        '[ -d /mnt/pass_through/0/emulated/0 ] && exit 0; '
        'sleep 1; '
        'done; exit 1',
        timeout=90,
    )
    run_shell(
        adb_path,
        serial,
        'set -e; '
        '[ ! -d /storage/self ] || [ -L /storage/self/primary ] || '
        '[ -e /storage/self/primary ] || '
        'ln -s /storage/emulated/0 /storage/self/primary; '
        'mkdir -p /mnt/pass_through/0/emulated/0/Download '
        '/storage/emulated/0/Download /sdcard/Download; '
        'chmod -R 777 /mnt/pass_through/0/emulated/0/Download '
        '/storage/emulated/0/Download /sdcard/Download; '
        'chcon -R u:object_r:media_rw_data_file:s0 '
        '/mnt/pass_through/0/emulated/0/Download',
    )

    source_root = '/mnt/pass_through/0/emulated/0/chromium_tests_root'
    target_root = '/sdcard/chromium_tests_root'
    if any(
        '/chromium_tests_root ' in m
        and (
            source_root in m
            or '/mnt/pass_through' in m
            or 'type remotevol' in m
        )
        for m in run_shell(adb_path, serial, ['mount']).splitlines()
    ):
        run_shell(
            adb_path,
            serial,
            f'chown -R shell:shell {source_root} && chmod -R 777 {source_root}',
        )
        return

    run_shell(
        adb_path,
        serial,
        f'mkdir -p {source_root} {target_root} && '
        f'chown -R shell:shell {source_root} && '
        f'chmod -R 777 {source_root} {target_root} && '
        f'chcon -R u:object_r:media_rw_data_file:s0 {source_root} && '
        f'mount --bind {source_root} {target_root}',
    )
    logging.info(
        'Bind-mounted %s to %s for Starview mmap and mkdtemp support',
        source_root,
        target_root,
    )


def provision_device(
    serial: str,
    adb_path: str = DEFAULT_ADB_PATH,
    boot_timeout: int = 480,
) -> None:
    """Provisions a Starview / Cuttlefish Android device for test execution."""
    ensure_root(adb_path, serial, timeout=boot_timeout)
    wait_adb_shell(
        adb_path,
        serial,
        ['getprop', 'sys.boot_completed'],
        '1',
        timeout=boot_timeout,
    )
    wait_adb_shell(
        adb_path,
        serial,
        ['pm', 'path', 'android'],
        r'package:.+',
        timeout=boot_timeout,
    )
    run_shell(
        adb_path,
        serial,
        'input keyevent KEYCODE_WAKEUP; wm dismiss-keyguard; '
        'svc power stayon true',
        check=False,
    )
    _ensure_mmap_capable_test_data_root(adb_path, serial)
