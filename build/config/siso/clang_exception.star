# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""workaround for exceptional clang actions. e.g. exit=137 (OOM?),
 or DeadlineExceeded.
"""

load("@builtin//lib/gn.star", "gn")
load("@builtin//runtime.star", "runtime")
load("@builtin//struct.star", "module")

def __step_config(ctx, step_config, use_windows_worker = None):
    cxx_exceptions = [
        {
            # TODO: crbug.com/380755128 - Make each compile unit smaller.
            "name": "fuzzer_large_compile",
            "action_outs": [
                # keep-sorted start
                "./obj/chrome/test/fuzzing/htmlfuzzer_proto_gen/htmlfuzzer_sub.pb.o",
                "./obj/chrome/test/fuzzing/jsfuzzer/jsfuzzer.o",
                "./obj/chrome/test/fuzzing/jsfuzzer_proto_gen/jsfuzzer.pb.o",
                "./obj/chrome/test/fuzzing/jsfuzzer_proto_gen/jsfuzzer_sub.pb.o",
                "./obj/chrome/test/fuzzing/renderer_fuzzing/renderer_in_process_mojolpm_fuzzer/renderer_in_process_mojolpm_fuzzer.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidl_fuzzer_grammar/webidl_fuzzer_grammar.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidl_fuzzer_grammar_proto_gen/webidl_fuzzer_grammar.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidl_in_process_fuzzer/webidl_in_process_fuzzer.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer/webidlfuzzer.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer/webidlfuzzer_sub0.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer/webidlfuzzer_sub9.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer_sub0.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer_sub1.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer_sub10.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer_sub11.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer_sub2.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer_sub3.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer_sub4.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer_sub5.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer_sub6.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer_sub7.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer_sub8.pb.o",
                "./obj/chrome/test/fuzzing/webidl_fuzzing/webidlfuzzer_proto_gen/webidlfuzzer_sub9.pb.o",
                # keep-sorted end
            ],
            "timeout": "15m",
            # need 9G for debug build
            "use_large": True,
        },
        {
            # TODO: crbug.com/413423339 - improve compile speed
            "name": "slow_compile",
            "action_outs": [
                # keep-sorted start
                "./obj/content/browser/browser/browser_interface_binders.o",
                "./obj/content/test/content_browsertests/interest_group_browsertest.o",
                "./obj/content/test/content_browsertests/navigation_controller_impl_browsertest.o",
                "./obj/content/test/content_browsertests/prerender_browsertest.o",
                "./obj/content/test/content_browsertests/site_per_process_browsertest.o",
                "./obj/content/test/content_unittests/auction_runner_unittest.o",
                "./obj/third_party/abseil-cpp/absl/functional/any_invocable_test/any_invocable_test.o",
                "./obj/third_party/highway/highway_tests/cast_test.o",
                "./obj/third_party/highway/highway_tests/convert_test.o",
                "./obj/third_party/highway/highway_tests/demote_test.o",
                "./obj/third_party/highway/highway_tests/div_test.o",
                "./obj/third_party/highway/highway_tests/if_test.o",
                "./obj/third_party/highway/highway_tests/interleaved_test.o",
                "./obj/third_party/highway/highway_tests/logical_test.o",
                "./obj/third_party/highway/highway_tests/mask_mem_test.o",
                "./obj/third_party/highway/highway_tests/mask_test.o",
                "./obj/third_party/highway/highway_tests/masked_arithmetic_test.o",
                "./obj/third_party/highway/highway_tests/memory_test.o",
                "./obj/third_party/highway/highway_tests/rotate_test.o",
                "./obj/third_party/highway/highway_tests/shift_test.o",
                "./obj/third_party/highway/highway_tests/shuffle4_test.o",
                "./obj/third_party/highway/highway_tests/widen_mul_test.o",
                # keep-sorted end
            ],
            "timeout": "4m",
        },
        {
            "name": "slow_scandeps",
            "action_outs": [
                # keep-sorted start
                "./obj/chrome/browser/prefs/impl/browser_prefs.o",
                # keep-sorted end
            ],
            "timeout": "4m",
        },
    ]

    # Check if the build target is Windows.
    is_target_windows = runtime.os == "windows" or ("args.gn" in ctx.metadata and gn.args(ctx).get("target_os") == '"win"')

    cc_exceptions = []
    new_rules = []
    for rule in step_config["rules"]:
        if rule["name"].endswith("/cxx") or rule["name"].endswith("/cc"):
            if "action_outs" in rule:
                fail("unexpeced \"action_outs\" in cxx rule %s" % rule["name"])
            if rule["name"].endswith("/cxx"):
                exceptions = cxx_exceptions
            elif rule["name"].endswith("/cc"):
                exceptions = cc_exceptions
            for ex in exceptions:
                r = {}
                r.update(rule)
                r["name"] += "/exception/" + ex["name"]
                outs = ex["action_outs"]
                if is_target_windows:
                    outs = [obj.removesuffix(".o") + ".obj" for obj in outs if obj.startswith("./obj/")]
                r["action_outs"] = outs
                if "timeout" in ex:
                    r["timeout"] = ex["timeout"]
                if "use_large" in ex and ex["use_large"]:
                    if use_windows_worker:
                        # Do not run large compiles on default Windows worker.
                        r["remote"] = False
                    else:
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

clang_exception = module(
    "clang_exception",
    step_config = __step_config,
)
