#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to boot Cuttlefish guest image directly in QEMU."""

import argparse
import logging
import os
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import zipfile


STARVIEW_DIR = os.path.dirname(os.path.abspath(__file__))

# Setup import path for common testing utils
sys.path.append(os.path.abspath(os.path.join(STARVIEW_DIR, '..', 'test')))
import common

AVBTOOL = os.path.join(STARVIEW_DIR, 'avbtool.py')
CVD_AVB_TESTKEY = os.path.join(STARVIEW_DIR, 'cvd_avb_testkey_rsa4096.pem')
CVD_AVB_PUBKEY = os.path.join(STARVIEW_DIR, 'cvd_rsa4096.avbpubkey')

if (
    not os.path.exists(AVBTOOL)
    or not os.path.exists(CVD_AVB_TESTKEY)
    or not os.path.exists(CVD_AVB_PUBKEY)
):
    raise FileNotFoundError(
        "Required AVB tools/keys (avbtool.py, cvd_avb_testkey_rsa4096.pem, "
        "cvd_rsa4096.avbpubkey) are missing in the starview directory."
    )


sys.path.append(STARVIEW_DIR)
import partition_creator
import simg2img
from hvc_mock import start_hvc_mock_responder


IS_HEADLESS_BY_DEFAULT = not (
    'DISPLAY' in os.environ
    or 'WAYLAND_DISPLAY' in os.environ
    or 'XDG_CURRENT_DESKTOP' in os.environ
)


def sign_uboot_env_image(image_path, partition_size=73728):
    """Signs the uboot_env image with the standard AOSP RSA4096 test key using avbtool."""
    logging.info(f"Adding AVB hash footer to {image_path}...")
    cmd = [
        'python3',
        AVBTOOL,
        'add_hash_footer',
        '--image',
        image_path,
        '--partition_name',
        'uboot_env',
        '--partition_size',
        str(partition_size),
        '--algorithm',
        'SHA256_RSA4096',
        '--key',
        CVD_AVB_TESTKEY,
    ]
    subprocess.run(cmd, check=True)


def create_persistent_vbmeta_image(output_path):
    """Generates a persistent vbmeta image chaining uboot_env using avbtool."""
    logging.info(f"Generating persistent vbmeta image at {output_path}...")
    cmd = [
        'python3',
        AVBTOOL,
        'make_vbmeta_image',
        '--algorithm',
        'SHA256_RSA4096',
        '--key',
        CVD_AVB_TESTKEY,
        '--output',
        output_path,
        '--chain_partition',
        f'uboot_env:1:{CVD_AVB_PUBKEY}',
    ]
    subprocess.run(cmd, check=True)

    # Pad the image to 64KB to match the fixed partition size defined in the
    # GPT/VMDK layout. Without this, QEMU will return I/O errors to the guest
    # when it attempts to read beyond the actual end of the file.
    with open(output_path, 'r+b') as f:
        f.truncate(65536)


