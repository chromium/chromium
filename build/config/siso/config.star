# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Config module for checking siso -config flags."""

load("@builtin//struct.star", "module")

__KNOWN_CONFIG_OPTIONS = [
    # Indicates that the build runs on a builder.
    "builder",

    # Indicate that it runs on Cog (automatically set on Cog).
    "cog",

    # Force disable additional remote on cog.
    # TODO: b/333033551 - check performance with/without remote on cog.
    "disable-remote-on-cog",

    # TODO: b/308405411 - Enable this config for all builders.
    "remote-devtools-frontend-typescript",

    # TODO: b/370860664 - Enable remote link by default after supporting
    # all platforms and target OSes.
    # For developers, we can't simply enable remote link without bytes
    # because developers need objects and tests locally for debugging
    # and testing.
    "remote-link",
]

def __check(ctx):
    if "config" in ctx.flags:
        for cfg in ctx.flags["config"].split(","):
            if cfg not in __KNOWN_CONFIG_OPTIONS:
                print("unknown config: %s" % cfg)

def __get(ctx, key):
    onCog = ctx.fs.exists("../.citc")
    disableRemoteOnCog = False
    if "config" in ctx.flags:
        for cfg in ctx.flags["config"].split(","):
            if cfg == key:
                return True
            if cfg == "disable-remote-on-cog":
                disableRemoteOnCog = True
            if cfg == "cog":
                onCog = True
    if onCog:
        if disableRemoteOnCog:
            return False

        # on cog, .citc directory exist in parent directory of exec root.
        # disable race strategy as "builder".
        # enable "remote-*" on cog
        # TODO: b/308405411 - enable "remote-devtools-frontend-typescript"
        if key in ("builder", "cog", "remote-link"):
            return True
    return False

config = module(
    "config",
    check = __check,
    get = __get,
)
