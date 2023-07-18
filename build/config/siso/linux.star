# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for linux."""

load("@builtin//struct.star", "module")
load("./clang_linux.star", "clang")
load("./config.star", "config")
load("./mojo.star", "mojo")
load("./nacl_linux.star", "nacl")
load("./nasm_linux.star", "nasm")
load("./proto_linux.star", "proto")
load("./remote_exec_wrapper.star", "remote_exec_wrapper")
load("./reproxy_from_rewrapper.star", "reproxy_from_rewrapper")
load("./reproxy_from_remote.star", "reproxy_from_remote")
load("./android.star", "android")

__filegroups = {}
__filegroups.update(android.filegroups)
__filegroups.update(clang.filegroups)
__filegroups.update(mojo.filegroups)
__filegroups.update(nacl.filegroups)
__filegroups.update(nasm.filegroups)
__filegroups.update(proto.filegroups)

__handlers = {}
__handlers.update(android.handlers)
__handlers.update(clang.handlers)
__handlers.update(mojo.handlers)
__handlers.update(nacl.handlers)
__handlers.update(nasm.handlers)
__handlers.update(proto.handlers)
__handlers.update(remote_exec_wrapper.handlers)
__handlers.update(reproxy_from_rewrapper.handlers)

def __disable_remote_b281663988(step_config):
    step_config["rules"].extend([
        {
            # TODO(b/281663988): missing headers.
            "name": "b281663988/missing-headers",
            "action_outs": [
                "./obj/ui/qt/qt5_shim/qt_shim.o",
                "./obj/ui/qt/qt6_shim/qt_shim.o",
                "./obj/ui/qt/qt5_shim/qt5_shim_moc.o",
                "./obj/ui/qt/qt6_shim/qt6_shim_moc.o",
                "./obj/ui/qt/qt_interface/qt_interface.o",
            ],
            "remote": False,
        },
    ])
    return step_config

def __disable_remote_b289968566(step_config):
    step_config["rules"].extend([
        {
            # TODO(b/289968566): they often faile with exit=137 (OOM?).
            # We should migrate default machine type to n2-standard-2.
            "name": "b289968566/exit-137",
            "action_outs": [
                "./android_clang_arm/obj/third_party/distributed_point_functions/distributed_point_functions/evaluate_prg_hwy.o",
                "./clang_x64_v8_arm64/obj/v8/torque_generated_initializers/js-to-wasm-tq-csa.o",
                "./clang_x86_v8_arm/obj/v8/torque_generated_initializers/js-to-wasm-tq-csa.o",
                "./obj/chrome/browser/ash/ash/autotest_private_api.o",
                "./obj/chrome/browser/ash/ash/chrome_browser_main_parts_ash.o",
                "./obj/chrome/browser/browser/browser_prefs.o",
                "./obj/chrome/browser/browser/chrome_browser_interface_binders.o",
                "./obj/chrome/browser/ui/ash/holding_space/browser_tests/holding_space_ui_browsertest.o",
                "./obj/chrome/test/browser_tests/browser_non_client_frame_view_chromeos_browsertest.o",
                "./obj/chrome/test/browser_tests/chrome_shelf_controller_browsertest.o",
                "./obj/chrome/test/browser_tests/device_local_account_browsertest.o",
                "./obj/chrome/test/browser_tests/file_manager_browsertest_base.o",
                "./obj/chrome/test/browser_tests/remote_apps_manager_browsertest.o",
                "./obj/v8/torque_generated_initializers/js-to-wasm-tq-csa.o",
            ],
            "remote": False,
        },
    ])
    return step_config

def __step_config(ctx, step_config):
    config.check(ctx)
    step_config["platforms"] = {
        "default": {
            "OSFamily": "Linux",
            "container-image": "docker://gcr.io/chops-public-images-prod/rbe/siso-chromium/linux@sha256:912808c295e578ccde53b0685bcd0d56c15d7a03e819dcce70694bfe3fdab35e",
            "label:action_default": "1",
        },
        "mojo": {
            "OSFamily": "Linux",
            "container-image": "docker://gcr.io/chops-public-images-prod/rbe/siso-chromium/linux@sha256:912808c295e578ccde53b0685bcd0d56c15d7a03e819dcce70694bfe3fdab35e",
            # action_mojo pool uses n2-highmem-8 machine as of 2023 Jun and
            # mojo_bindings_generators.py will run faster on n2-highmem-8 than
            # n2-custom-2-3840
            # e.g.
            #  n2-highmem-8: exec: 880.202978ms
            #  n2-custom-2-3840: exec: 2.42808488s
            "label:action_mojo": "1",
        },
    }

    step_config = __disable_remote_b289968566(step_config)

    # reproxy_from_rewrapper takes precedence over remote exec wrapper handler if enabled.
    if reproxy_from_rewrapper.enabled(ctx):
        step_config = __disable_remote_b281663988(step_config)
        step_config = reproxy_from_rewrapper.step_config(ctx, step_config)
        if config.get(ctx, "remote_to_reproxy"):
            # Exclude mojo and clang. Already covered by reproxy_from_rewrapper.
            # (Duplicate rules are not allowed, so it wouldn't work anyway.)
            if android.enabled(ctx):
                step_config = android.step_config(ctx, step_config)
            step_config = nacl.step_config(ctx, step_config)
            step_config = nasm.step_config(ctx, step_config)
            step_config = proto.step_config(ctx, step_config)
            step_config = reproxy_from_remote.step_config(ctx, step_config)
    elif remote_exec_wrapper.enabled(ctx):
        step_config = __disable_remote_b281663988(step_config)
        step_config = remote_exec_wrapper.step_config(ctx, step_config)
    else:
        if android.enabled(ctx):
            step_config = android.step_config(ctx, step_config)
        step_config = clang.step_config(ctx, step_config)
        step_config = mojo.step_config(ctx, step_config)
        step_config = nacl.step_config(ctx, step_config)
        step_config = nasm.step_config(ctx, step_config)
        step_config = proto.step_config(ctx, step_config)

    return step_config

chromium = module(
    "chromium",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