def boot_cuttlefish(args, cuttlefish_zip, bootloader, temp_dir):
    """Extracts images, initializes partition structures, and starts QEMU."""
    logging.info(f"Extracting {cuttlefish_zip} to {temp_dir}...")
    with zipfile.ZipFile(cuttlefish_zip, 'r') as zip_ref:
        zip_ref.extractall(temp_dir)

    super_img = os.path.join(temp_dir, 'super.img')
    assert os.path.exists(super_img), f"super.img not found in {temp_dir}"
    simg2img.unsparse_in_place(super_img)
    # The extracted super.img contains the Fuchsia Fxfs system blobs and has
    # almost 0 bytes free. When Fuchsia boots, fshost allocates mutable storage
    # volumes (e.g., 'unencrypted' and 'data') inside Fxfs. Expanding super.img
    # with spare space as a sparse file provides the needed space for volume
    # creation.
    with open(super_img, 'r+b') as f:
        f.truncate(f.seek(0, os.SEEK_END) + 8 * 1024 * 1024 * 1024)

    userdata_img = os.path.join(temp_dir, 'userdata.img')
    assert os.path.exists(userdata_img), f"userdata.img not found in {temp_dir}"
    simg2img.unsparse_in_place(userdata_img)
    # Ensure sufficient capacity on /data for installing large APKs (such as
    # Chrome) and writing runtime dex caches.
    with open(userdata_img, 'r+b') as f:
        f.truncate(f.seek(0, os.SEEK_END) + 16 * 1024 * 1024 * 1024)

    partition_creator.create_zero_image(
        os.path.join(temp_dir, 'metadata.img'), 16
    )
    partition_creator.create_zero_image(
        os.path.join(temp_dir, 'u-boot-vars.img'), 1
    )

    partition_creator.create_misc_image(os.path.join(temp_dir, 'misc.img'))
    partition_creator.create_zero_image(os.path.join(temp_dir, 'dummy.img'), 1)

    partition_creator.create_uboot_env_image(
        {
            'ethprime': 'eth1',
            'bootcmd': 'virtio scan && verified_boot_android virtio 1 _a',
            'bootcmd_android': 'verified_boot_android virtio 1 _a',
            'bootdelay': '0',
            'bootargs': ' '.join(
                [
                    'earlyprintk=ttyS0',
                    'console=ttyS0',
                    'androidboot.console=ttyS0',
                    'androidboot.boot_devices=pci0000:00/0000:00:04.0',
                    'androidboot.fstab_suffix=cf.ext4.hctr2',
                    'androidboot.force_normal_boot=1',
                    'androidboot.apex_updatable=0',
                ]
            ),
        },
        os.path.join(temp_dir, 'uboot_env.img'),
    )
    sign_uboot_env_image(os.path.join(temp_dir, 'uboot_env.img'))

    create_persistent_vbmeta_image(
        os.path.join(temp_dir, 'persistent_vbmeta.img')
    )

    partition_creator.create_gpt_and_vmdk(
        [
            {
                'name': 'uboot_env',
                'path': os.path.join(temp_dir, 'uboot_env.img'),
            },
            {
                'name': 'vbmeta',
                'path': os.path.join(temp_dir, 'persistent_vbmeta.img'),
            },
            {'name': 'misc', 'path': os.path.join(temp_dir, 'misc.img')},
            {'name': 'boot_a', 'path': os.path.join(temp_dir, 'boot.img')},
            {
                'name': 'vendor_boot_a',
                'path': os.path.join(temp_dir, 'vendor_boot.img'),
            },
            {
                'name': 'init_boot_a',
                'path': os.path.join(temp_dir, 'init_boot.img'),
            },
            {'name': 'vbmeta_a', 'path': os.path.join(temp_dir, 'vbmeta.img')},
            {
                'name': 'vbmeta_system_a',
                'path': os.path.join(temp_dir, 'vbmeta_system.img'),
            },
            {'name': 'super', 'path': os.path.join(temp_dir, 'super.img')},
            {
                'name': 'userdata',
                'path': os.path.join(temp_dir, 'userdata.img'),
            },
            {
                'name': 'metadata',
                'path': os.path.join(temp_dir, 'metadata.img'),
            },
        ],
        os.path.join(temp_dir, 'disk.gpt'),
        os.path.join(temp_dir, 'disk.vmdk'),
    )

    qemu_cmd = [
        args.qemu_path,
        '-m',
        '28672',
        '-smp',
        '6',
        '-machine',
        'pc',
        '-cpu',
        'host',
        '-enable-kvm',
        # Load U-Boot ROM as pflash (read-only code)
        '-drive',
        f'if=pflash,format=raw,readonly=on,file={bootloader}',
        # Load U-Boot vars flash (read-write environment)
        '-drive',
        f'if=pflash,format=raw,file={os.path.join(temp_dir, "u-boot-vars.img")}',
        # Shift our main system disk to virtio 1 (second slot) by mapping a dummy disk
        # to virtio 0, since Cuttlefish U-Boot and fstab expect Android on virtio 1.
        '-drive',
        f'file={os.path.join(temp_dir, "dummy.img")},format=raw,if=none,id=drive-disk0',
        '-device',
        'virtio-blk-pci-non-transitional,drive=drive-disk0,id=virtio-disk0,bootindex=1',
        # Map our partitioned VMDK disk as virtio 1
        '-drive',
        f'file={os.path.join(temp_dir, "disk.vmdk")},format=vmdk,if=none,id=drive-disk1',
        '-device',
        'virtio-blk-pci-non-transitional,drive=drive-disk1,id=virtio-disk1',
        # User-mode networking with static hostfwd for ADB and SSH
        '-netdev',
        f'user,id=net0,hostfwd=tcp::2222-:22,hostfwd=tcp::{args.adb_port}-:5555',
        '-device',
        'virtio-net-pci,netdev=net0,vectors=8,addr=1.2',
        # Map standard serial port (U-Boot & Kernel console) directly to stdio
        '-serial',
        'stdio',
        # Enable QEMU monitor on a unix socket
        '-monitor',
        f'unix:{os.path.join(temp_dir, "monitor.sock")},server,nowait',
    ]
    if args.headless:
        qemu_cmd.append('-nographic')

    hvc_args, hvc_stop_event = start_hvc_mock_responder(temp_dir)
    qemu_cmd.extend(hvc_args)

    logging.info("Launching QEMU with command:")
    logging.info(" ".join(qemu_cmd))

    qemu_proc = subprocess.Popen(qemu_cmd)

    return qemu_proc, hvc_stop_event


