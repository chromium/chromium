# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import shutil
from typing import Set

from cca import build
from cca import cli
from cca import util
from cca.commands import dev as dev_cmd

_BUNDLE_TSC_OUTPUT_TEMP_DIR = "/tmp/cca-bundle-out"


@cli.command(
    "bundle",
    help="bundle CCA",
    description="bundle CCA as a static website that can be served by any "
    "static HTTP file server",
)
# TODO(pihsun): Should we derive the MWC tsconfig_library.json directly from
# BUILD.gn, so the board argument isn't needed?
@cli.option(
    "board",
    help=("board name. "
          "Use any board name with Chrome already built. "
          "The provided board name is used for finding MWC and lit, "
          "which is board independent. "
          "All other board dependent references will be stubbed."),
)
def cmd(board: str) -> int:
    os.makedirs(_BUNDLE_TSC_OUTPUT_TEMP_DIR, exist_ok=True)

    cca_root = os.getcwd()

    build.generate_tsconfig(board)

    util.run_node([
        "typescript/bin/tsc",
        "--outDir",
        _BUNDLE_TSC_OUTPUT_TEMP_DIR,
        "--noEmit",
        "false",
        # Makes compilation faster
        "--incremental",
        # For better debugging experience.
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

    handler = dev_cmd.RequestHandler(cca_root, _BUNDLE_TSC_OUTPUT_TEMP_DIR,
                                     util.get_gen_dir(board))
    routes = handler.routes

    output_folder = os.path.join(cca_root, "dist")
    shutil.rmtree(output_folder, ignore_errors=True)
    os.makedirs(output_folder, exist_ok=True)

    to_output_files: Set[str] = set()

    # TODO(pihsun): We don't need all the images.
    top_level_folders = [
        os.path.join(cca_root, folder)
        for folder in ["css", "images", "views", "sounds"]
    ] + [os.path.join(_BUNDLE_TSC_OUTPUT_TEMP_DIR, "js")]

    for folder in top_level_folders:
        folder_name = os.path.basename(folder)
        for dirpath, _, filenames in os.walk(folder):
            relative_dir = os.path.relpath(dirpath, folder)
            for file in filenames:
                if file.endswith(".gni"):
                    continue
                to_output_files.add(
                    os.path.normpath(
                        os.path.join(folder_name, relative_dir, file)))

    to_output_files.add("js/lib/ffmpeg.wasm")

    # TODO(pihsun): Include all mwc / cros_component into the bundle.
    to_output_files.add("chrome_stub/resources/mwc/lit/index.js")

    def write_response_to(response: bytes, path: str):
        output_path = os.path.join(output_folder, path)
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        with open(output_path, "wb") as fout:
            fout.write(response)

    for route in routes:
        if isinstance(route.pattern, re.Pattern):
            for file in set(to_output_files):
                request_path = f"/{file}"
                if route.pattern.fullmatch(request_path):
                    logging.debug(f"{route.pattern} -> {file}")
                    to_output_files.remove(file)
                    resp = route.handler(request_path)
                    write_response_to(resp, file)
        else:
            path = route.pattern.lstrip("/")
            logging.debug(f"{route.pattern} -> {path}")
            resp = route.handler(route.pattern)
            write_response_to(resp, path)
            if path in to_output_files:
                to_output_files.remove(path)

    if to_output_files:
        logging.warning(
            f"Some files are not covered by route: {to_output_files}.")

    return 0
