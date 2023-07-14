# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for rewriting remote config (non-rewrapper) into reproxy config."""

load("@builtin//struct.star", "module")

def __step_config(ctx, step_config):
    default_platform = step_config.get("platforms", {}).get("default")
    if not default_platform:
        fail("Need a default platform for converting remote config into reproxy config")

    for rule in step_config["rules"]:
        if not rule.get("remote"):
            continue

        platform = default_platform
        platform_ref = rule.get("platform_ref")
        if platform_ref:
            platform = step_config["platforms"].get(platform_ref)
            if not platform:
                fail("Rule %s uses undefined platform '%s'" % (rule["name"], platform_ref))

        rule["reproxy_config"] = {
            "platform": platform,
            "labels": {
                # TODO: don't hardcode this
                "type": "tool",
            },
            "canonicalize_working_dir": rule.get("canonicalize_dir", False),
            "exec_strategy": "remote",
            "exec_timeout": rule.get("timeout", "10m"),
            "download_outputs": True,
        }

    return step_config

reproxy_from_remote = module(
    "reproxy_from_remote",
    step_config = __step_config,
)
