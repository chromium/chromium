# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from cca import cli
from cca import util


def _make_dir(path: str):
    util.run(['fileutil', 'mkdir', path])


def _upload_dir(local_path: str,
                remote_path: str,
                force: bool = True,
                recursive: bool = True):
    _make_dir(os.path.join(remote_path, os.path.basename(local_path)))
    cmd = ["fileutil", "cp"]
    if force:
        cmd.append("-f")
    if recursive:
        cmd.append("-R")
    cmd.extend([local_path, remote_path])
    util.run(cmd)


@cli.command(
    "upload",
    help="Upload CCA bundle to internal server",
    description="Upload CCA bundle to internal server for UI development",
)
@cli.option(
    "--remote-root-path",
    default="/google/data/rw/teams/chromeos-camera-app/",
    type=str,
    help="Expected CCA bundle uploaded location on server",
)
def cmd(remote_root_path: str) -> int:
    cca_root = os.getcwd()
    local_path = os.path.join(cca_root, 'dist')
    _upload_dir(local_path, remote_root_path)
    return 0
