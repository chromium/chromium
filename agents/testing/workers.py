# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for isolated workers that run promptfoo tests."""

import contextlib
import logging
import pathlib
import shutil
import subprocess
import time


class WorkDir(contextlib.AbstractContextManager):
    """A WorkDir used for testing destructive changes by an agent.

    Each workdir acts like a local shallow clone and has its own isolated
    checkout state (staging, untracked files, `//.gemini/extensions/`).
    """

    def __init__(
        self,
        name: str,
        src_root_dir: pathlib.Path,
        clean: bool,
        verbose: bool,
        force: bool,
        btrfs: bool,
    ):
        self.path = src_root_dir.parent.joinpath(name)
        self.src_root_dir = src_root_dir
        self.clean = clean
        self.verbose = verbose
        self.force = force
        self.btrfs = btrfs

    def __enter__(self) -> 'WorkDir':
        if self.path.exists():
            if self.force:
                self._clean()
            else:
                raise FileExistsError(
                    f'Workdir already exists at: {self.path}. Remove it '
                    'manually or use --force to remove it.')

        logging.info('Creating new workdir: %s', self.path)
        start_time = time.time()
        if self.btrfs:
            # gclient-new-workdir does the same thing but reflinks the .git dirs
            # which we don't need to waste time on
            subprocess.check_call(
                ['btrfs', 'subvol', 'snapshot', self.src_root_dir, self.path],
                stdout=None if self.verbose else subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
            )
        else:
            subprocess.check_call(
                ['gclient-new-workdir.py', self.src_root_dir, self.path],
                stdout=None if self.verbose else subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
            )
        logging.debug('Took %s seconds', time.time() - start_time)
        return self

    def __exit__(self, *_exc_info) -> None:
        if self.clean:
            self._clean()

    def _clean(self) -> None:
        logging.info('Removing existing workdir: %s', self.path)
        cmd = ['sudo', 'btrfs', 'subvolume', 'delete', self.path]
        start_time = time.time()
        if self.btrfs:
            if self.force:
                cmd.insert(1, '-n')
            result = subprocess.call(
                cmd,
                stdout=None if self.verbose else subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
            )
            if result != 0:
                logging.debug('Failed to remove with subvolume delete.')
        if not self.btrfs or result != 0:
            logging.debug('Removing with shutil')
            shutil.rmtree(self.path)
        logging.debug('Took %s seconds', time.time() - start_time)
