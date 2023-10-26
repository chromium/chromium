# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for parsing rewrapper cfg file."""

load("@builtin//struct.star", "module")
load("./config.star", "config")

def __parse(ctx, cfg_file):
    if not cfg_file:
        fail("cfg file expected but none found")
    if not ctx.fs.exists(cfg_file):
        fail("looked for rewrapper cfg %s but not found, is download_remoteexec_cfg set in gclient custom_vars?" % cfg_file)

    reproxy_config = {}
    for line in str(ctx.fs.read(cfg_file)).splitlines():
        if line.startswith("canonicalize_working_dir="):
            reproxy_config["canonicalize_working_dir"] = line.removeprefix("canonicalize_working_dir=").lower() == "true"

        reproxy_config["download_outputs"] = True
        if line.startswith("download_outputs="):
            reproxy_config["download_outputs"] = line.removeprefix("download_outputs=").lower() == "true"

        if line.startswith("exec_strategy="):
            exec_strategy = line.removeprefix("exec_strategy=")

            # Disable racing on builders since bots don't have many CPU cores.
            if exec_strategy == "racing" and config.get(ctx, "builder"):
                exec_strategy = "remote_local_fallback"
            reproxy_config["exec_strategy"] = exec_strategy

        if line.startswith("exec_timeout="):
            reproxy_config["exec_timeout"] = line.removeprefix("exec_timeout=")

        if line.startswith("reclient_timeout="):
            reproxy_config["reclient_timeout"] = line.removeprefix("reclient_timeout=")

        if line.startswith("inputs="):
            reproxy_config["inputs"] = line.removeprefix("inputs=").split(",")

        if line.startswith("labels="):
            if "labels" not in reproxy_config:
                reproxy_config["labels"] = dict()
            for label in line.removeprefix("labels=").split(","):
                label_parts = label.split("=")
                if len(label_parts) != 2:
                    fail("not k,v %s" % label)
                reproxy_config["labels"][label_parts[0]] = label_parts[1]

        if line.startswith("platform="):
            if "platform" not in reproxy_config:
                reproxy_config["platform"] = dict()
            for label in line.removeprefix("platform=").split(","):
                label_parts = label.split("=")
                if len(label_parts) != 2:
                    fail("not k,v %s" % label)
                reproxy_config["platform"][label_parts[0]] = label_parts[1]

        if line.startswith("remote_wrapper="):
            reproxy_config["remote_wrapper"] = line.removeprefix("remote_wrapper=")

        if line.startswith("server_address="):
            reproxy_config["server_address"] = line.removeprefix("server_address=")
    return reproxy_config

rewrapper_cfg = module(
    "rewrapper_cfg",
    parse = __parse,
)
