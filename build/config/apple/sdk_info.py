#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import doctest
import itertools
import os
import plistlib
import subprocess
import sys

# This script prints information about the build system, the operating
# system and the iOS or Mac SDK (depending on the platform "iphonesimulator",
# "iphoneos" or "macosx" generally).


def SplitVersion(version):
    """Splits the Xcode version to 3 values.

    >>> list(SplitVersion('8.2.1.1'))
    ['8', '2', '1']
    >>> list(SplitVersion('9.3'))
    ['9', '3', '0']
    >>> list(SplitVersion('10.0'))
    ['10', '0', '0']
    """
    version = version.split('.')
    return itertools.islice(
        itertools.chain(version, itertools.repeat('0')), 0, 3
    )


def FormatVersion(version):
    """Converts Xcode version to a format required for DTXcode in Info.plist

    >>> FormatVersion('8.2.1')
    '0821'
    >>> FormatVersion('9.3')
    '0930'
    >>> FormatVersion('10.0')
    '1000'
    """
    major, minor, patch = SplitVersion(version)
    return ('%2s%s%s' % (major, minor, patch)).replace(' ', '0')


_HERMETIC_XCODE_DIR = os.path.abspath(
    os.path.join(
        os.path.dirname(__file__), '..', '..', 'mac_files', 'xcode_binaries'
    )
)

_PLATFORM_DISPLAY_NAME = {
    'appletvos': 'AppleTVOS',
    'appletvsimulator': 'AppleTVSimulator',
    'iphoneos': 'iPhoneOS',
    'iphonesimulator': 'iPhoneSimulator',
    'macosx': 'MacOSX',
    'visionos': 'XROS',
    'visionsimulator': 'XRSimulator',
    'watchos': 'WatchOS',
    'watchsimulator': 'WatchSimulator',
    'xros': 'XROS',
    'xrsimulator': 'XRSimulator',
}


def FillXcodeVersion(settings, developer_dir):
    """Fills the Xcode version and build number into |settings|."""
    if developer_dir:
        xcode_version_plist_path = os.path.join(
            developer_dir, 'Contents/version.plist'
        )
        with open(xcode_version_plist_path, 'rb') as f:
            version_plist = plistlib.load(f)
        version_verbatim = version_plist['CFBundleShortVersionString']
        settings['xcode_version'] = FormatVersion(version_verbatim)
        settings['xcode_version_int'] = int(settings['xcode_version'], 10)
        settings['xcode_version_verbatim'] = version_verbatim
        settings['xcode_build'] = version_plist['ProductBuildVersion']
        return

    lines = (
        subprocess.check_output(['xcodebuild', '-version'])
        .decode('UTF-8')
        .splitlines()
    )
    version_verbatim = lines[0].split()[-1]
    settings['xcode_version'] = FormatVersion(version_verbatim)
    settings['xcode_version_int'] = int(settings['xcode_version'], 10)
    settings['xcode_version_verbatim'] = version_verbatim
    settings['xcode_build'] = lines[-1].split()[-1]


def FillMachineOSBuild(settings, developer_dir=None):
    """Fills OS build number into |settings|."""
    if sys.platform != 'darwin':
        # Non-Darwin hosts lack `sw_vers -buildVersion`, so use the SDK's
        # `ProductBuildVersion` as a stand-in for `BuildMachineOSBuild`.
        dev_dir = developer_dir or _HERMETIC_XCODE_DIR
        for platform_name in ('MacOSX', 'iPhoneSimulator', 'iPhoneOS'):
            sys_ver_plist = os.path.join(
                dev_dir,
                'Contents',
                'Developer',
                'Platforms',
                f'{platform_name}.platform',
                'Developer',
                'SDKs',
                f'{platform_name}.sdk',
                'System',
                'Library',
                'CoreServices',
                'SystemVersion.plist',
            )
            if os.path.exists(sys_ver_plist):
                with open(sys_ver_plist, 'rb') as f:
                    settings['machine_os_build'] = plistlib.load(f)[
                        'ProductBuildVersion'
                    ]
                return
        raise RuntimeError(
            f'SystemVersion.plist not found in any SDK under {dev_dir}'
        )

    machine_os_build = (
        subprocess.check_output(['sw_vers', '-buildVersion'])
        .decode('UTF-8')
        .strip()
    )
    settings['machine_os_build'] = machine_os_build


