#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Entry point for orchestrate to expose the test environment."""

import argparse
import sys

from common import catch_sigterm, register_log_args
from test_env_setup import expose_env

if __name__ == '__main__':
    catch_sigterm()
    parser = argparse.ArgumentParser()
    register_log_args(parser)
    parser.add_argument(
        '--mypid',
        type=int,
        default=0,
        help='The pid used in the pid file name.',
    )
    args, _ = parser.parse_known_args()
    sys.exit(expose_env(args.logs_dir, args.mypid))
