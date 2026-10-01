#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks that SettingsFragment subclasses are in enums.xml.

Finds them using dexdump on ChromePublic.apk.
"""

import argparse
import os
import pathlib
import subprocess
import sys
import xml.etree.ElementTree as ET

_TARGET_INTERFACE = (
    "org.chromium.components.browser_ui.settings.SettingsFragment"
)


# Arbitrarily chosen. Guards against our logic reporting too few fragments.
_MIN_EXPECTED_FRAGMENTS = 50


class _ClassInfo:
    def __init__(self):
        self.is_abstract = False
        self.is_interface = False
        self.supertypes = []



def _parse_type(line: bytes) -> str:
    """Extracts a dot-separated Java type name from a dexdump line.

    Examples:
      "  Class descriptor  : 'Lorg/chromium/chrome/MainSettings;'"
        -> "org.chromium.chrome.MainSettings"
      "  Superclass        : 'Ljava/lang/Object;'"
        -> "java.lang.Object"
      "    #0              : 'Lorg/chromium/SettingsFragment;'"
        -> "org.chromium.SettingsFragment"
    """
    start = line.find(b"'L")
    if start == -1:
        return ""
    start += 2
    end = line.find(b";'", start)
    if end == -1:
        return ""
    return line[start:end].replace(b"/", b".").decode("ascii")


def _java_hash_code(s: str) -> int:
    """Computes signed 32-bit String.hashCode() matching Java behavior."""
    h = 0
    for c in s:
        h = (31 * h + ord(c)) & 0xFFFFFFFF
    return h if h < 0x80000000 else h - 0x100000000


def _parse_fragment_hashes_from_enums_xml(enums_file: str) -> dict[int, str]:
    with open(enums_file, "r", encoding="utf-8") as f:
        tree = ET.parse(f)
    root = tree.getroot()
    fragment_hashes = {}
    for enum_node in root.findall(
        ".//enum[@name='AndroidSettingsFragmentHashes']"
    ):
        for int_node in enum_node.findall("int"):
            value_str = int_node.get("value")
            if value_str is not None:
                fragment_hashes[int(value_str)] = int_node.get("label")
    return fragment_hashes


# Recursively traverses class hierarchy (supers and interfaces) to determine
# whether `class_name` implements or extends `_TARGET_INTERFACE`.
def _implements_target(class_name, classes, memo):
    memo_res = memo.get(class_name)
    if memo_res is not None:
        return memo_res

    class_info = classes.get(class_name)
    if class_info:
        for supertype in class_info.supertypes:
            if _implements_target(supertype, classes, memo):
                memo[class_name] = True
                return True

    memo[class_name] = False
    return False


def _find_settings_classes(classes):
    concrete_fragments = []
    memo = {_TARGET_INTERFACE: True}
    for class_name, class_info in classes.items():
        if (
            not class_info.is_interface
            and not class_info.is_abstract
            and _implements_target(class_name, classes, memo)
        ):
            concrete_fragments.append(class_name)
    return concrete_fragments


def _parse_dexdump(proc):
    classes = {}
    class_info = None
    in_interfaces = False

    ord_c = ord("C")
    ord_a = ord("A")
    ord_s = ord("S")
    ord_i = ord("I")

    for line in proc.stdout:
        if in_interfaces:
            if line.startswith(b"    #"):
                class_info.supertypes.append(_parse_type(line))
                continue
            in_interfaces = False

        # Top-level class fields in dexdump output start with two spaces
        # ("  "). Method bodies and instructions start with 4+ spaces and
        # can be skipped immediately.
        if len(line) < 3 or line[:2] != b"  " or line[2] == ord(" "):
            continue

        c = line[2]
        if c == ord_c and line.startswith(b"  Class descriptor"):
            curr_class = _parse_type(line)
            class_info = _ClassInfo()
            classes[curr_class] = class_info
        elif c == ord_a and line.startswith(b"  Access flags"):
            # "Access flags" line only occurs for class headers in dexdump
            # (method and field definitions use "access_flags :").
            class_info.is_abstract = b"ABSTRACT" in line
            class_info.is_interface = b"INTERFACE" in line
        elif c == ord_s and line.startswith(b"  Superclass"):
            class_info.supertypes.append(_parse_type(line))
        elif c == ord_i and line.startswith(b"  Interfaces"):
            in_interfaces = True

    return classes


def _main():
    parser = argparse.ArgumentParser(
        description="Verify SettingsFragment subclasses are in enums.xml."
    )
    parser.add_argument(
        "--dexdump-path", required=True, help="Path to dexdump executable."
    )
    parser.add_argument(
        "--apk-path", required=True, help="Path to ChromePublic.apk."
    )
    parser.add_argument(
        "--enums-xml-path", required=True, help="Path to enums.xml."
    )
    parser.add_argument(
        "--stamp", help="Path to stamp file to write on success."
    )
    args = parser.parse_args()

    #   -n: omit debug information.
    #   -j: disable dex verification.
    cmd = [args.dexdump_path, "-n", "-j", args.apk_path]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, bufsize=1024 * 1024)
    classes = _parse_dexdump(proc)
    if proc.wait() != 0:
        sys.exit(f'Command failed: {cmd} (code={proc.returncode})')

    if _TARGET_INTERFACE not in classes:
        sys.exit(
            f"Target interface {_TARGET_INTERFACE} not found in {args.apk_path}"
        )

    concrete_fragments = _find_settings_classes(classes)

    # Guard against logic error by ensuring there are a good number of matches.
    if len(concrete_fragments) < _MIN_EXPECTED_FRAGMENTS:
        sys.exit(
            f"Expected at least {_MIN_EXPECTED_FRAGMENTS} {_TARGET_INTERFACE} "
            f"subclasses in {args.apk_path}. Found {concrete_fragments}"
        )

    fragment_hashes = _parse_fragment_hashes_from_enums_xml(args.enums_xml_path)

    # Guard against logic error by ensuring there are a good number of matches.
    if len(fragment_hashes) < _MIN_EXPECTED_FRAGMENTS:
        sys.exit(
            f"Expected at least {_MIN_EXPECTED_FRAGMENTS} entries in "
            f"AndroidSettingsFragmentHashes enum (in {args.enums_xml_path}). "
            f"Found {fragment_hashes}"
        )

    # Extract simple class names matching Java's Class.getSimpleName(),
    # stripping both package name and enclosing class names for inner classes.
    unique_simple_names = {
        c.rsplit(".", 1)[-1].rsplit("$", 1)[-1] for c in concrete_fragments
    }
    missing_fragments = []
    for name in sorted(unique_simple_names):
        h = _java_hash_code(name)
        if fragment_hashes.get(h) != name:
            missing_fragments.append((h, name))

    if missing_fragments:
        missing_str = "\n".join(
            f'<int value="{h}" label="{name}"/>'
            for h, name in missing_fragments
        )
        sys.exit(
            "The following entries for AndroidSettingsFragmentHashes are "
            f"missing in {args.enums_xml_path}:\n{missing_str}"
        )

    if args.stamp:
        pathlib.Path(args.stamp).touch()


if __name__ == "__main__":
    _main()
