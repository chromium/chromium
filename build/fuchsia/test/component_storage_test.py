#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Runs component_storage tests on a fuchsia emulator. """

import logging
import os
import subprocess
import sys

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
                '--isolated-script-test-output') and not arg.startswith(
                    '--isolated-script-test-perf-output'):
            extra_args.append(arg)
    proc = subprocess.Popen([
        os.path.join(os.path.dirname(__file__), 'test_env_setup.py'),
        '--logs-dir', LOG_DIR
    ] + extra_args)
    assert wait_for_env_setup(proc, LOG_DIR)
    logging.warning('test_env_setup.py is running on process %s', proc.pid)

    storage = ComponentStorage('core/feedback')

    assert len(storage.list('.', 'data')) > 0
    # '/' should behave the same as '.'
    assert storage.list('/', 'data') == storage.list('.', 'data')

    storage.pull('build_version.txt', TMP, 'data')
    assert len(Path(os.path.join(TMP, 'build_version.txt')).read_text()) > 0

    Path(os.path.join(TMP, 'tmp.txt')).write_text(
        'this is a line of meaningless content for testing only.')
    storage.push(os.path.join(TMP, 'tmp.txt'), '.', 'tmp')
    assert 'tmp.txt' in storage.list('.', 'tmp')
    # There isn't a way to cat the file directly by using storage component; it
    # has to be copied back.
    storage.pull('tmp.txt', os.path.join(TMP, 'tmp2.txt'), 'tmp')
    assert Path(os.path.join(TMP, 'tmp.txt')).read_text() == Path(
        os.path.join(TMP, 'tmp2.txt')).read_text()

    storage.delete('tmp.txt', 'tmp')
    assert 'tmp.txt' not in storage.list('.', 'tmp')

    proc.terminate()
    return proc.wait()


if __name__ == '__main__':
    # Creates the isolate dir for daemon to ensure it can be shared across the
    # processes.
    with IsolateDaemon.IsolateDir():
        sys.exit(main())
