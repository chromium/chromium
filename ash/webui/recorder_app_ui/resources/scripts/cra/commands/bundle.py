# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import pathlib
import queue
import re
import shutil
from typing import Optional, TypeVar

from cra import build
from cra import cli
from cra import util
from cra.commands import dev as dev_cmd

_BUNDLE_TSC_OUTPUT_TEMP_DIR = pathlib.Path("/tmp/cra-bundle-out")

T = TypeVar('T')

_RequestPath = pathlib.PurePosixPath


def _assert_exist(val: Optional[T]) -> T:
    assert val is not None
    return val


def _get_static_paths(cra_root: pathlib.Path) -> set[_RequestPath]:
    static_paths = set([
        _RequestPath('index.html'),
        _RequestPath("chrome_stub/theme/typography.css"),
        _RequestPath("chrome_stub/theme/colors.css"),
    ])
    static_root = cra_root / "static"

    # Path.walk() is only available in Python 3.12, and glinux default system
    # Python is at 3.11.
    # TODO(pihsun): Change to Path.walk when newer Python is available by
    # default.
    for dirpath, _, filenames in os.walk(static_root):
        dirpath = pathlib.Path(dirpath)
        relative_dir = dirpath.relative_to(static_root)
        static_base = _RequestPath(static_root.name)
        for file in filenames:
            if file.endswith(".gni"):
                continue
            static_paths.add(static_base / relative_dir / file)

    return static_paths


@cli.command(
    "bundle",
    help="bundle cra",
    description="bundle cra as a static website that can be served by any "
    "static HTTP file server with SPA support",
)
@util.build_dir_option()
def cmd(build_dir: pathlib.Path) -> int:
    _BUNDLE_TSC_OUTPUT_TEMP_DIR.mkdir(parents=True, exist_ok=True)

    build.generate_tsconfig(build_dir)

    cra_root = util.get_cra_root()

    util.run_node(
        [
            "typescript/bin/tsc",
            "--outDir",
            str(_BUNDLE_TSC_OUTPUT_TEMP_DIR),
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
        ],
        cwd=cra_root)

    handler = dev_cmd.RequestHandler(cra_root, _BUNDLE_TSC_OUTPUT_TEMP_DIR,
                                     build_dir, util.get_strings_dir())

    output_folder = cra_root / "dist"
    shutil.rmtree(output_folder, ignore_errors=True)
    output_folder.mkdir(parents=True, exist_ok=True)

    # Gets the response from `path` and write the response to output bundle
    # folder.
    def handle(path: _RequestPath) -> bytes:
        if path.is_absolute():
            path = path.relative_to(_RequestPath("/"))
        logging.debug(f'Processing {path}')
        response = _assert_exist(handler.handle(path))[0]
        output_path = output_folder / path
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, "wb") as fout:
            fout.write(response)
        return response

    for static_path in _get_static_paths(cra_root):
        handle(static_path)

    js_paths = queue.Queue[_RequestPath]()
    added_js_paths = set[_RequestPath]()

    def add_path(path: _RequestPath):
        if path in added_js_paths:
            return
        js_paths.put(path)
        added_js_paths.add(path)

    add_path(_RequestPath("init.js"))

    while not js_paths.empty():
        path = js_paths.get_nowait()
        dir = path.parent
        response = handle(path).decode()
        for match in re.finditer(r'^import\s+(?:[^;]*?)([\'"])([^\'"]*?)\1',
                                 response, re.MULTILINE | re.DOTALL):
            # Why is there no normpath in pathlib... T_T
            import_path = _RequestPath(os.path.normpath(dir / match[2]))
            add_path(import_path)

    return 0
