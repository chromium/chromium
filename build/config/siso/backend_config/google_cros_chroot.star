# -*- bazel-starlark -*-
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso backend config for Google ChromeOS chroot ebuild."""

load("@builtin//struct.star", "module")

def __platform_properties(ctx):
    # LINT.IfChange
    container_image = "docker://gcr.io/chops-public-images-prod/rbe/siso-chromium/linux@sha256:ef35d347f4a4a2d32b76fd908e66e96f59bf8ba7379fd5626548244c45343b2b"

    return {
        "default": {
            "OSFamily": "Linux",
            "container-image": container_image,
            "dockerChrootPath": ".",
            "dockerRuntime": "runsc",
        },
        # Large workers are usually used for Python actions like generate bindings, mojo generators etc
        # They can run on Linux workers.
        "large": {
            "OSFamily": "Linux",
            "container-image": container_image,
            "dockerChrootPath": ".",
            "dockerRuntime": "runsc",
        },
    }
    # LINT.ThenChange(/buildtools/reclient_cfgs/linux_chroot/chromium-browser-clang/rewrapper_linux.cfg)

backend = module(
    "backend",
    platform_properties = __platform_properties,
)
