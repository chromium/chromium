#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Ensure files in the directory are thoroughly tested."""

import io
import sys
import unittest

import coverage  # pylint: disable=import-error


def main():
    """Gather coverage data, ensure included files are 100% covered."""
    cov = coverage.coverage(data_file=None,
                            include='publish_package.py',
                            config_file=True)
    cov.start()
    # pylint: disable=import-outside-toplevel
    # import tests after coverage start to also cover definition lines.
    import publish_package_unittests
    # pylint: enable=import-outside-toplevel
    suite = unittest.TestLoader().loadTestsFromModule(
        publish_package_unittests)
    if not unittest.TextTestRunner().run(suite).wasSuccessful():
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
