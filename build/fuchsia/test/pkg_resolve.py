#!/usr/bin/env vpython3

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A standalone tool to resolve a list of packages."""

# Note, this is a temporary tool and should be removed in favor of a better way
# to expose the functionality or merge with other use cases of resolve_packages.

import sys

from common import resolve_packages


def main():
    """Resolve a list of packages on a target."""
    if len(sys.argv) < 3:
        raise ValueError('pkg_resolve.py target [list of packages]')
    resolve_packages(sys.argv[2:], sys.argv[1])


if __name__ == '__main__':
    sys.exit(main())
