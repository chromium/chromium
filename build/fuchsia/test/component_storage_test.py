#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs component_storage tests on a fuchsia emulator."""

import logging
import os
import subprocess
import sys
import tempfile

from pathlib import Path

from component_storage import ComponentStorage
from isolate_daemon import IsolateDaemon
from test_env_setup import wait_for_env_setup

LOG_DIR = os.getenv('ISOLATED_OUTDIR', '/tmp')
TMP = os.getenv('TMPDIR', '/tmp')


def main() -> int:
    """Entry of the test."""
    extra_args = []
    for arg in sys.argv:
        if not arg.startswith(
            '--isolated-script-test-output'
        ) and not arg.startswith('--isolated-script-test-perf-output'):
            extra_args.append(arg)
    proc = subprocess.Popen(
        [
            os.path.join(os.path.dirname(__file__), 'test_env_setup.py'),
            '--logs-dir',
            LOG_DIR,
        ]
        + extra_args
    )
    assert wait_for_env_setup(proc, LOG_DIR)
    logging.warning('test_env_setup.py is running on process %s', proc.pid)

    storage = ComponentStorage('core/feedback')

    with tempfile.TemporaryDirectory(dir=TMP) as tmp_dir:
        src_file = os.path.join(tmp_dir, 'component_storage_test.txt')
        Path(src_file).write_text(
            'this is a line of meaningless content for testing only.'
        )
        for capability in ('data', 'tmp'):
            storage.push(src_file, '.', capability)
            assert 'component_storage_test.txt' in storage.list('.', capability)
            # '/' should behave the same as '.'
            assert storage.list('/', capability) == storage.list(
                '.', capability
            )

            pull_dir = os.path.join(tmp_dir, capability)
            os.makedirs(pull_dir)
            storage.pull('component_storage_test.txt', pull_dir, capability)
            assert (
                Path(
                    os.path.join(pull_dir, 'component_storage_test.txt')
                ).read_text()
                == Path(src_file).read_text()
            )

            # There isn't a way to cat the file directly by using storage
            # component; it has to be copied back.
            pulled_file = os.path.join(tmp_dir, f'{capability}_pulled.txt')
            storage.pull('component_storage_test.txt', pulled_file, capability)
            assert Path(pulled_file).read_text() == Path(src_file).read_text()

            storage.delete('component_storage_test.txt', capability)
            assert 'component_storage_test.txt' not in storage.list(
                '.', capability
            )

    proc.terminate()
    return proc.wait()


if __name__ == '__main__':
    # Creates the isolate dir for ffx to ensure it can be shared across the
    # processes.
    with IsolateDaemon.IsolateDir():
        sys.exit(main())
