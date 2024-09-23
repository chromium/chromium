# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
import json
import pathlib
import re

from cra import util
import gen_images_js as gen_images_js_module


def gen_images_js() -> str:
    images_base = util.get_cra_root() / "images"

    with open(images_base / "images.gni") as f:
        gn = f.read()
        match = re.search(r"images\s*=\s*(\[.*?\])", gn, re.DOTALL)
        assert match is not None
        images = ast.literal_eval(match.group(1))
        match = re.search(r"icons\s*=\s*(\[.*?\])", gn, re.DOTALL)
        assert match is not None
        icons = ast.literal_eval(match.group(1))
        images += [f"icons/{icon}" for icon in icons]

    images = [images_base / image for image in images]

    return gen_images_js_module.gen_images_js(images, images_base)


# TODO(pihsun): This & _get_tsc_references is getting more and more
# complicated. Maybe we should just grab the built tsconfig_build_ts.json and
# modify the path to point to the correct location instead.
def _get_tsc_paths(build_dir: pathlib.Path) -> dict[str, list[str]]:
    root_dir = util.get_chromium_root()
    images_dir = util.get_cra_root() / "images"

    resources_dir = build_dir / "gen/ui/webui/resources/tsc"

    mwc_dir = root_dir / "third_party/material_web_components"
    lit_d_ts = mwc_dir / "lit_exports.d.ts"
    mwc_components_dir = mwc_dir / "components-chromium/node_modules/@material"

    cros_components_dir = resources_dir / "cros_components/to_be_rewritten"

    metrics_dir = (
        build_dir /
        "gen/ash/webui/common/resources/preprocessed/metrics")

    return {
        "//resources/*": [str(resources_dir / "*")],
        "chrome://resources/*": [str(resources_dir / "*")],
        "chrome://resources/mwc/lit/index.js": [str(lit_d_ts)],
        "chrome://resources/mwc/@material/*": [str(mwc_components_dir / "*")],
        "chrome://resources/cros_components/*":
        [str(cros_components_dir / "*")],
        "/images/*": [str(images_dir / "*")],
        "chrome://resources/ash/common/metrics/*": [str(metrics_dir / "*")],
    }


def _make_mojom_symlink(build_dir: pathlib.Path):
    cra_root = util.get_cra_root()
    root_dir = util.get_chromium_root()

    preprocessed_mojo_dir = build_dir / "gen" / cra_root.relative_to(
        root_dir) / "preprocessed/mojom"
    cra_mojo_dir = cra_root / "mojom"

    assert preprocessed_mojo_dir.exists(), (
        f"{preprocessed_mojo_dir} doesn't exist, "
        "is Chrome compiled at least once?")

    if cra_mojo_dir.is_symlink():
        if cra_mojo_dir.readlink() != preprocessed_mojo_dir:
            # There's a symlink here that's not pointing to the correct path.
            # This might happen when changing build_dir. Remove the symlink and
            # recreate in this case.
            cra_mojo_dir.unlink()
            cra_mojo_dir.symlink_to(preprocessed_mojo_dir)
    elif cra_mojo_dir.exists():
        # Some other things are at the mojom path. cra.py won't work in
        # this case.
        raise Exception("resources/mojom exists but not a symlink."
                        " Please remove it and try again.")
    else:
        cra_mojo_dir.symlink_to(preprocessed_mojo_dir)


def _get_tsc_references(build_dir: pathlib.Path) -> list[dict[str, str]]:
    mwc_gen_dir = build_dir / "gen/third_party/material_web_components/"
    cros_components_tsconfig = (
        build_dir /
        "gen/third_party/cros-components/tsconfig_cros_components_ts.json")

    ash_common_tsconfig = (
        build_dir /
        "gen/ash/webui/common/resources/tsconfig_build_ts.json")

    return [{
        "path": str(mwc_gen_dir / "tsconfig_library.json")
    }, {
        "path": str(mwc_gen_dir / "tsconfig_bundle_lit_ts.json")
    }, {
        "path": str(cros_components_tsconfig)
    }, {
        "path": str(ash_common_tsconfig)
    }]


def generate_tsconfig(build_dir: pathlib.Path):
    # TODO(pihsun): This needs to be in sync with BUILD.gn, have some heuristic
    # to get the dependency from there or from the generated tsconfig.json
    # instead?
    cra_root = util.get_cra_root()

    with open(cra_root / "tsconfig_base.json") as f:
        tsconfig = json.load(f)

    _make_mojom_symlink(build_dir)

    tsconfig["files"] = [str(p) for p in cra_root.glob("**/*.ts")]
    tsconfig["compilerOptions"]["rootDir"] = str(cra_root)
    tsconfig["compilerOptions"]["noEmit"] = True
    tsconfig["compilerOptions"]["paths"] = _get_tsc_paths(build_dir)
    tsconfig["compilerOptions"]["plugins"] = [{
        "name": "ts-lit-plugin",
        "strict": True
    }]
    tsconfig["references"] = _get_tsc_references(build_dir)

    with open(cra_root / "tsconfig.json", "w") as f:
        json.dump(tsconfig, f)
