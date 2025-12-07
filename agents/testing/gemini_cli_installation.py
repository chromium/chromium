# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for handling gemini-cli installations."""

import logging
import pathlib
import subprocess

CIPD_ROOT = pathlib.Path(__file__).resolve().parent / 'cipd' / 'gemini-cli'
CIPD_PACKAGES = [
    ('infra/3pp/tools/nodejs/linux-${arch}', 'version:3@25.0.0'),
    ('infra/3pp/npm/gemini-cli/linux-${arch}', 'version:3@0.9.0'),
]


def fetch_cipd_gemini_cli(verbose):
    # Note this cannot be in the same cipd root as promptfoo due to
    # node_modules folder conflicting
    logging.debug('Cipd root not initialized. Creating.')
    subprocess.check_call([
        'cipd',
        'init',
        '-force',
        str(CIPD_ROOT),
    ])
    for package, version in CIPD_PACKAGES:
        logging.debug('install %s@%s', package, version)
        subprocess.check_call(
            [
                'cipd',
                'install',
                package,
                version,
                '-root',
                CIPD_ROOT,
                '-log-level',
                'debug' if verbose else 'warning',
            ],
            stdout=subprocess.DEVNULL,
        )
    return (CIPD_ROOT / 'node_modules' / '.bin' / 'gemini',
            CIPD_ROOT / 'bin' / 'node')
