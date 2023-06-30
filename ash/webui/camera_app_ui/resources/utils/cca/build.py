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


def build_preload_images_js(outdir: str):
    with open("images/images.gni") as f:
        match = re.search(r"in_app_images\s*=\s*(\[.*?\])", f.read(),
                          re.DOTALL)
        assert match is not None
        in_app_images = ast.literal_eval(match.group(1))

    preload_images_js_path = os.path.join(outdir, "preload_images.js")
    if os.path.exists(preload_images_js_path):
        with open(preload_images_js_path) as f:
            preload_images_js = f.read()
    else:
        preload_images_js = None

    with tempfile.NamedTemporaryFile("w") as f:
        f.writelines(
            os.path.abspath(f"images/{asset}") + "\n"
            for asset in in_app_images)
        f.flush()
        with tempfile.NamedTemporaryFile("r") as temp_file:
            cmd = [
                "utils/gen_preload_images_js.py",
                "--images_list_file",
                f.name,
                "--output_file",
                temp_file.name,
            ]
            util.run(cmd)

            new_preload_images_js = temp_file.read()
            # Only write when the generated preload_images.js changes, to avoid
            # changing mtime of the preload_images.js file when the images are
            # not changed, so rsync won't copy the file again on deploy.
            if new_preload_images_js == preload_images_js:
                return
            with open(preload_images_js_path, "w") as output_file:
                output_file.write(new_preload_images_js)


def _get_tsc_paths(board: str) -> Dict[str, List[str]]:
    root_dir = util.get_chromium_root()
    target_gen_dir = os.path.join(root_dir, f"out_{board}/Release/gen")

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
    target_gen_dir = os.path.join(root_dir, f"out_{board}/Release/gen")
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
    root_dir = util.get_chromium_root()
    target_gen_dir = os.path.join(root_dir, f"out_{board}/Release/gen")
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

    target_gen_dir = os.path.join(root_dir, f"out_{board}/Release/gen")
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
