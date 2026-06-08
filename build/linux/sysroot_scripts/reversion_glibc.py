# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Rewrite incompatible default symbols in glibc.
"""

import re
import subprocess

# This constant comes from the oldest glibc version in
# //chrome/installer/linux/debian/dist_package_versions.json and
# //chrome/installer/linux/rpm/dist_package_provides.json
MAX_ALLOWED_GLIBC_VERSION = [2, 26]
MAX_ALLOWED_GLIBC_VERSION_ARCH = {
    "riscv64": [2, 33],
}

VERSION_PATTERN = re.compile("GLIBC_([0-9\.]+)")
SECTION_PATTERN = re.compile(r"^ *\[ *[0-9]+\] +(\S+) +\S+ + ([0-9a-f]+) .*$")

# Some otherwise disallowed symbols are referenced in the linux-chromeos build.
# To continue supporting it, allow these symbols to remain enabled.
SYMBOL_ALLOWLIST = {
    "fts64_close",
    "fts64_open",
    "fts64_read",
    "memfd_create",
}

IS_LITTLE_ENDIAN = {
    "amd64": True,
    "i386": True,
    "armhf": True,
    "arm64": True,
    "mipsel": True,
    "mips64el": True,
    "ppc64el": True,
    "riscv64": True,
    "s390x": False,
}


def reversion_glibc(bin_file: str, arch: str) -> None:
    max_allowed_glibc_version = MAX_ALLOWED_GLIBC_VERSION_ARCH.get(
        arch, MAX_ALLOWED_GLIBC_VERSION)
    is_little_endian = IS_LITTLE_ENDIAN[arch]
    # The two dictionaries below map from symbol name to
    # (symbol version, symbol index).
    #
    # The default version for a given symbol (which may be unsupported).
    default_version = {}
    # The max supported symbol version for a given symbol.
    supported_version = {}

    # Populate |default_version| and |supported_version| with data from readelf.
    stdout = subprocess.check_output(
        ["readelf", "--dyn-syms", "--wide", bin_file])
    for line in stdout.decode("utf-8").split("\n"):
        cols = re.split("\s+", line)
        # Remove localentry and next element which appears only in ppc64le
        # readelf output. Keeping them causes incorrect symbol parsing
        # leading to improper GLIBC version restrictions.
        if len(cols) > 7 and cols[7] == "[<localentry>:":
            cols.pop(7)
            cols.pop(7)
        # Skip the preamble.
        if len(cols) < 9:
            continue

        index = cols[1].rstrip(":")
        # Skip the header.
        if not index.isdigit():
            continue

        index = int(index)
        name = cols[8].split("@")
        # Ignore unversioned symbols.
        if len(name) < 2:
            continue

        base_name = name[0]
        version = name[-1]
        # The default version will have '@@' in the name.
        is_default = len(name) > 2

        if version.startswith("XCRYPT_"):
            # Prefer GLIBC_* versioned symbols over XCRYPT_* ones.
            # Set the version to something > MAX_ALLOWED_GLIBC_VERSION
            # so this symbol will not be picked.
            version = [10**10]
        else:
            match = re.match(VERSION_PATTERN, version)
            # Ignore symbols versioned with GLIBC_PRIVATE.
            if not match:
                continue
            version = [int(part) for part in match.group(1).split(".")]

        if version < max_allowed_glibc_version:
            old_supported_version = supported_version.get(
                base_name, ([-1], -1))
            supported_version[base_name] = max((version, index),
                                               old_supported_version)
        if is_default:
            default_version[base_name] = (version, index)

    # Get the offset into the binary of the .gnu.version section from readelf.
    stdout = subprocess.check_output(
        ["readelf", "--sections", "--wide", bin_file])
    for line in stdout.decode("utf-8").split("\n"):
        if match := SECTION_PATTERN.match(line):
            section_name, address = match.groups()
            if section_name == ".gnu.version":
                gnu_version_addr = int(address, base=16)
                break
    else:
        raise Exception("No .gnu.version section found")

    # Rewrite the binary.
    bin_data = bytearray(open(bin_file, "rb").read())
    for name, (version, index) in default_version.items():
        # No need to rewrite the default if it's already an allowed version.
        if version <= max_allowed_glibc_version:
            continue

        if name in SYMBOL_ALLOWLIST:
            continue
        elif name in supported_version:
            _, supported_index = supported_version[name]
        else:
            supported_index = -1

        # The .gnu.version section is divided into 16-bit chunks that give the
        # symbol versions.  The 16th bit is a flag that's false for the default
        # version.
        #
        # Disable the unsupported symbol.
        old_default = gnu_version_addr + 2 * index
        if is_little_endian:
            old_default += 1
        assert (bin_data[old_default] & 0x80) == 0
        bin_data[old_default] ^= 0x80

        # If we found a supported version, enable that as default.
        if supported_index != -1:
            new_default = gnu_version_addr + 2 * supported_index
            if is_little_endian:
                new_default += 1
            assert (bin_data[new_default] & 0x80) == 0x80
            bin_data[new_default] ^= 0x80

    open(bin_file, "wb").write(bin_data)