def _wait_adb_shell(adb_path, adb_port, cmd, pattern):
    """Waits for an ADB shell command output to match regex pattern or times out."""
    target = f'127.0.0.1:{adb_port}'
    start_time = time.time()
    while time.time() - start_time < 300:
        res = subprocess.run(
            [adb_path, '-s', target, 'shell'] + cmd,
            capture_output=True,
            text=True,
        )
        if res.returncode == 0 and re.fullmatch(
            pattern, res.stdout.strip(), re.DOTALL
        ):
            return True
        time.sleep(10)
    return False


def main():
    common.catch_sigterm()
    parser = argparse.ArgumentParser(description="Boot Cuttlefish in raw QEMU")
    parser.add_argument(
        '--packages',
        default=Path(STARVIEW_DIR) / '../../..',
        type=Path,
        help='Directory containing cuttlefish guest images, bootloader, '
        'and other tools.',
    )
    parser.add_argument(
        '--qemu-path',
        default=os.path.join(
            common.SDK_ROOT,
            'tools',
            'x64',
            'qemu_internal',
            'bin',
            'qemu-system-x86_64',
        ),
        help='Path to qemu-system-x86_64 binary.',
    )
    parser.add_argument(
        '--adb-path',
        default=os.path.join(
            common.DIR_SRC_ROOT,
            'third_party',
            'android_sdk',
            'public',
            'platform-tools',
            'adb',
        ),
        help='Path to adb binary.',
    )
    parser.add_argument(
        '--adb-port',
        type=int,
        default=6520,
        help='Host port to forward guest ADB (5555) to.',
    )
    parser.add_argument(
        '--headless',
        action='store_true',
        default=IS_HEADLESS_BY_DEFAULT,
        help='Run QEMU in headless mode (without GUI). '
        'Defaults to True if no desktop environment is detected.',
    )
    args = parser.parse_args()

    isolated_outdir = os.environ.get('ISOLATED_OUTDIR')
    handlers = [logging.StreamHandler()]
    if isolated_outdir:
        os.makedirs(isolated_outdir, exist_ok=True)
        log_file = os.path.join(isolated_outdir, 'emulator.log')
        handlers.append(logging.FileHandler(log_file))
    logging.basicConfig(
        level=logging.INFO,
        format='%(levelname)s %(asctime)s %(message)s',
        handlers=handlers,
        force=True,
    )

    logging.info(
        f"Running in {'headless' if args.headless else 'headfull'} mode."
    )

    assert os.path.exists(args.qemu_path) or shutil.which(args.qemu_path), (
        f"QEMU not found at {args.qemu_path}"
    )

    assert os.path.exists(args.adb_path) or shutil.which(args.adb_path), (
        f"ADB not found at {args.adb_path}"
    )

    cuttlefish_dir = args.packages / 'cuttlefish'
    assert cuttlefish_dir.exists(), (
        f"Cuttlefish directory does not exist: {cuttlefish_dir}"
    )

    cuttlefish_zip = None
    for f in os.listdir(cuttlefish_dir):
        if re.match(r'^cf_x86_64_blazer_starnix-img-.*\.zip$', f):
            cuttlefish_zip = os.path.join(cuttlefish_dir, f)
            break

    assert cuttlefish_zip, (
        f"No Cuttlefish guest image ZIP found in {cuttlefish_dir}"
    )

    bootloader = args.packages / 'uboot' / 'u-boot.rom'
    assert bootloader.exists(), f"Bootloader not found at {bootloader}"

    hvc_stop_event = None
    qemu_proc = None

    subprocess.run(['killall', '-9', 'qemu-system-x86_64'])
    subprocess.run([args.adb_path, 'kill-server'])
    subprocess.run(['killall', '-9', 'adb'])

    tmp_dir = tempfile.gettempdir()
    for item in os.listdir(tmp_dir):
        if item.startswith('cf_qemu_'):
            shutil.rmtree(os.path.join(tmp_dir, item), ignore_errors=True)

    with tempfile.TemporaryDirectory(prefix='cf_qemu_') as temp_dir:
        try:
            qemu_proc, hvc_stop_event = boot_cuttlefish(
                args, cuttlefish_zip, bootloader, temp_dir
            )

            target = f'127.0.0.1:{args.adb_port}'
            subprocess.run([args.adb_path, 'connect', target])

            logging.info(
                f"Waiting for guest ADB to respond on port {args.adb_port}..."
            )
            if not _wait_adb_shell(
                args.adb_path, args.adb_port, ['echo', 'ready'], 'ready'
            ):
                logging.error("Timed out waiting for guest ADB to respond.")
                return 1
            logging.info("Guest ADB is ready.")

            logging.info("Restarting ADB daemon as root...")
            subprocess.run([args.adb_path, '-s', target, 'root'])

            # Wait for ADB to reconnect as root after daemon restart.
            if not _wait_adb_shell(
                args.adb_path, args.adb_port, ['echo', 'ready'], 'ready'
            ):
                logging.error(
                    "Timed out waiting for guest ADB after root restart."
                )
                return 1

            # Wait for Android to complete boot.
            logging.info("Waiting for Android to complete boot...")
            if not _wait_adb_shell(
                args.adb_path,
                args.adb_port,
                ['getprop', 'sys.boot_completed'],
                '1',
            ):
                logging.warning("Timed out waiting for sys.boot_completed=1.")

            # Wait for package manager to become responsive.
            logging.info(
                "Waiting for Android package manager to become ready..."
            )
            if not _wait_adb_shell(
                args.adb_path,
                args.adb_port,
                ['pm', 'list', 'packages'],
                'package:.*',
            ):
                logging.error("Timed out waiting for package manager.")
                return 1

            logging.info("Successfully connected to guest ADB!")

            logging.info("Press Ctrl+C to terminate the emulator.")
            common.wait_for_sigterm()
        finally:
            if hvc_stop_event:
                logging.info("Stopping HVC mock responder...")
                hvc_stop_event.set()

            if qemu_proc and qemu_proc.poll() is None:
                logging.info("Terminating QEMU process...")
                qemu_proc.terminate()
                try:
                    qemu_proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    qemu_proc.kill()


if __name__ == '__main__':
    sys.exit(main())
