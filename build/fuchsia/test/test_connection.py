#!/usr/bin/env vpython3

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A standalone tool to test the connection of a target."""

# Note, this is a temporary tool and should be removed in favor of a better way
# to expose the functionality or merge with other use cases of get_ssh_address.

import sys

from ffx_integration import test_connection


def main():
    """Test a connection against a fuchsia target via ffx."""
    if len(sys.argv) < 2:
        raise ValueError('test_connection.py target')
    test_connection(sys.argv[1])


if __name__ == '__main__':
    sys.exit(main())
