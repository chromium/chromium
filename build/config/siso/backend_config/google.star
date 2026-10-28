# -*- bazel-starlark -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso backend config for Google."""

load("@builtin//struct.star", "module")

def __is_cros_rbe(ctx):
    """Whether the build uses a ChromeOS RBE instance.

    projects/chromeos-*-bot instances are used by the chromeos-chrome ebuild
    in the ChromeOS SDK and by SimpleChrome. Unlike the Chrome instances they
    have no action_default/action_large worker pools and run actions under
    gVisor.
    """
    return ctx.flags.get("reapi_instance", "").startswith("projects/chromeos-")

def __platform_properties(ctx):
    container_image = "docker://gcr.io/chops-public-images-prod/rbe/siso-chromium/linux@sha256:2ab815965d0886100cec5495c53757698d7c73fa21b344b42ff182234391e47a"

    if __is_cros_rbe(ctx):
        # Note: this intentionally does not use dockerChrootPath=. (chroot
        # into the input root). Chrome's bundled clang is dynamically linked,
        # and in chroot mode the chroot's /lib64/ld-linux-x86-64.so.2, libc
        # etc. would have to be uploaded too. The chromeos-chrome ebuild bind
        # mounts the build dir and the board sysroot into the Chromium
        # checkout instead, so all inputs are inside the siso workspace and
        # the standard Chromium layout applies (crrev.com/c/8535406).
        props = {
            "OSFamily": "Linux",
            "container-image": container_image,
            "dockerRuntime": "runsc",
        }
        return {
            "default": props,
            "large": props,
        }
    return {
        "default": {
            "OSFamily": "Linux",
            "container-image": container_image,
            "label:action_default": "1",
        },
        # Large workers are usually used for Python actions like generate bindings, mojo generators etc
        # They can run on Linux workers.
        "large": {
            "OSFamily": "Linux",
            "container-image": container_image,
            # As of Jul 2023, the action_large pool uses n2-highmem-8 with 200GB of pd-ssd.
            # The pool is intended for the following actions.
            #  - slow actions that can benefit from multi-cores and/or faster disk I/O. e.g. link, mojo, generate bindings etc.
            #  - actions that fail for OOM.
            "label:action_large": "1",
        },
    }

def __configs(ctx):
    if "reapi_address" in ctx.flags:
        if ctx.flags["reapi_address"] != "remotebuildexecution.googleapis.com:443":
            return []
    if __is_cros_rbe(ctx):
        return []
    return [
        "googlechrome",
    ]

backend = module(
    "backend",
    platform_properties = __platform_properties,
    configs = __configs,
)
