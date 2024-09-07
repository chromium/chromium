#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Ensure files in the directory are thoroughly tested."""

import importlib
import io
import os
import sys
import unittest

import coverage  # pylint: disable=import-error

# The files need to have sufficient coverages.
COVERED_FILES = [
    'compatible_utils.py', 'deploy_to_fuchsia.py', 'flash_device.py',
    'log_manager.py', 'publish_package.py', 'serve_repo.py'
]

# The files will be tested without coverage requirements.
TESTED_FILES = [
    'bundled_test_runner.py', 'common.py', 'ffx_emulator.py',
    'modification_waiter.py', 'monitors.py', 'serial_boot_device.py',
    'test_env_setup.py', 'test_server.py'
]


def main():
    """Gather coverage data, ensure included files are 100% covered."""

    # Fuchsia tests not supported on Windows
    if os.name == 'nt':
        return 0

    cov = coverage.coverage(data_file=None,
                            include=COVERED_FILES,
                            config_file=True)
    cov.start()

    for file in COVERED_FILES + TESTED_FILES:
        print('Testing ' + file + ' ...')
        # pylint: disable=import-outside-toplevel
        # import tests after coverage start to also cover definition lines.
        module = importlib.import_module(file.replace('.py', '_unittests'))
        # pylint: enable=import-outside-toplevel

        tests = unittest.TestLoader().loadTestsFromModule(module)
        if not unittest.TextTestRunner().run(tests).wasSuccessful():
            return 1

    cov.stop()
    outf = io.StringIO()
    percentage = cov.report(file=outf, show_missing=True)
    if int(percentage) != 100:
        print(outf.getvalue())
        print('FATAL: Insufficient coverage (%.f%%)' % int(percentage))
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
