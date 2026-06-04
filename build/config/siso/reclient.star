# -*- bazel-starlark -*-
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for using remote exec wrapper when use_reclient=true."""

load("@builtin//struct.star", "module")
load("./gn_logs.star", "gn_logs")

def __filegroups(ctx):
    return {}

__handlers = {}

def __use_reclient(ctx):
    return gn_logs.read(ctx).get("use_reclient") == "true"

def __step_config(ctx, step_config):
    new_rules = []
    for rule in step_config["rules"]:
        if (rule["name"].startswith("clang/cxx") or rule["name"].startswith("clang/cc") or
            rule["name"].startswith("clang-cl/cxx") or rule["name"].startswith("clang-cl/cc") or
            rule["name"].startswith("clang/objc") or rule["name"].startswith("clang-coverage/")):
            # Enable remote exec wrapper.
            rule["use_remote_exec_wrapper"] = True
            rule["remote"] = False
            rule.pop("handler", None)

            # Remove command_prefix to match wrapped commands.
            rule.pop("command_prefix", None)

            new_rules.append(rule)
            continue

        # Disable native remote execution for other rules when using reclient.
        if rule.get("remote"):
            rule["remote"] = False
        new_rules.append(rule)

    step_config["rules"] = new_rules
    return step_config

reclient = module(
    "reclient",
    enabled = __use_reclient,
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
