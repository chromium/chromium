# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shlex
import subprocess
import time
from typing import List

from cca import cli
from cca import build
from cca import util

_CCA_OVERRIDE_PATH = "/etc/camera/cca"
_CCA_OVERRIDE_FEATURE = "CCALocalOverride"
_CHROME_DEV_CONF_PATH = "/etc/chrome_dev.conf"


def _local_override_enabled(device: str) -> bool:
    chrome_dev_conf = util.check_output(
        ["ssh", device, "--", "cat", _CHROME_DEV_CONF_PATH])
    # This is a simple heuristic that is not 100% accurate, since this only
    # matches the feature name which can be in other irrevelant position in the
    # file. This should be fine though since this is only used for developers
    # and it's not expected to have the exact string match outside of
    # --enable-features added by this script.
    return _CCA_OVERRIDE_FEATURE in chrome_dev_conf


def _ensure_local_override_enabled(device: str, force: bool):
    if _local_override_enabled(device):
        return
    util.run([
        "ssh",
        device,
        "--",
        f'echo "--enable-features={_CCA_OVERRIDE_FEATURE}"' +
        f" >> {_CHROME_DEV_CONF_PATH}",
    ])
    if not force:
        prompt = input("Need to restart UI for deploy to take effect, " +
                       "do it now? (y/N): ").lower()
        if prompt != "y":
            print(
                "Not restarting UI. " +
                "`restart ui` on DUT manually for the change to take effect.")
            return
    util.run(["ssh", device, "--", "restart", "ui"])


# Script to reload all CSS on the page by appending a different search
# parameter to the URL each time this is run. Note that Date.now() has
# milliseconds accuracy, so in practice multiple run of the cca.py deploy
# script will have different search parameter.
_CSS_RELOAD_SCRIPT = """
for (const link of document.querySelectorAll('link[rel="stylesheet"]')) {
    const url = new URL(link.href);
    url.searchParams.set('cca-deploy-refresh', Date.now().toString());
    link.href = url.toString();
}
console.log('All CSS reloaded');
"""


def _can_only_reload_css(changed_files: List[str]) -> bool:
    for file in changed_files:
        # Ignore deployed_version.js since this always change every deploy, and
        # doesn't affect anything other than the startup console log and toast.
        if file.endswith("/deployed_version.js"):
            continue
        # Ignore folders.
        if file.endswith("/"):
            continue
        # .css change is okay.
        if file.endswith(".css"):
            continue
        return False
    return True


def _reload_cca(device: str, changed_files: List[str]):
    try:
        reload_script = "document.location.reload()"
        if _can_only_reload_css(changed_files):
            reload_script = _CSS_RELOAD_SCRIPT
        util.run([
            "ssh",
            device,
            "--",
            "cca",
            "open",
            "&&",
            "cca",
            "eval",
            shlex.quote(reload_script),
            ">",
            "/dev/null",
        ])
    except subprocess.CalledProcessError as e:
        print("Failed to reload CCA on DUT, "
              "please make sure that the DUT is logged in "
              "and `cca setup` has been run on DUT.")


# Use a fixed temporary output folder for deploy, so incremental compilation
# works and deploy is faster.
_DEPLOY_OUTPUT_TEMP_DIR = "/tmp/cca-deploy-out"


def _rsync_to_device(device: str,
                     src: str,
                     target: str,
                     *,
                     extra_arguments: List[str] = []):
    """Returns list of files that are changed."""
    cmd = [
        "rsync",
        "--recursive",
        "--inplace",
        "--delete",
        "--mkpath",
        "--times",
        # rsync by default use source file permission masked by target file
        # system umask while transferring new files, and since workstation
        # defaults to have file not readable by others, this makes deployed
        # file not readable by Chrome.
        # Set --chmod=a+rX to rsync to fix this ('a' so it won't be affected by
        # local umask, +r for read and +X for executable bit on folder), and
        # set --perms so existing files that might have the wrong permission
        # will have their permission fixed.
        "--perms",
        "--chmod=a+rX",
        # Sets rsync output format to %n which prints file path that are
        # changed. (By default rsync only copies file that have different size
        # or modified time.)
        "--out-format=%n",
        *extra_arguments,
        src,
        f"{device}:{target}",
    ]
    output = util.check_output(cmd)
    return [os.path.join(target, file) for file in output.splitlines()]


@cli.command(
    "deploy",
    help="deploy to device",
    description=(
        "Deploy CCA to device. "
        "This script only works if there's no .cc / .grd changes. "
        "And please build Chrome at least once before running the command."),
)
@cli.option("board")
@cli.option("device")
@cli.option(
    "--force",
    help="Don't prompt for restarting Chrome.",
    action="store_true",
)
@cli.option(
    "--reload",
    help="Try reloading CCA window after deploy. "
    "Please run `cca setup` on DUT once before using this argument.",
    action="store_true",
)
def cmd(board: str, device: str, force: bool, reload: bool):
    cca_root = os.getcwd()

    os.makedirs(_DEPLOY_OUTPUT_TEMP_DIR, exist_ok=True)
    js_out_dir = os.path.join(_DEPLOY_OUTPUT_TEMP_DIR, "js")

    build.generate_tsconfig(board)

    util.run_node([
        "typescript/bin/tsc",
        "--outDir",
        _DEPLOY_OUTPUT_TEMP_DIR,
        "--noEmit",
        "false",
        # Makes compilation faster
        "--incremental",
        # For better debugging experience on DUT.
        "--inlineSourceMap",
        "--inlineSources",
        # Makes devtools show TypeScript source with better path
        "--sourceRoot",
        "/",
        # For easier developing / test cycle.
        "--noUnusedLocals",
        "false",
        "--noUnusedParameters",
        "false",
    ])

    build.build_preload_images_js(js_out_dir)

    # Note that although we always rerun tsc, when the JS inputs are not
    # changed, tsc also doesn't change the output file's mtime, so rsync will
    # correctly skip those unchanged files.
    changed_files = _rsync_to_device(
        device,
        f"{js_out_dir}/",
        f"{_CCA_OVERRIDE_PATH}/js/",
        extra_arguments=["--exclude=tsconfig.tsbuildinfo"],
    )

    for dir in ["css", "images", "views", "sounds"]:
        changed_files += _rsync_to_device(
            device,
            f"{os.path.join(cca_root, dir)}/",
            f"{_CCA_OVERRIDE_PATH}/{dir}/",
        )

    current_time = time.strftime("%F %T%z")
    util.run([
        "ssh",
        device,
        "--",
        "printf",
        "%s",
        shlex.quote(
            f'export const DEPLOYED_VERSION = "cca.py deploy {current_time}";'
        ),
        ">",
        f"{_CCA_OVERRIDE_PATH}/js/deployed_version.js",
    ])

    _ensure_local_override_enabled(device, force)

    if reload:
        _reload_cca(device, changed_files)
