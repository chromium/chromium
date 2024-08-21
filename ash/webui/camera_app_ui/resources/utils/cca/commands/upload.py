# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from cca import cli
from cca import util


def _make_dir(path: str):
    util.run(['fileutil', 'mkdir', path])


def _upload_dir(name: str,
                local_path: str,
                remote_path: str,
                force: bool = True,
                recursive: bool = True):
    _make_dir(os.path.join(remote_path, name))
    cmd = ["fileutil", "cp"]
    if force:
        cmd.append("-f")
    if recursive:
        cmd.append("-R")
    cmd.extend([local_path, os.path.join(remote_path, name)])
    util.run(cmd)
    url = ("https://x20.corp.google.com"
           f"/teams/chromeos-camera-app/cca-bundle/{name}")
    print("Upload Successfully."
          f" Click the below URL to open your CCA bundle:\n{url}")


def _delete_dir(path: str, force: bool = True, recursive: bool = True):
    cmd = ["fileutil", "rm"]
    if force:
        cmd.append("-f")
    if recursive:
        cmd.append("-R")
    cmd.append(path)
    util.run(cmd)


def _check_path_exist(remote_root_path: str, name: str) -> bool:
    cmd = [
        "fileutil", "find", remote_root_path, "--", "-name", name, "-maxdepth",
        "1"
    ]
    output = util.check_output(cmd).strip()
    return output == os.path.join(remote_root_path, name)


def _confirm_name_deletion(name) -> bool:
    while True:
        message = (
            "CCA bundle directory with name: "
            f"{name} already exists."
            "\nPress 'Y' to overwrite the old directory with the new one. "
            "[Y/n]:")
        response = input(message).lower()
        if response == "y" or response == "":
            return True
        elif response == "n":
            return False
        else:
            print("Invalid response. Please enter 'Y' or 'n'.")


@cli.command(
    "upload",
    help="Upload CCA bundle to internal server",
    description="Upload CCA bundle to internal server for UI development",
)
@cli.option(
    "name",
    help="Uploaded CCA bundle directory name.",
)
@cli.option(
    "--remote-root-path",
    default="/google/data/rw/teams/chromeos-camera-app/cca-bundle",
    type=str,
    help="Expected CCA bundle uploaded location on server",
)
def cmd(name: str, remote_root_path: str) -> int:
    cca_root = os.getcwd()
    local_path = os.path.join(cca_root, 'dist/*')
    remote_path = os.path.join(remote_root_path, name)
    if _check_path_exist(remote_root_path, name):
        if _confirm_name_deletion(name):
            _delete_dir(remote_path)
        else:
            return 0
    _upload_dir(name, local_path, remote_root_path)
    return 0
