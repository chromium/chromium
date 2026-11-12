# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provisions physical Starview Android devices for Chromium test runs."""

import os
from typing import List, Optional, Sequence

import provision_starview


def resolve_devices(
    adb_path: str, devices: Optional[Sequence[str]] = None
) -> List[str]:
    """Resolves target ADB device serials."""
    if devices:
        return list(devices)
    if os.environ.get('ANDROID_SERIAL'):
        return [os.environ['ANDROID_SERIAL']]
    resolved: List[str] = []
    for line in provision_starview.run_adb(
        adb_path, ['devices']
    ).stdout.splitlines():
        parts = line.strip().split()
        if len(parts) >= 2 and parts[1] == 'device':
            resolved.append(parts[0])
    if not resolved:
        raise RuntimeError('No connected ADB devices found.')
    return resolved


def force_stop_packages(adb_path: str, serial: str) -> None:
    """Force-stops any running or leftover Chromium packages on the device."""
    for line in provision_starview.run_shell(
        adb_path, serial, ['pm', 'list', 'packages', 'org.chromium']
    ).splitlines():
        line = line.strip()
        if line.startswith('package:'):
            pkg = line.split('package:', 1)[1].strip()
            if 'org.chromium' in pkg:
                provision_starview.run_shell(
                    adb_path, serial, ['am', 'force-stop', pkg]
                )


def provision_physical_device(
    serial: str,
    adb_path: str = provision_starview.DEFAULT_ADB_PATH,
) -> None:
    """Provisions a physical Starview device and applies hardware hygiene."""
    provision_starview.provision_device(serial, adb_path=adb_path)
    provision_starview.run_shell(
        adb_path,
        serial,
        'settings put system screen_brightness_mode 0; '
        'settings put system screen_brightness 1',
        check=False,
    )
    force_stop_packages(adb_path, serial)
