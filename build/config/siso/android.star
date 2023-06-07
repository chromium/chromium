# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for Android builds."""

load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")
load("./config.star", "config")

def __enabled(ctx):
    if "args.gn" in ctx.metadata:
        gn_args = gn.parse_args(ctx.metadata["args.gn"])
        if gn_args.get("target_os") == '"android"':
            return True
    return False

def __step_config(ctx, step_config):
    __input_deps(ctx, step_config["input_deps"])

    remote_run = config.get(ctx, "remote_android") or config.get(ctx, "remote_all")
    step_config["rules"].extend([
        # See also https://chromium.googlesource.com/chromium/src/build/+/HEAD/android/docs/java_toolchain.md
        {
            "name": "android/write_build_config",
            "command_prefix": "python3 ../../build/android/gyp/write_build_config.py",
            "handler": "android_write_build_config",
            # TODO(crbug.com/1452038): include only required build_config.json files in GN config.
            "indirect_inputs": {
                "includes": ["*.build_config.json"],
            },
            "remote": remote_run,
            "canonicalize_dir": True,
        },
        {
            "name": "android/ijar",
            "command_prefix": "python3 ../../build/android/gyp/ijar.py",
            "remote": remote_run,
            "canonicalize_dir": True,
        },
        {
            "name": "android/turbine",
            "command_prefix": "python3 ../../build/android/gyp/turbine.py",
            "handler": "android_turbine",
            # TODO(crrev.com/c/4596899): Add Java inputs in GN config.
            "inputs": [
                "third_party/jdk/current/bin/java",
                "third_party/android_sdk/public/platforms/android-33/android.jar",
                "third_party/android_sdk/public/platforms/android-33/optional/org.apache.http.legacy.jar",
            ],
            # TODO(crbug.com/1452038): include only required jar files in GN config.
            "indirect_inputs": {
                "includes": ["*.jar"],
            },
            "remote": remote_run,
            "canonicalize_dir": True,
        },
        {
            "name": "android/compile_java",
            "command_prefix": "python3 ../../build/android/gyp/compile_java.py",
            "handler": "android_compile_java",
            # TODO(crrev.com/c/4596899): Add Java inputs in GN config.
            "inputs": [
                "third_party/jdk/current/bin/javac",
                "third_party/android_sdk/public/platforms/android-33/optional/org.apache.http.legacy.jar",
            ],
            # TODO(crbug.com/1452038): include only required java, jar files in GN config.
            "indirect_inputs": {
                "includes": ["*.java", "*.ijar.jar", "*.turbine.jar"],
            },
            # Don't include files under --generated-dir.
            # This is probably optimization for local incrmental builds.
            # However, this is harmful for remote build cache hits.
            "ignore_extra_input_pattern": ".*srcjars.*\\.java",
            "ignore_extra_output_pattern": ".*srcjars.*\\.java",
            "remote": remote_run,
            "canonicalize_dir": True,
        },
        {
            "name": "android/dex",
            "command_prefix": "python3 ../../build/android/gyp/dex.py",
            "handler": "android_dex",
            # TODO(crrev.com/c/4596899): Add Java inputs in GN config.
            "inputs": [
                "third_party/jdk/current/bin/java",
                "third_party/android_sdk/public/platforms/android-33/android.jar",
                "third_party/android_sdk/public/platforms/android-33/optional/org.apache.http.legacy.jar",
            ],
            # TODO(crbug.com/1452038): include only required jar,dex files in GN config.
            "indirect_inputs": {
                "includes": ["*.dex", "*.ijar.jar", "*.turbine.jar"],
            },
            # *.dex files are intermediate files used in incremental builds.
            # Fo remote actions, let's ignore them, assuming remote cache hits compensate.
            "ignore_extra_input_pattern": ".*\\.dex",
            "ignore_extra_output_pattern": ".*\\.dex",
            "remote": remote_run,
            "canonicalize_dir": True,
        },
        {
            "name": "android/filter_zip",
            "command_prefix": "python3 ../../build/android/gyp/filter_zip.py",
            "remote": remote_run,
            "canonicalize_dir": True,
        },
    ])
    return step_config

