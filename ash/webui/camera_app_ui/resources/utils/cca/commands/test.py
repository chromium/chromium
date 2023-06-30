# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import List

from cca import cli
from cca import util


@cli.command("test", help="run tests", description="Run CCA tests on device.")
@cli.option("device")
@cli.option(
    "pattern",
    nargs="*",
    default=["camera.CCAUI*"],
    help="test patterns. (default: camera.CCAUI*)",
)
def cmd(device: str, pattern: List[str]):
    assert (
        "CCAUI" not in device
    ), "The first argument should be <device> instead of a test name pattern."
    cmd = ["cros_run_test", "--device", device, "--tast"] + pattern
    util.run(cmd)