def FillSDKPathAndVersion(
    settings, platform, xcode_version, developer_dir=None
):
    """Fills the SDK path and version for |platform| into |settings|."""
    if sys.platform != 'darwin':
        dev_dir = os.path.abspath(developer_dir or _HERMETIC_XCODE_DIR)
        platform_name = _PLATFORM_DISPLAY_NAME[platform]
        sdk_platform_path = os.path.join(
            dev_dir,
            'Contents',
            'Developer',
            'Platforms',
            f'{platform_name}.platform',
        )
        sdks_dir = os.path.join(sdk_platform_path, 'Developer', 'SDKs')
        unversioned_sdk = os.path.join(sdks_dir, f'{platform_name}.sdk')
        sys_ver_plist = os.path.join(
            unversioned_sdk,
            'System',
            'Library',
            'CoreServices',
            'SystemVersion.plist',
        )
        with open(sys_ver_plist, 'rb') as f:
            sys_ver = plistlib.load(f)
        sdk_version = sys_ver['ProductVersion']
        sdk_build = sys_ver['ProductBuildVersion']
        versioned_sdk = os.path.join(
            sdks_dir, f'{platform_name}{sdk_version}.sdk'
        )
        sdk_path = (
            versioned_sdk if os.path.exists(versioned_sdk) else unversioned_sdk
        )
        settings['sdk_path'] = sdk_path
        settings['sdk_version'] = sdk_version
        settings['sdk_platform_path'] = sdk_platform_path
        settings['sdk_build'] = sdk_build
        settings['toolchains_path'] = os.path.join(
            dev_dir,
            'Contents',
            'Developer',
            'Toolchains',
            'XcodeDefault.xctoolchain',
        )
        return

    settings['sdk_path'] = (
        subprocess.check_output(['xcrun', '-sdk', platform, '--show-sdk-path'])
        .decode('UTF-8')
        .strip()
    )
    settings['sdk_version'] = (
        subprocess.check_output(
            ['xcrun', '-sdk', platform, '--show-sdk-version']
        )
        .decode('UTF-8')
        .strip()
    )
    settings['sdk_platform_path'] = (
        subprocess.check_output(
            ['xcrun', '-sdk', platform, '--show-sdk-platform-path']
        )
        .decode('UTF-8')
        .strip()
    )
    settings['sdk_build'] = (
        subprocess.check_output(
            ['xcrun', '-sdk', platform, '--show-sdk-build-version']
        )
        .decode('UTF-8')
        .strip()
    )
    settings['toolchains_path'] = os.path.join(
        subprocess.check_output(['xcode-select', '-print-path'])
        .decode('UTF-8')
        .strip(),
        'Toolchains/XcodeDefault.xctoolchain',
    )


def CreateXcodeSymlinkAt(src, dst, root_build_dir):
    """Create symlink to Xcode directory at target location."""

    if not os.path.isdir(dst):
        os.makedirs(dst)

    dst = os.path.join(dst, os.path.basename(src))
    updated_value = os.path.join(root_build_dir, dst)

    if sys.platform != 'darwin':
        # Use a relative symlink on non-Darwin hosts so that RBE remote
        # execution workers (which mount the execroot at `/b/f/w/...` rather
        # than the host's absolute path) can resolve `sdk/xcode_links/...`.
        src = os.path.relpath(
            os.path.abspath(src), os.path.abspath(os.path.dirname(dst))
        )

    # Update the symlink only if it is different from the current destination.
    if os.path.islink(dst):
        current_src = os.readlink(dst)
        if current_src == src:
            return updated_value
        os.unlink(dst)
        sys.stderr.write(
            'existing symlink %s points %s; want %s. Removed.'
            % (dst, current_src, src)
        )
    os.symlink(src, dst)
    return updated_value


def main():
    doctest.testmod()

    parser = argparse.ArgumentParser()
    parser.add_argument('--developer_dir')
    parser.add_argument(
        '--get_sdk_info',
        action='store_true',
        default=False,
        help='Returns SDK info in addition to xcode info.',
    )
    parser.add_argument(
        '--get_machine_info',
        action='store_true',
        default=False,
        help='Returns machine info in addition to xcode info.',
    )
    parser.add_argument(
        '--create_symlink_at',
        help='Create symlink of SDK at given location and '
        'returns the symlinked paths as SDK info instead '
        'of the original location.',
    )
    parser.add_argument(
        '--root_build_dir', default='.', help='Value of gn $root_build_dir'
    )
    parser.add_argument(
        'platform',
        choices=[
            'appletvos',
            'appletvsimulator',
            'iphoneos',
            'iphonesimulator',
            'macosx',
            'watchos',
            'watchsimulator',
        ],
    )
    args = parser.parse_args()
    if not args.developer_dir and sys.platform != 'darwin':
        args.developer_dir = _HERMETIC_XCODE_DIR
    if args.developer_dir:
        os.environ['DEVELOPER_DIR'] = args.developer_dir

    settings = {}
    if args.get_machine_info:
        FillMachineOSBuild(settings, args.developer_dir)
    FillXcodeVersion(settings, args.developer_dir)
    if args.get_sdk_info:
        FillSDKPathAndVersion(
            settings,
            args.platform,
            settings['xcode_version'],
            args.developer_dir,
        )

    for key in sorted(settings):
        value = settings[key]
        if args.create_symlink_at and '_path' in key:
            value = CreateXcodeSymlinkAt(
                value, args.create_symlink_at, args.root_build_dir
            )
        if isinstance(value, str):
            value = '"%s"' % value
        print('%s=%s' % (key, value))


if __name__ == '__main__':
    sys.exit(main())