def __android_compile_java_handler(ctx, cmd):
    out = cmd.outputs[0]
    outputs = [
        out + ".md5.stamp",
    ]
    ctx.actions.fix(outputs = cmd.outputs + outputs)

def __android_dex_handler(ctx, cmd):
    out = cmd.outputs[0]
    inputs = [
        out.replace("obj/", "gen/").replace(".dex.jar", ".build_config.json"),
    ]

    # Add __dex.desugardeps to the outputs.
    outputs = [
        out + ".md5.stamp",
    ]
    for i, arg in enumerate(cmd.args):
        if arg == "--desugar-dependencies":
            outputs.append(ctx.fs.canonpath(cmd.args[i + 1]))

    # TODO: dex.py takes --incremental-dir to reuse the .dex produced in a previous build.
    # Should remote dex action also take this?
    ctx.actions.fix(
        inputs = cmd.inputs + inputs,
        outputs = cmd.outputs + outputs,
    )

def __android_turbine_handler(ctx, cmd):
    inputs = []
    outputs = []
    out_fileslist = False
    if cmd.args[len(cmd.args) - 1].startswith("@"):
        out_fileslist = True
    for i, arg in enumerate(cmd.args):
        if arg.startswith("--jar-path="):
            jar_path = ctx.fs.canonpath(arg.removeprefix("--jar-path="))
            if out_fileslist:
                outputs.append(jar_path + ".java_files_list.txt")
    ctx.actions.fix(
        inputs = cmd.inputs + inputs,
        outputs = cmd.outputs + outputs,
    )

def __android_write_build_config_handler(ctx, cmd):
    inputs = []
    for i, arg in enumerate(cmd.args):
        if arg == "--shared-libraries-runtime-deps":
            inputs.append(ctx.fs.canonpath(cmd.args[i + 1]))
    ctx.actions.fix(inputs = cmd.inputs + inputs)

__handlers = {
    "android_compile_java": __android_compile_java_handler,
    "android_dex": __android_dex_handler,
    "android_turbine": __android_turbine_handler,
    "android_write_build_config": __android_write_build_config_handler,
}

def __input_deps(ctx, input_deps):
    # TODO(crrev.com/c/4596899): Add Java inputs in GN config.
    input_deps["third_party/jdk/current:current"] = [
        "third_party/jdk/current/bin/java",
        "third_party/jdk/current/conf/logging.properties",
        "third_party/jdk/current/conf/security/java.security",
        "third_party/jdk/current/lib/ct.sym",
        "third_party/jdk/current/lib/jli/libjli.so",
        "third_party/jdk/current/lib/jrt-fs.jar",
        "third_party/jdk/current/lib/jvm.cfg",
        "third_party/jdk/current/lib/libawt.so",
        "third_party/jdk/current/lib/libawt_headless.so",
        "third_party/jdk/current/lib/libawt_xawt.so",
        "third_party/jdk/current/lib/libjava.so",
        "third_party/jdk/current/lib/libjimage.so",
        "third_party/jdk/current/lib/libjli.so",
        "third_party/jdk/current/lib/libjsvml.so",
        "third_party/jdk/current/lib/libmanagement.so",
        "third_party/jdk/current/lib/libmanagement_ext.so",
        "third_party/jdk/current/lib/libnet.so",
        "third_party/jdk/current/lib/libnio.so",
        "third_party/jdk/current/lib/libverify.so",
        "third_party/jdk/current/lib/libzip.so",
        "third_party/jdk/current/lib/modules",
        "third_party/jdk/current/lib/server/classes.jsa",
        "third_party/jdk/current/lib/server/libjvm.so",
        "third_party/jdk/current/lib/tzdb.dat",
    ]
    input_deps["third_party/jdk/current/bin/java"] = [
        "third_party/jdk/current:current",
    ]
    input_deps["third_party/jdk/current/bin/javac"] = [
        "third_party/jdk/current:current",
    ]

android = module(
    "android",
    enabled = __enabled,
    step_config = __step_config,
    filegroups = {},
    handlers = __handlers,
    input_deps = __input_deps,
)
