#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script is used to build Debian sysroot images for building Chromium.
"""

import argparse
import hashlib
import lzma
import os
import re
import shutil
import subprocess
import tempfile
import time

import requests

DISTRO = "debian"
RELEASE = "bullseye"

# This number is appended to the sysroot key to cause full rebuilds.  It
# should be incremented when removing packages or patching existing packages.
# It should not be incremented when adding packages.
SYSROOT_RELEASE = 2

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

CHROME_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", ".."))
BUILD_DIR = os.path.join(CHROME_DIR, "out", "sysroot-build", RELEASE)

# gpg keyring file generated using generate_keyring.sh
KEYRING_FILE = os.path.join(SCRIPT_DIR, "keyring.gpg")

ARCHIVE_TIMESTAMP = "20230611T210420Z"

ARCHIVE_URL = f"https://snapshot.debian.org/archive/debian/{ARCHIVE_TIMESTAMP}/"
APT_SOURCES_LIST = [
    # Debian 12 (Bookworm) is needed for GTK4.  It should be kept before
    # bullseye so that bullseye takes precedence.
    ("bookworm", ["main"]),
    ("bookworm-updates", ["main"]),
    # This mimics a sources.list from bullseye.
    ("bullseye", ["main", "contrib", "non-free"]),
    ("bullseye-updates", ["main", "contrib", "non-free"]),
    ("bullseye-backports", ["main", "contrib", "non-free"]),
]

TRIPLES = {
    "amd64": "x86_64-linux-gnu",
    "i386": "i386-linux-gnu",
    "armhf": "arm-linux-gnueabihf",
    "arm64": "aarch64-linux-gnu",
    "mipsel": "mipsel-linux-gnu",
    "mips64el": "mips64el-linux-gnuabi64",
}

REQUIRED_TOOLS = [
    "dpkg-deb",
    "file",
    "gpgv",
    "readelf",
    "tar",
    "xz",
]

# Package configuration
PACKAGES_EXT = "xz"
RELEASE_FILE = "Release"
RELEASE_FILE_GPG = "Release.gpg"

# Packages common to all architectures.
DEBIAN_PACKAGES = [
    "comerr-dev",
    "krb5-multidev",
    "libaom0",
    "libaom3",
    "libasound2",
    "libasound2-dev",
    "libasyncns0",
    "libatk-bridge2.0-0",
    "libatk-bridge2.0-dev",
    "libatk1.0-0",
    "libatk1.0-dev",
    "libatomic1",
    "libatspi2.0-0",
    "libatspi2.0-dev",
    "libattr1",
    "libaudit1",
    "libavahi-client3",
    "libavahi-common3",
    "libavcodec-dev",
    "libavcodec58",
    "libavcodec59",
    "libavformat-dev",
    "libavformat59",
    "libavutil-dev",
    "libavutil56",
    "libavutil57",
    "libb2-1",
    "libblkid-dev",
    "libblkid1",
    "libbluetooth-dev",
    "libbluetooth3",
    "libbluray2",
    "libbrotli-dev",
    "libbrotli1",
    "libbsd0",
    "libbz2-1.0",
    "libc6",
    "libc6-dev",
    "libcairo-gobject2",
    "libcairo-script-interpreter2",
    "libcairo2",
    "libcairo2-dev",
    "libcap-dev",
    "libcap-ng0",
    "libcap2",
    "libchromaprint1",
    "libcjson1",
    "libcloudproviders0",
    "libcodec2-0.9",
    "libcodec2-1.0",
    "libcolord2",
    "libcom-err2",
    "libcrypt-dev",
    "libcrypt1",
    "libcups2",
    "libcups2-dev",
    "libcupsimage2",
    "libcupsimage2-dev",
    "libcurl3-gnutls",
    "libcurl4-gnutls-dev",
    "libdatrie-dev",
    "libdatrie1",
    "libdav1d4",
    "libdav1d6",
    "libdb5.3",
    "libdbus-1-3",
    "libdbus-1-dev",
    "libdbus-glib-1-2",
    "libdbusmenu-glib-dev",
    "libdbusmenu-glib4",
    "libdbusmenu-gtk3-4",
    "libdbusmenu-gtk4",
    "libdeflate-dev",
    "libdeflate0",
    "libdouble-conversion3",
    "libdrm-amdgpu1",
    "libdrm-dev",
    "libdrm-nouveau2",
    "libdrm-radeon1",
    "libdrm2",
    "libegl-dev",
    "libegl1",
    "libegl1-mesa",
    "libegl1-mesa-dev",
    "libelf-dev",
    "libelf1",
    "libepoxy-dev",
    "libepoxy0",
    "libevdev-dev",
    "libevdev2",
    "libevent-2.1-7",
    "libexpat1",
    "libexpat1-dev",
    "libffi-dev",
    "libffi7",
    "libflac-dev",
    "libflac8",
    "libfontconfig-dev",
    "libfontconfig1",
    "libfreetype-dev",
    "libfreetype6",
    "libfribidi-dev",
    "libfribidi0",
    "libgbm-dev",
    "libgbm1",
    "libgcc-10-dev",
    "libgcc-s1",
    "libgcrypt20",
    "libgcrypt20-dev",
    "libgdk-pixbuf-2.0-0",
    "libgdk-pixbuf-2.0-dev",
    "libgl-dev",
    "libgl1",
    "libgl1-mesa-dev",
    "libgl1-mesa-glx",
    "libglapi-mesa",
    "libgles-dev",
    "libgles1",
    "libgles2",
    "libglib2.0-0",
    "libglib2.0-dev",
    "libglvnd-dev",
    "libglvnd0",
    "libglx-dev",
    "libglx0",
    "libgme0",
    "libgmp10",
    "libgnutls-dane0",
    "libgnutls-openssl27",
    "libgnutls28-dev",
    "libgnutls30",
    "libgnutlsxx28",
    "libgomp1",
    "libgpg-error-dev",
    "libgpg-error0",
    "libgraphene-1.0-0",
    "libgraphene-1.0-dev",
    "libgraphite2-3",
    "libgraphite2-dev",
    "libgsm1",
    "libgssapi-krb5-2",
    "libgssrpc4",
    "libgtk-3-0",
    "libgtk-3-dev",
    "libgtk-4-1",
    "libgtk-4-dev",
    "libgtk2.0-0",
    "libgudev-1.0-0",
    "libharfbuzz-dev",
    "libharfbuzz-gobject0",
    "libharfbuzz-icu0",
    "libharfbuzz0b",
    "libhogweed6",
    "libhwy1",
    "libice6",
    "libicu-le-hb0",
    "libicu67",
    "libidl-2-0",
    "libidn11",
    "libidn2-0",
    "libinput-dev",
    "libinput10",
    "libjbig-dev",
    "libjbig0",
    "libjpeg62-turbo",
    "libjpeg62-turbo-dev",
    "libjson-glib-1.0-0",
    "libjsoncpp-dev",
    "libjsoncpp24",
    "libjxl0.7",
    "libjxl0.7",
    "libk5crypto3",
    "libkadm5clnt-mit12",
    "libkadm5srv-mit12",
    "libkdb5-10",
    "libkeyutils1",
    "libkrb5-3",
    "libkrb5-dev",
    "libkrb5support0",
    "liblcms2-2",
    "libldap-2.4-2",
    "liblerc4",
    "libltdl7",
    "liblz4-1",
    "liblzma5",
    "liblzo2-2",
    "libmbedcrypto7",
    "libmd0",
    "libmd4c0",
    "libminizip-dev",
    "libminizip1",
    "libmount-dev",
    "libmount1",
    "libmp3lame0",
    "libmpg123-0",
    "libmtdev1",
    "libncurses-dev",
    "libncurses6",
    "libncursesw6",
    "libnettle8",
    "libnghttp2-14",
    "libnorm1",
    "libnsl2",
    "libnspr4",
    "libnspr4-dev",
    "libnss-db",
    "libnss3",
    "libnss3-dev",
    "libnuma1",
    "libogg-dev",
    "libogg0",
    "libopengl0",
    "libopenjp2-7",
    "libopenmpt0",
    "libopus-dev",
    "libopus-dev",
    "libopus0",
    "libp11-kit0",
    "libpam0g",
    "libpam0g-dev",
    "libpango-1.0-0",
    "libpango1.0-dev",
    "libpangocairo-1.0-0",
    "libpangoft2-1.0-0",
    "libpangox-1.0-0",
    "libpangoxft-1.0-0",
    "libpci-dev",
    "libpci3",
    "libpciaccess0",
    "libpcre16-3",
    "libpcre2-16-0",
    "libpcre2-32-0",
    "libpcre2-8-0",
    "libpcre2-dev",
    "libpcre2-posix2",
    "libpcre3",
    "libpcre3-dev",
    "libpcre32-3",
    "libpcrecpp0v5",
    "libpgm-5.3-0",
    "libpipewire-0.3-0",
    "libpipewire-0.3-dev",
    "libpixman-1-0",
    "libpixman-1-dev",
    "libpng-dev",
    "libpng16-16",
    "libproxy1v5",
    "libpsl5",
    "libpthread-stubs0-dev",
    "libpulse-dev",
    "libpulse-mainloop-glib0",
    "libpulse0",
    "libqt5concurrent5",
    "libqt5core5a",
    "libqt5dbus5",
    "libqt5gui5",
    "libqt5network5",
    "libqt5printsupport5",
    "libqt5sql5",
    "libqt5test5",
    "libqt5widgets5",
    "libqt5xml5",
    "libqt6concurrent6",
    "libqt6core6",
    "libqt6dbus6",
    "libqt6gui6",
    "libqt6network6",
    "libqt6opengl6",
    "libqt6openglwidgets6",
    "libqt6printsupport6",
    "libqt6sql6",
    "libqt6test6",
    "libqt6widgets6",
    "libqt6xml6",
    "librabbitmq4",
    "librav1e0",
    "libre2-9",
    "libre2-dev",
    "librest-0.7-0",
    "librist4",
    "librsvg2-2",
    "librtmp1",
    "libsasl2-2",
    "libselinux1",
    "libselinux1-dev",
    "libsepol1",
    "libsepol1-dev",
    "libshine3",
    "libsm6",
    "libsnappy-dev",
    "libsnappy1v5",
    "libsndfile1",
    "libsodium23",
    "libsoup-gnome2.4-1",
    "libsoup2.4-1",
    "libsoxr0",
    "libspa-0.2-dev",
    "libspeechd-dev",
    "libspeechd2",
    "libspeex1",
    "libsqlite3-0",
    "libsrt1.5-gnutls",
    "libssh-gcrypt-4",
    "libssh2-1",
    "libssl-dev",
    "libssl1.1",
    "libstdc++-10-dev",
    "libstdc++6",
    "libsvtav1enc1",
    "libswresample-dev",
    "libswresample3",
    "libswresample4",
    "libsystemd-dev",
    "libsystemd0",
    "libtasn1-6",
    "libthai-dev",
    "libthai0",
    "libtheora0",
    "libtheora0",
    "libtiff-dev",
    "libtiff5",
    "libtiff6",
    "libtiffxx5",
    "libtinfo6",
    "libtirpc3",
    "libts0",
    "libtwolame0",
    "libudev-dev",
    "libudev1",
    "libudfread0",
    "libunbound8",
    "libunistring2",
    "libutempter-dev",
    "libutempter0",
    "libuuid1",
    "libva-dev",
    "libva-drm2",
    "libva-glx2",
    "libva-wayland2",
    "libva-x11-2",
    "libva2",
    "libvdpau1",
    "libvorbis0a",
    "libvorbisenc2",
    "libvorbisfile3",
    "libvpx-dev",
    "libvpx6",
    "libvpx7",
    "libvulkan-dev",
    "libvulkan1",
    "libwacom2",
    "libwavpack1",
    "libwayland-bin",
    "libwayland-client0",
    "libwayland-cursor0",
    "libwayland-dev",
    "libwayland-egl-backend-dev",
    "libwayland-egl1",
    "libwayland-egl1-mesa",
    "libwayland-server0",
    "libwebp-dev",
    "libwebp6",
    "libwebp7",
    "libwebpdemux2",
    "libwebpmux3",
    "libwrap0",
    "libx11-6",
    "libx11-dev",
    "libx11-xcb-dev",
    "libx11-xcb1",
    "libx264-160",
    "libx264-164",
    "libx265-192",
    "libx265-199",
    "libxau-dev",
    "libxau6",
    "libxcb-dri2-0",
    "libxcb-dri2-0-dev",
    "libxcb-dri3-0",
    "libxcb-dri3-dev",
    "libxcb-glx0",
    "libxcb-glx0-dev",
    "libxcb-icccm4",
    "libxcb-image0",
    "libxcb-image0-dev",
    "libxcb-keysyms1",
    "libxcb-present-dev",
    "libxcb-present0",
    "libxcb-randr0",
    "libxcb-randr0-dev",
    "libxcb-render-util0",
    "libxcb-render-util0-dev",
    "libxcb-render0",
    "libxcb-render0-dev",
    "libxcb-shape0",
    "libxcb-shape0-dev",
    "libxcb-shm0",
    "libxcb-shm0-dev",
    "libxcb-sync-dev",
    "libxcb-sync1",
    "libxcb-util-dev",
    "libxcb-util1",
    "libxcb-xfixes0",
    "libxcb-xfixes0-dev",
    "libxcb-xinerama0",
    "libxcb-xinput0",
    "libxcb-xkb1",
    "libxcb1",
    "libxcb1-dev",
    "libxcomposite-dev",
    "libxcomposite1",
    "libxcursor-dev",
    "libxcursor1",
    "libxdamage-dev",
    "libxdamage1",
    "libxdmcp-dev",
    "libxdmcp6",
    "libxext-dev",
    "libxext6",
    "libxfixes-dev",
    "libxfixes3",
    "libxft-dev",
    "libxft2",
    "libxi-dev",
    "libxi6",
    "libxinerama-dev",
    "libxinerama1",
    "libxkbcommon-dev",
    "libxkbcommon-x11-0",
    "libxkbcommon0",
    "libxml2",
    "libxml2-dev",
    "libxrandr-dev",
    "libxrandr2",
    "libxrender-dev",
    "libxrender1",
    "libxshmfence-dev",
    "libxshmfence1",
    "libxslt1-dev",
    "libxslt1.1",
    "libxss-dev",
    "libxss1",
    "libxt-dev",
    "libxt6",
    "libxtst-dev",
    "libxtst6",
    "libxvidcore4",
    "libxxf86vm-dev",
    "libxxf86vm1",
    "libzmq5",
    "libzstd1",
    "libzvbi0",
    "linux-libc-dev",
    "mesa-common-dev",
    "ocl-icd-libopencl1",
    "qt6-base-dev",
    "qt6-base-dev-tools",
    "qtbase5-dev",
    "qtbase5-dev-tools",
    "shared-mime-info",
    "uuid-dev",
    "wayland-protocols",
    "x11proto-dev",
    "zlib1g",
    "zlib1g-dev",
]

DEBIAN_PACKAGES_ARCH = {
    "amd64": [
        "libasan6",
        "libdrm-intel1",
        "libitm1",
        "liblsan0",
        "libmfx1",
        "libquadmath0",
        "libtsan0",
        "libubsan1",
        "valgrind",
    ],
    "i386": [
        "libasan6",
        "libdrm-intel1",
        "libitm1",
        "libquadmath0",
        "libubsan1",
        "valgrind",
    ],
    "armhf": [
        "libasan6",
        "libdrm-etnaviv1",
        "libdrm-exynos1",
        "libdrm-freedreno1",
        "libdrm-omap1",
        "libdrm-tegra0",
        "libubsan1",
        "valgrind",
    ],
    "arm64": [
        "libasan6",
        "libdrm-etnaviv1",
        "libdrm-freedreno1",
        "libdrm-tegra0",
        "libgmp10",
        "libitm1",
        "liblsan0",
        "libthai0",
        "libtsan0",
        "libubsan1",
        "valgrind",
    ],
    "mipsel": [],
    "mips64el": [
        "valgrind",
    ],
}


def banner(message: str) -> None:
    print("#" * 70)
    print(message)
    print("#" * 70)


def sub_banner(message: str) -> None:
    print("-" * 70)
    print(message)
    print("-" * 70)


def hash_file(hasher, file_name: str) -> str:
    with open(file_name, "rb") as f:
        while chunk := f.read(8192):
            hasher.update(chunk)
    return hasher.hexdigest()


def atomic_copyfile(source: str, destination: str) -> None:
    dest_dir = os.path.dirname(destination)
    with tempfile.NamedTemporaryFile(mode="wb", delete=False,
                                     dir=dest_dir) as temp_file:
        temp_filename = temp_file.name
    shutil.copyfile(source, temp_filename)
    os.rename(temp_filename, destination)


def download_or_copy_non_unique_filename(url: str, dest: str) -> None:
    """
    Downloads a file from a given URL to a destination with a unique filename,
    based on the SHA-256 hash of the URL.
    """
    hash_digest = hashlib.sha256(url.encode()).hexdigest()
    unique_dest = f"{dest}.{hash_digest}"
    download_or_copy(url, unique_dest)
    atomic_copyfile(unique_dest, dest)


def download_or_copy(source: str, destination: str) -> None:
    """
    Downloads a file from the given URL or copies it from a local path to the
    specified destination.
    """
    if os.path.exists(destination):
        print(f"{destination} already in place")
        return

    if source.startswith(("http://", "https://")):
        download_file(source, destination)
    else:
        atomic_copyfile(source, destination)


def download_file(url: str, dest: str, retries=5) -> None:
    """
    Downloads a file from a URL to a specified destination with retry logic,
    directory creation, and atomic write.
    """
    print(f"Downloading from {url} -> {dest}")
    # Create directories if they don't exist
    os.makedirs(os.path.dirname(dest), exist_ok=True)

    for attempt in range(retries):
        try:
            with requests.get(url, stream=True) as response:
                response.raise_for_status()

                # Use a temporary file to write data
                with tempfile.NamedTemporaryFile(
                        mode="wb", delete=False,
                        dir=os.path.dirname(dest)) as temp_file:
                    for chunk in response.iter_content(chunk_size=8192):
                        temp_file.write(chunk)

                # Rename temporary file to destination file
                os.rename(temp_file.name, dest)
                print(f"Downloaded {dest}")
                break

        except requests.RequestException as e:
            print(f"Attempt {attempt} failed: {e}")
            # Exponential back-off
            time.sleep(2**attempt)
    else:
        raise Exception(f"Failed to download file after {retries} attempts")


def sanity_check() -> None:
    """
    Performs sanity checks to ensure the environment is correctly set up.
    """
    banner("Sanity Checks")

    # Determine the Chrome build directory
    os.makedirs(BUILD_DIR, exist_ok=True)
    print(f"Using build directory: {BUILD_DIR}")

    # Check for required tools
    missing = [tool for tool in REQUIRED_TOOLS if not shutil.which(tool)]
    if missing:
        raise Exception(f"Required tools not found: {', '.join(missing)}")


def clear_install_dir(install_root: str) -> None:
    if os.path.exists(install_root):
        shutil.rmtree(install_root)
    os.makedirs(install_root)


def create_tarball(install_root: str, arch: str) -> None:
    tarball_path = os.path.join(BUILD_DIR,
                                f"{DISTRO}_{RELEASE}_{arch}_sysroot.tar.xz")
    banner("Creating tarball " + tarball_path)
    command = [
        "tar",
        "-I",
        "xz -z9 -T0 --lzma2='dict=256MiB'",
        "-cf",
        tarball_path,
        "-C",
        install_root,
        ".",
    ]
    subprocess.run(command, check=True)


def generate_package_list_dist_repo(arch: str, dist: str,
                                    repo_name: str) -> list[dict[str, str]]:
    repo_basedir = f"{ARCHIVE_URL}/dists/{dist}"
    package_list = f"{BUILD_DIR}/Packages.{dist}_{repo_name}_{arch}"
    package_list = f"{package_list}.{PACKAGES_EXT}"
    package_file_arch = f"{repo_name}/binary-{arch}/Packages.{PACKAGES_EXT}"
    package_list_arch = f"{repo_basedir}/{package_file_arch}"

    download_or_copy_non_unique_filename(package_list_arch, package_list)
    verify_package_listing(package_file_arch, package_list, dist)

    with lzma.open(package_list, "rt") as src:
        return [
            dict(
                line.split(": ", 1) for line in package_meta.splitlines()
                if not line.startswith(" "))
            for package_meta in src.read().split("\n\n") if package_meta
        ]


def generate_package_list(arch: str) -> dict[str, str]:
    package_meta = {}
    for dist, repos in APT_SOURCES_LIST:
        for repo_name in repos:
            for meta in generate_package_list_dist_repo(arch, dist, repo_name):
                package_meta[meta["Package"]] = meta

    # Read the input file and create a dictionary mapping package names to URLs
    # and checksums.
    missing = set(DEBIAN_PACKAGES + DEBIAN_PACKAGES_ARCH[arch])
    package_dict: dict[str, str] = {}
    for meta in package_meta.values():
        package = meta["Package"]
        if package in missing:
            missing.remove(package)
            url = ARCHIVE_URL + meta["Filename"]
            package_dict[url] = meta["SHA256"]
    if missing:
        raise Exception(f"Missing packages: {', '.join(missing)}")

    # Write the URLs and checksums of the requested packages to the output file
    output_file = os.path.join(SCRIPT_DIR, "generated_package_lists",
                               f"{RELEASE}.{arch}")
    with open(output_file, "w") as f:
        f.write("\n".join(sorted(package_dict)) + "\n")
    return package_dict


def hacks_and_patches(install_root: str, script_dir: str, arch: str) -> None:
    banner("Misc Hacks & Patches")

    # Remove an unnecessary dependency on qtchooser.
    qtchooser_conf = os.path.join(install_root, "usr", "lib", TRIPLES[arch],
                                  "qt-default/qtchooser/default.conf")
    if os.path.exists(qtchooser_conf):
        os.remove(qtchooser_conf)

    # libxcomposite1 is missing a symbols file.
    atomic_copyfile(
        os.path.join(script_dir, "libxcomposite1-symbols"),
        os.path.join(install_root, "debian", "libxcomposite1", "DEBIAN",
                     "symbols"),
    )

    # Include limits.h in stdlib.h to fix an ODR issue.
    stdlib_h = os.path.join(install_root, "usr", "include", "stdlib.h")
    replace_in_file(stdlib_h, r"(#include <stddef.h>)",
                    r"\1\n#include <limits.h>")

    # Move pkgconfig scripts.
    pkgconfig_dir = os.path.join(install_root, "usr", "lib", "pkgconfig")
    os.makedirs(pkgconfig_dir, exist_ok=True)
    triple_pkgconfig_dir = os.path.join(install_root, "usr", "lib",
                                        TRIPLES[arch], "pkgconfig")
    if os.path.exists(triple_pkgconfig_dir):
        for file in os.listdir(triple_pkgconfig_dir):
            shutil.move(os.path.join(triple_pkgconfig_dir, file),
                        pkgconfig_dir)

    # GTK4 is provided by bookworm (12), but pango is provided by bullseye
    # (11).  Fix the GTK4 pkgconfig file to relax the pango version
    # requirement.
    gtk4_pc = os.path.join(pkgconfig_dir, "gtk4.pc")
    replace_in_file(gtk4_pc, r"pango [>=0-9. ]*", "pango")
    replace_in_file(gtk4_pc, r"pangocairo [>=0-9. ]*", "pangocairo")


def replace_in_file(file_path: str, search_pattern: str,
                    replace_pattern: str) -> None:
    with open(file_path, "r") as file:
        content = file.read()
    with open(file_path, "w") as file:
        file.write(re.sub(search_pattern, replace_pattern, content))


def install_into_sysroot(build_dir: str, install_root: str,
                         packages: dict[str, str]) -> None:
    """
    Installs libraries and headers into the sysroot environment.
    """
    banner("Install Libs And Headers Into Jail")

    debian_packages_dir = os.path.join(build_dir, "debian-packages")
    os.makedirs(debian_packages_dir, exist_ok=True)

    debian_dir = os.path.join(install_root, "debian")
    os.makedirs(debian_dir, exist_ok=True)
    control_file = os.path.join(debian_dir, "control")
    # Create an empty control file
    open(control_file, "a").close()

    for package, sha256sum in packages.items():
        package_name = os.path.basename(package)
        package_path = os.path.join(debian_packages_dir, package_name)

        banner(f"Installing {package_name}")
        download_or_copy(package, package_path)
        if hash_file(hashlib.sha256(), package_path) != sha256sum:
            raise ValueError(f"SHA256 mismatch for {package_path}")

        sub_banner(f"Extracting to {install_root}")
        subprocess.run(["dpkg-deb", "-x", package_path, install_root],
                       check=True)

        base_package = get_base_package_name(package_path)
        debian_package_dir = os.path.join(debian_dir, base_package, "DEBIAN")

        # Extract the control file
        os.makedirs(debian_package_dir, exist_ok=True)
        with subprocess.Popen(
            ["dpkg-deb", "-e", package_path, debian_package_dir],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
        ) as proc:
            _, err = proc.communicate()
            if proc.returncode != 0:
                message = "Failed to extract control from"
                raise Exception(
                    f"{message} {package_path}: {err.decode('utf-8')}")

    # Prune /usr/share, leaving only pkgconfig, wayland, and wayland-protocols
    usr_share = os.path.join(install_root, "usr", "share")
    for item in os.listdir(usr_share):
        full_path = os.path.join(usr_share, item)
        if os.path.isdir(full_path) and item not in [
                "pkgconfig",
                "wayland",
                "wayland-protocols",
        ]:
            shutil.rmtree(full_path)


def get_base_package_name(package_path: str) -> str:
    """
    Retrieves the base package name from a Debian package.
    """
    result = subprocess.run(["dpkg-deb", "--field", package_path, "Package"],
                            capture_output=True,
                            text=True)
    if result.returncode != 0:
        raise Exception(
            f"Failed to get package name from {package_path}: {result.stderr}")
    return result.stdout.strip()


def cleanup_jail_symlinks(install_root: str) -> None:
    """
    Cleans up jail symbolic links by converting absolute symlinks
    into relative ones.
    """
    for root, dirs, files in os.walk(install_root):
        for name in files + dirs:
            full_path = os.path.join(root, name)
            if os.path.islink(full_path):
                target_path = os.readlink(full_path)

                # Check if the symlink is absolute and points inside the
                # install_root.
                if os.path.isabs(target_path):
                    # Compute the relative path from the symlink to the target.
                    relative_path = os.path.relpath(
                        os.path.join(install_root, target_path.strip("/")),
                        os.path.dirname(full_path),
                    )
                    # Verify that the target exists inside the install_root.
                    joined_path = os.path.join(os.path.dirname(full_path),
                                               relative_path)
                    if not os.path.exists(joined_path):
                        raise Exception(
                            f"Link target doesn't exist: {joined_path}")
                    os.remove(full_path)
                    os.symlink(relative_path, full_path)


def verify_library_deps(install_root: str) -> None:
    """
    Verifies if all required libraries are present in the sysroot environment.
    """
    # Get all shared libraries and their dependencies.
    shared_libs = set()
    needed_libs = set()
    for root, _, files in os.walk(install_root):
        for file in files:
            if ".so" not in file:
                continue
            path = os.path.join(root, file)
            islink = os.path.islink(path)
            if islink:
                path = os.path.join(root, os.readlink(path))
            cmd_file = ["file", path]
            output = subprocess.check_output(cmd_file).decode()
            if ": ELF" not in output or "shared object" not in output:
                continue
            shared_libs.add(file)
            if islink:
                continue
            cmd_readelf = ["readelf", "-d", path]
            output = subprocess.check_output(cmd_readelf).decode()
            for line in output.split("\n"):
                if "NEEDED" in line:
                    needed_libs.add(line.split("[")[1].split("]")[0])

    missing_libs = needed_libs - shared_libs
    if missing_libs:
        raise Exception(f"Missing libraries: {missing_libs}")


def build_sysroot(arch: str) -> None:
    install_root = os.path.join(BUILD_DIR, f"{RELEASE}_{arch}_staging")
    clear_install_dir(install_root)
    packages = generate_package_list(arch)
    install_into_sysroot(BUILD_DIR, install_root, packages)
    hacks_and_patches(install_root, SCRIPT_DIR, arch)
    cleanup_jail_symlinks(install_root)
    verify_library_deps(install_root)
    create_tarball(install_root, arch)


def upload_sysroot(arch: str) -> str:
    tarball_path = os.path.join(BUILD_DIR,
                                f"{DISTRO}_{RELEASE}_{arch}_sysroot.tar.xz")
    command = [
        "upload_to_google_storage_first_class.py",
        "--bucket",
        "chrome-linux-sysroot",
        tarball_path,
    ]
    return subprocess.check_output(command).decode("utf-8")


def verify_package_listing(file_path: str, output_file: str,
                           dist: str) -> None:
    """
    Verifies the downloaded Packages.xz file against its checksum and GPG keys.
    """
    # Paths for Release and Release.gpg files
    repo_basedir = f"{ARCHIVE_URL}/dists/{dist}"
    release_list = f"{repo_basedir}/{RELEASE_FILE}"
    release_list_gpg = f"{repo_basedir}/{RELEASE_FILE_GPG}"

    release_file = os.path.join(BUILD_DIR, f"{dist}-{RELEASE_FILE}")
    release_file_gpg = os.path.join(BUILD_DIR, f"{dist}-{RELEASE_FILE_GPG}")

    if not os.path.exists(KEYRING_FILE):
        raise Exception(f"KEYRING_FILE not found: {KEYRING_FILE}")

    # Download Release and Release.gpg files
    download_or_copy_non_unique_filename(release_list, release_file)
    download_or_copy_non_unique_filename(release_list_gpg, release_file_gpg)

    # Verify Release file with GPG
    subprocess.run(
        ["gpgv", "--keyring", KEYRING_FILE, release_file_gpg, release_file],
        check=True)

    # Find the SHA256 checksum for the specific file in the Release file
    sha256sum_pattern = re.compile(r"([a-f0-9]{64})\s+\d+\s+" +
                                   re.escape(file_path) + r"$")
    sha256sum_match = None
    with open(release_file, "r") as f:
        for line in f:
            if match := sha256sum_pattern.search(line):
                sha256sum_match = match.group(1)
                break

    if not sha256sum_match:
        raise Exception(
            f"Checksum for {file_path} not found in {release_file}")

    if hash_file(hashlib.sha256(), output_file) != sha256sum_match:
        raise Exception(f"Checksum mismatch for {output_file}")


def main():
    parser = argparse.ArgumentParser(
        description="Build and upload Debian sysroot images for Chromium.")
    parser.add_argument("command", choices=["build", "upload"])
    parser.add_argument("architecture", choices=list(TRIPLES))
    args = parser.parse_args()

    sanity_check()

    if args.command == "build":
        build_sysroot(args.architecture)
    elif args.command == "upload":
        upload_sysroot(args.architecture)


if __name__ == "__main__":
    main()
