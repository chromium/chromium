#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks if Chromium enables the same Crubit features as upstream Crubit.

Upstream Crubit enables `SUPPORTED_FEATURES` from `features/global_features.bzl`
by default.  Chromium enables `crubit_globally_enabled_features` from
`features.gni` (next to this script).  This script reads `global_features.bzl`
at the `CRUBIT_REVISION` used by the Chromium toolchain and then compares it
against `features.gni`.

The script reports:

* Features from `.bzl` that `.gni` doesn't enable
  (and that are not in `KNOWN_EXCLUSIONS` below).
* Features that `.gni` enables, but `.bzl` doesn't.
* Features from `crubit_fast_tracked_features` that are enabled by
  `.bzl` (and therefore can be moved to `crubit_globally_enabled_features`).
* Stale entries in `KNOWN_EXCLUSIONS`.
"""

import argparse
import ast
import os
import re
import sys
import urllib.request

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_SRC_DIR = os.path.join(THIS_DIR, '..', '..', '..', '..')
sys.path.append(os.path.join(_SRC_DIR, 'build'))
sys.path.append(os.path.join(_SRC_DIR, 'tools', 'rust'))
import gn_helpers  # noqa: E402
from update_rust import CHROMIUM_DIR, CRUBIT_REVISION  # noqa: E402

GNI_PATH = os.path.join(THIS_DIR, 'features.gni')
GNI_GLOBALLY_ENABLED_VARIABLE = 'crubit_globally_enabled_features'
GNI_FAST_TRACKED_VARIABLE = 'crubit_fast_tracked_features'

BZL_URL_FORMAT = (
    'https://raw.githubusercontent.com/google/crubit/'
    '{revision}/features/global_features.bzl'
)
BZL_VARIABLE = 'SUPPORTED_FEATURES'

# Features from `SUPPORTED_FEATURES` that Chromium knowingly doesn't enable,
# and the reasons why.
KNOWN_EXCLUSIONS = {
    'nonnull_smart_pointers': (
        'TODO(https://crbug.com/584725061): Enable this feature.  This '
        'requires adding `nullable.rs` (and `shared_ptr.rs` that it depends '
        'on) to `cc_std` in `//build/rust/crubit/BUILD.gn`.'
    ),
    'proto_references': (
        'This feature only affects C++ protobuf types, and Chromium '
        'doesn\'t use Crubit\'s protobuf integration.'
    ),
}


def ParseBzlFeatures(bzl_text):
    """Returns the `SUPPORTED_FEATURES` list from `global_features.bzl`.

    Starlark syntax is a subset of Python syntax, so `ast` can parse it.
    """
    try:
        assignment_node = next(
            node
            for node in ast.parse(bzl_text).body
            if isinstance(node, ast.Assign)
            and any(
                isinstance(t, ast.Name) and t.id == BZL_VARIABLE
                for t in node.targets
            )
        )
    except StopIteration as e:
        raise ValueError(f'Could not find `{BZL_VARIABLE} = ...`') from e

    try:
        assignment_value = ast.literal_eval(assignment_node.value)
    except ValueError as e:
        raise ValueError(f'`{BZL_VARIABLE}` is not a list of strings') from e

    return list(map(str, assignment_value))


def ParseGniList(gni_text, variable):
    """Returns the list from the `<variable> = [ ... ]` assignment in `.gni`.

    Later changes to the variable (e.g. `<variable> += ...`) are ignored.
    """
    match = re.search(
        rf'^{variable} = \[.*?^\]', gni_text, re.MULTILINE | re.DOTALL
    )
    if not match:
        raise ValueError(f'Could not find `{variable} = [ ... ]`')
    return gn_helpers.FromGNArgs(match.group(0))[variable]


def FindMissingFeatures(upstream, chromium, exclusions):
    """Returns upstream features that are not enabled in Chromium."""
    return sorted(set(upstream) - set(chromium) - set(exclusions))


def FindUnexpectedFeatures(upstream, chromium, fast_tracked):
    """Returns Chromium features that are not upstream nor fast-tracked."""
    return sorted(set(chromium) - set(upstream) - set(fast_tracked))


def FindStaleFastTrackedFeatures(upstream, fast_tracked):
    """Returns `fast_tracked` features that are upstream."""
    return sorted(set(fast_tracked) & set(upstream))


def FindStaleExclusions(upstream, chromium, exclusions):
    """Returns `exclusions` that are not upstream or are enabled in Chromium."""
    return sorted(f for f in exclusions if f not in upstream or f in chromium)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--bzl-path',
        help='read `global_features.bzl` from this path (e.g. from a local '
        'Crubit checkout), instead of downloading it from GitHub at '
        '`CRUBIT_REVISION`',
    )
    args = parser.parse_args()

    if args.bzl_path:
        bzl_source = args.bzl_path
        with open(args.bzl_path) as f:
            bzl_text = f.read()
    else:
        bzl_source = BZL_URL_FORMAT.format(revision=CRUBIT_REVISION)
        with urllib.request.urlopen(bzl_source, timeout=60) as response:
            bzl_text = response.read().decode('utf-8')
    upstream = ParseBzlFeatures(bzl_text)

    with open(GNI_PATH) as f:
        gni_text = f.read()
    globally_enabled = ParseGniList(gni_text, GNI_GLOBALLY_ENABLED_VARIABLE)
    fast_tracked = ParseGniList(gni_text, GNI_FAST_TRACKED_VARIABLE)
    chromium = globally_enabled + fast_tracked

    script = os.path.relpath(__file__, CHROMIUM_DIR)
    gni = os.path.relpath(GNI_PATH, CHROMIUM_DIR)
    missing = FindMissingFeatures(upstream, chromium, KNOWN_EXCLUSIONS)
    unexpected = FindUnexpectedFeatures(upstream, chromium, fast_tracked)
    stale_fast_tracked = FindStaleFastTrackedFeatures(upstream, fast_tracked)
    stale_exclusions = FindStaleExclusions(upstream, chromium, KNOWN_EXCLUSIONS)
    if missing:
        print(
            f'Upstream Crubit enables these features (see `{BZL_VARIABLE}` '
            f'in {bzl_source}), but Chromium doesn\'t (see '
            f'`{GNI_GLOBALLY_ENABLED_VARIABLE}` in //{gni}):'
        )
        for feature in missing:
            print(f'  * {feature}')
        print(
            'Please open a bug at https://crbug.com/new and assign it to '
            'lukasza@google.com, to enable these features in Chromium (or '
            f'to add them to `KNOWN_EXCLUSIONS` in //{script}).'
        )
    if unexpected:
        print(
            'Chromium enables these features, but upstream Crubit doesn\'t '
            f'(they are not in `{BZL_VARIABLE}` in {bzl_source}):'
        )
        for feature in unexpected:
            print(f'  * {feature}')
        print(
            f'Please remove them from `{GNI_GLOBALLY_ENABLED_VARIABLE}` in '
            f'//{gni} (or, if Chromium should enable them before upstream '
            f'Crubit does, move them to `{GNI_FAST_TRACKED_VARIABLE}`).'
        )
    if stale_fast_tracked:
        print(
            f'Upstream Crubit now enables these features from '
            f'`{GNI_FAST_TRACKED_VARIABLE}` in //{gni} - please move them to '
            f'`{GNI_GLOBALLY_ENABLED_VARIABLE}`:'
        )
        for feature in stale_fast_tracked:
            print(f'  * {feature}')
    if stale_exclusions:
        print(
            f'These entries in `KNOWN_EXCLUSIONS` in //{script} are stale '
            '(upstream Crubit doesn\'t enable them anymore, or Chromium '
            'already enables them) - please remove them:'
        )
        for feature in stale_exclusions:
            print(f'  * {feature}')
    if missing or unexpected or stale_fast_tracked or stale_exclusions:
        return 1

    print(
        f'OK: Chromium enables the same features as `{BZL_VARIABLE}` at '
        f'{bzl_source} (except `KNOWN_EXCLUSIONS` in //{script} and '
        f'`{GNI_FAST_TRACKED_VARIABLE}` in //{gni}).'
    )
    return 0


if __name__ == '__main__':
    sys.exit(main())
