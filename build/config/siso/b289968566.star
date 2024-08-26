# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""workaround for b/289968566. they often faile with exit=137 (OOM?)."""

load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")

def __step_config(ctx, step_config):
    # TODO(b/289968566): they often faile with exit=137 (OOM?).
    # They need to run on a machine has more memory than the default machine type n2-custom-2-3840
    exit137_list = [
        # Fallback happens with the following args.gn
        # (linux-build-perf-developer, win-build-perf-developer)
        # is_component_build = true
        # is_debug = true
        # symbol_level = 2
        # use_siso = true
        "./obj/chrome/test/fuzzing/jsfuzzer_proto_gen/jsfuzzer.pb.o",
    ]
    if runtime.os == "windows":
        exit137_list = [obj.removesuffix(".o") + ".obj" for obj in exit137_list if obj.startswith("./obj/")]

    new_rules = []
    for rule in step_config["rules"]:
        if not rule["name"].endswith("/cxx"):
            new_rules.append(rule)
            continue
        if "action_outs" in rule:
            fail("unexpeced \"action_outs\" in cxx rule %s" % rule["name"])
        r = {}
        r.update(rule)
        r["name"] += "/b289968566/exit-137"
        r["action_outs"] = exit137_list

        # Some large compile take longer than the default timeout 2m.
        # TODO(crbug.com/362140878) obj/chrome/test/fuzzing/jsfuzzer_proto_gen/jsfuzzer.pb.o: too slow compile (with coverage)
        r["timeout"] = "15m"

        # use `_large` variant of platform if it doesn't use default platform,
        # i.e. mac/win case.
        if "platform_ref" in r:
            r["platform_ref"] = r["platform_ref"] + "_large"
        else:
            r["platform_ref"] = "large"
        if r.get("handler") == "rewrite_rewrapper":
            r["handler"] = "rewrite_rewrapper_large"
        new_rules.append(r)
        new_rules.append(rule)
    step_config["rules"] = new_rules
    return step_config

b289968566 = module(
    "b289968566",
    step_config = __step_config,
)
