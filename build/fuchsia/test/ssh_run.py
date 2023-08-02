#!/usr/bin/env vpython3

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A standalone tool to run a command on fuchsia through ssh."""

# Note, this is a temporary tool and should be removed in favor of a better way
# to expose the functionality or merge with other use cases of get_ssh_address.

import subprocess
import sys

from compatible_utils import get_ssh_prefix
from common import get_ssh_address


def main():
    """Execute a command against a fuchsia target via ssh."""
    if len(sys.argv) < 3:
        raise ValueError('ssh_run.py target command')

    ssh_prefix = get_ssh_prefix(get_ssh_address(sys.argv[1]))
    subprocess.run(ssh_prefix + ['--'] + sys.argv[2:], check=True)


if __name__ == '__main__':
    sys.exit(main())
