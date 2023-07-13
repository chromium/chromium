# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
import glob
import json
import os
import re
import tempfile
from typing import Dict, List

from cca import util
import gen_preload_images_js as gen_preload_images_js_module


def gen_preload_images_js() -> str:
    with open("images/images.gni") as f:
        gn = f.read()
        match = re.search(r"in_app_images\s*=\s*(\[.*?\])", gn, re.DOTALL)
        assert match is not None
        in_app_images = ast.literal_eval(match.group(1))

        match = re.search(r"standalone_images\s*=\s*(\[.*?\])", gn, re.DOTALL)
        assert match is not None
        standalone_images = ast.literal_eval(match.group(1))

    in_app_images = [
        os.path.abspath(f"images/{asset}") for asset in in_app_images
    ]
    standalone_images = [
        os.path.abspath(f"images/{asset}") for asset in standalone_images
    ]

    return gen_preload_images_js_module.gen_preload_images_js(
        in_app_images, standalone_images)


def build_preload_images_js(outdir: str):
    preload_images_js_path = os.path.join(outdir, "preload_images.js")
    if os.path.exists(preload_images_js_path):
        with open(preload_images_js_path) as f:
            preload_images_js = f.read()
    else:
        preload_images_js = None

    new_preload_images_js = gen_preload_images_js()

    # Only write when the generated preload_images.js changes, to avoid
    # changing mtime of the preload_images.js file when the images are
    # not changed, so rsync won't copy the file again on deploy.
    if new_preload_images_js == preload_images_js:
        return
    with open(preload_images_js_path, "w") as output_file:
        output_file.write(new_preload_images_js)


def _get_tsc_paths(board: str) -> Dict[str, List[str]]:
    root_dir = util.get_chromium_root()
    target_gen_dir = util.get_gen_dir(board)

    resources_dir = os.path.join(target_gen_dir, "ui/webui/resources/tsc/*")

    lit_d_ts = os.path.join(
        root_dir, "third_party/material_web_components/lit_exports.d.ts")

    return {
        "//resources/*": [os.path.relpath(resources_dir)],
        "chrome://resources/*": [os.path.relpath(resources_dir)],
        "chrome://resources/mwc/lit/index.js": [os.path.relpath(lit_d_ts)],
    }


def _make_mojom_symlink(board: str):
    cca_root = os.getcwd()
    root_dir = util.get_chromium_root()
    target_gen_dir = util.get_gen_dir(board)
    src_relative_dir = os.path.relpath(cca_root, root_dir)
    generated_mojom_dir = os.path.join(target_gen_dir, src_relative_dir,
                                       "mojom")
    target = os.path.join(cca_root, "mojom")

    if os.path.islink(target):
        if os.readlink(target) != generated_mojom_dir:
            # There's a symlink here that's not pointing to the correct path.
            # This might happen when changing board. Remove the symlink and
            # recreate in this case.
            os.remove(target)
            os.symlink(generated_mojom_dir, target)
    elif os.path.exists(target):
        # Some other things are at the mojom path. cca.py won't work in
        # this case.
        raise Exception("resources/mojom exists but not a symlink."
                        " Please remove it and try again.")
    else:
        os.symlink(generated_mojom_dir, target)


def _get_tsc_references(board: str) -> List[Dict[str, str]]:
    target_gen_dir = util.get_gen_dir(board)
    mwc_tsconfig_path = os.path.join(
        target_gen_dir,
        "third_party/material_web_components/tsconfig_library.json",
    )

    return [{"path": os.path.relpath(mwc_tsconfig_path)}]


def generate_tsconfig(board: str):
    cca_root = os.getcwd()
    # TODO(pihsun): This needs to be in sync with BUILD.gn, have some heuristic
    # to get the dependency from there or from the generated tsconfig.json
    # instead?
    root_dir = util.get_chromium_root()
    common_definitions = os.path.join(root_dir, "tools/typescript/definitions")

    target_gen_dir = util.get_gen_dir(board)
    assert os.path.exists(target_gen_dir), (
        f"Failed to find the build output dir {target_gen_dir}."
        " Please check the board name and build Chrome once.")

    with open(os.path.join(cca_root, "tsconfig_base.json")) as f:
        tsconfig = json.load(f)

    _make_mojom_symlink(board)

    tsconfig["files"] = glob.glob("js/**/*.ts", recursive=True)
    tsconfig["files"].append(os.path.join(common_definitions, "pending.d.ts"))
    tsconfig["compilerOptions"]["rootDir"] = cca_root
    tsconfig["compilerOptions"]["noEmit"] = True
    tsconfig["compilerOptions"]["paths"] = _get_tsc_paths(board)
    tsconfig["compilerOptions"]["plugins"] = [{
        "name": "ts-lit-plugin",
        "strict": True
    }]
    tsconfig["references"] = _get_tsc_references(board)

    with open(os.path.join(cca_root, "tsconfig.json"), "w") as f:
        json.dump(tsconfig, f)
