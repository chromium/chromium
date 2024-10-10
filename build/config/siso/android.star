# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for Android builds."""

load("@builtin//encoding.star", "json")
load("@builtin//lib/gn.star", "gn")
load("@builtin//struct.star", "module")
load("./config.star", "config")

def __enabled(ctx):
    if "args.gn" in ctx.metadata:
        gn_args = gn.args(ctx)
        if gn_args.get("target_os") == '"android"':
            return True
    return False

def __filegroups(ctx):
    return {}

def __step_config(ctx, step_config):
    remote_run = True  # Turn this to False when you do file access trace.
    step_config["rules"].extend([
        # See also https://chromium.googlesource.com/chromium/src/build/+/HEAD/android/docs/java_toolchain.md
        {
            "name": "android/write_build_config",
            "command_prefix": "python3 ../../build/android/gyp/write_build_config.py",
            "handler": "android_write_build_config",
            "remote": remote_run,
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "android/ijar",
            "command_prefix": "python3 ../../build/android/gyp/ijar.py",
            "remote": remote_run,
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "android/turbine",
            "command_prefix": "python3 ../../build/android/gyp/turbine.py",
            "handler": "android_turbine",
            "remote": remote_run,
            "platform_ref": "large",
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "android/compile_resources",
            "command_prefix": "python3 ../../build/android/gyp/compile_resources.py",
            "handler": "android_compile_resources",
            "exclude_input_patterns": [
                "*.a",
                "*.cc",
                "*.h",
                "*.inc",
                "*.info",
                "*.o",
                "*.pak",
                "*.sql",
            ],
            "remote": remote_run,
            "canonicalize_dir": True,
            "timeout": "5m",
        },
        {
            "name": "android/compile_java",
            "command_prefix": "python3 ../../build/android/gyp/compile_java.py",
            "handler": "android_compile_java",
            # Don't include files under --generated-dir.
            # This is probably optimization for local incrmental builds.
            # However, this is harmful for remote build cache hits.
            "ignore_extra_input_pattern": ".*srcjars.*\\.java",
            "ignore_extra_output_pattern": ".*srcjars.*\\.java",
            "remote": remote_run,
            "platform_ref": "large",
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "android/dex",
            "command_prefix": "python3 ../../build/android/gyp/dex.py",
            "handler": "android_dex",
            # TODO(crbug.com/40270798): include only required jar, dex files in GN config.
            "indirect_inputs": {
                "includes": ["*.dex", "*.ijar.jar", "*.turbine.jar"],
            },
            "exclude_input_patterns": [
                "*.a",
                "*.cc",
                "*.h",
                "*.inc",
                "*.info",
                "*.o",
                "*.pak",
                "*.sql",
            ],
            # *.dex files are intermediate files used in incremental builds.
            # Fo remote actions, let's ignore them, assuming remote cache hits compensate.
            "ignore_extra_input_pattern": ".*\\.dex",
            "ignore_extra_output_pattern": ".*\\.dex",
            "remote": remote_run,
            "platform_ref": "large",
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "android/filter_zip",
            "command_prefix": "python3 ../../build/android/gyp/filter_zip.py",
            "remote": remote_run,
            "canonicalize_dir": True,
            "timeout": "2m",
        },
        {
            "name": "android/generate_resource_allowlist",
            "command_prefix": "python3 ../../tools/resources/generate_resource_allowlist.py",
            "indirect_inputs": {
                "includes": ["*.o", "*.a"],
            },
            # When remote linking without bytes enabled, .o, .a files don't
            # exist on the local file system.
            # This step also should run remortely to avoid downloading them.
            "remote": config.get(ctx, "remote-link"),
            "platform_ref": "large",
            "canonicalize_dir": True,
            "timeout": "2m",
        },
    ])
    return step_config

def __filearg(ctx, arg):
    fn = ""
    if arg.startswith("@FileArg("):
        f = arg.removeprefix("@FileArg(").removesuffix(")").split(":")
        fn = f[0].removesuffix("[]")  # [] suffix controls expand list?
        v = json.decode(str(ctx.fs.read(ctx.fs.canonpath(fn))))
        for k in f[1:]:
            v = v[k]
        arg = v
    if type(arg) == "string":
        if arg.startswith("["):
            return fn, json.decode(arg)
        return fn, [arg]
    return fn, arg

def __android_compile_resources_handler(ctx, cmd):
    # Script:
    #   https://crsrc.org/c/build/android/gyp/compile_resources.py
    # GN Config:
    #   https://crsrc.org/c/build/config/android/internal_rules.gni;l=2163;drc=1b15af251f8a255e44f2e3e3e7990e67e87dcc3b
    #   https://crsrc.org/c/build/config/android/system_image.gni;l=58;drc=39debde76e509774287a655285d8556a9b8dc634
    # Sample args:
    #   --aapt2-path ../../third_party/android_build_tools/aapt2/cipd/aapt2
    #   --android-manifest gen/chrome/android/trichrome_library_system_stub_apk__manifest.xml
    #   --arsc-package-name=org.chromium.trichromelibrary
    #   --arsc-path obj/chrome/android/trichrome_library_system_stub_apk.ap_
    #   --debuggable
    #   --dependencies-res-zip-overlays=@FileArg\(gen/chrome/android/webapk/shell_apk/maps_go_webapk.build_config.json:deps_info:dependency_zip_overlays\)
    #   --dependencies-res-zips=@FileArg\(gen/chrome/android/webapk/shell_apk/maps_go_webapk.build_config.json:deps_info:dependency_zips\)
    #   --depfile gen/chrome/android/webapk/shell_apk/maps_go_webapk__compile_resources.d
    #   --emit-ids-out=gen/chrome/android/webapk/shell_apk/maps_go_webapk__compile_resources.resource_ids
    #   --extra-res-packages=@FileArg\(gen/chrome/android/webapk/shell_apk/maps_go_webapk.build_config.json:deps_info:extra_package_names\)
    #   --include-resources(=)../../third_party/android_sdk/public/platforms/android-34/android.jar
    #   --info-path obj/chrome/android/webapk/shell_apk/maps_go_webapk.ap_.info
    #   --min-sdk-version=24
    #   --proguard-file obj/chrome/android/webapk/shell_apk/maps_go_webapk/maps_go_webapk.resources.proguard.txt
    #   --r-text-out gen/chrome/android/webapk/shell_apk/maps_go_webapk__compile_resources_R.txt
    #   --rename-manifest-package=org.chromium.trichromelibrary
    #   --srcjar-out gen/chrome/android/webapk/shell_apk/maps_go_webapk__compile_resources.srcjar
    #   --target-sdk-version=33
    #   --version-code 1
    #   --version-name Developer\ Build
    #   --webp-cache-dir=obj/android-webp-cache
    inputs = []
    for i, arg in enumerate(cmd.args):
        for k in ["--dependencies-res-zips=", "--dependencies-res-zip-overlays=", "--extra-res-packages="]:
            if arg.startswith(k):
                arg = arg.removeprefix(k)
                _, v = __filearg(ctx, arg)
                for f in v:
                    f = ctx.fs.canonpath(f)
                    inputs.append(f)
                    if k == "--dependencies-res-zips=" and ctx.fs.exists(f + ".info"):
                        inputs.append(f + ".info")

    ctx.actions.fix(
        inputs = cmd.inputs + inputs,
    )

def __android_compile_java_handler(ctx, cmd):
    # Script:
    #   https://crsrc.org/c/build/android/gyp/compile_java.py
    # GN Config:
    #   https://crsrc.org/c/build/config/android/internal_rules.gni;l=2995;drc=775b3a9ebccd468c79592dad43ef46632d3a411f
    # Sample args:
    #   --depfile=gen/chrome/android/chrome_test_java__compile_java.d
    #   --generated-dir=gen/chrome/android/chrome_test_java/generated_java
    #   --jar-path=obj/chrome/android/chrome_test_java.javac.jar
    #   --java-srcjars=\[\"gen/chrome/browser/tos_dialog_behavior_generated_enum.srcjar\",\ \"gen/chrome/android/chrome_test_java__assetres.srcjar\",\ \"gen/chrome/android/chrome_test_java.generated.srcjar\"\]
    #   --target-name //chrome/android:chrome_test_java__compile_java
    #   --classpath=@FileArg\(gen/chrome/android/chrome_test_java.build_config.json:android:sdk_interface_jars\)
    #   --header-jar obj/chrome/android/chrome_test_java.turbine.jar
    #   --classpath=\[\"obj/chrome/android/chrome_test_java.turbine.jar\"\]
    #   --classpath=@FileArg\(gen/chrome/android/chrome_test_java.build_config.json:deps_info:javac_full_interface_classpath\)
    #   --kotlin-jar-path=obj/chrome/browser/tabmodel/internal/java.kotlinc.jar
    #   --chromium-code=1
    #   --warnings-as-errors
    #   --jar-info-exclude-globs=\[\"\*/R.class\",\ \"\*/R\\\$\*.class\",\ \"\*/Manifest.class\",\ \"\*/Manifest\\\$\*.class\",\ \"\*/\*GEN_JNI.class\"\]
    #   --enable-errorprone
    #   @gen/chrome/android/chrome_test_java.sources

    out = cmd.outputs[0]
    outputs = [
        out + ".md5.stamp",
    ]

    inputs = []
    for i, arg in enumerate(cmd.args):
        for k in ["--classpath=", "--bootclasspath=", "--processorpath="]:
            if arg.startswith(k):
                arg = arg.removeprefix(k)
                fn, v = __filearg(ctx, arg)
                if fn:
                    inputs.append(ctx.fs.canonpath(fn))
                for f in v:
                    f, _, _ = f.partition(":")
                    inputs.append(ctx.fs.canonpath(f))

    ctx.actions.fix(
        inputs = cmd.inputs + inputs,
        outputs = cmd.outputs + outputs,
    )

def __android_dex_handler(ctx, cmd):
    out = cmd.outputs[0]
    inputs = []

    # Add __dex.desugardeps to the outputs.
    outputs = [
        out + ".md5.stamp",
    ]
    for i, arg in enumerate(cmd.args):
        if arg == "--desugar-dependencies":
            outputs.append(ctx.fs.canonpath(cmd.args[i + 1]))
        for k in ["--class-inputs=", "--bootclasspath=", "--classpath=", "--class-inputs-filearg=", "--dex-inputs-filearg="]:
            if arg.startswith(k):
                arg = arg.removeprefix(k)
                _, v = __filearg(ctx, arg)
                for f in v:
                    f, _, _ = f.partition(":")
                    f = ctx.fs.canonpath(f)
                    inputs.append(f)

    # TODO: dex.py takes --incremental-dir to reuse the .dex produced in a previous build.
    # Should remote dex action also take this?
    ctx.actions.fix(
        inputs = cmd.inputs + inputs,
        outputs = cmd.outputs + outputs,
    )

def __android_turbine_handler(ctx, cmd):
    inputs = []
    for i, arg in enumerate(cmd.args):
        for k in ["--classpath=", "--processorpath="]:
            if arg.startswith(k):
                arg = arg.removeprefix(k)
                _, v = __filearg(ctx, arg)
                for f in v:
                    f, _, _ = f.partition(":")
                    inputs.append(ctx.fs.canonpath(f))

    ctx.actions.fix(
        inputs = cmd.inputs + inputs,
    )

def __deps_configs(ctx, f, seen, inputs):
    if f in seen:
        return
    seen[f] = True
    inputs.append(f)
    v = json.decode(str(ctx.fs.read(f)))
    for f in v["deps_info"]["deps_configs"]:
        f = ctx.fs.canonpath(f)
        __deps_configs(ctx, f, seen, inputs)
    if "public_deps_configs" in v["deps_info"]:
        for f in v["deps_info"]["public_deps_configs"]:
            f = ctx.fs.canonpath(f)
            __deps_configs(ctx, f, seen, inputs)

def __android_write_build_config_handler(ctx, cmd):
    # Script:
    #   https://crsrc.org/c/build/android/gyp/write_build_config.py
    # GN Config:
    #   https://crsrc.org/c/build/config/android/internal_rules.gni;l=122;drc=99e4f79301e108ea3d27ec84320f430490382587
    # Sample args:
    #   --type=java_library
    #   --depfile gen/third_party/android_deps/org_jetbrains_kotlinx_kotlinx_metadata_jvm_java__build_config_crbug_908819.d
    #   --deps-configs=\[\"gen/third_party/kotlin_stdlib/kotlin_stdlib_java.build_config.json\"\]
    #   --public-deps-configs=\[\]
    #   --build-config gen/third_party/android_deps/org_jetbrains_kotlinx_kotlinx_metadata_jvm_java.build_config.json
    #   --gn-target //third_party/android_deps:org_jetbrains_kotlinx_kotlinx_metadata_jvm_java
    #   --non-chromium-code
    #   --host-jar-path lib.java/third_party/android_deps/org_jetbrains_kotlinx_kotlinx_metadata_jvm.jar
    #   --unprocessed-jar-path ../../third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_metadata_jvm/kotlinx-metadata-jvm-0.1.0.jar
    #   --interface-jar-path obj/third_party/android_deps/org_jetbrains_kotlinx_kotlinx_metadata_jvm.ijar.jar
    #   --is-prebuilt
    #   --bundled-srcjars=\[\]
    inputs = []
    seen = {}
    for i, arg in enumerate(cmd.args):
        if arg in ["--shared-libraries-runtime-deps", "--secondary-abi-shared-libraries-runtime-deps"]:
            inputs.append(ctx.fs.canonpath(cmd.args[i + 1]))
            continue
        if arg == "--tested-apk-config":
            f = ctx.fs.canonpath(cmd.args[i + 1])
            __deps_configs(ctx, f, seen, inputs)
            continue
        for k in ["--deps-configs=", "--public-deps-configs=", "--annotation-processor-configs="]:
            if arg.startswith(k):
                arg = arg.removeprefix(k)
                v = json.decode(arg)
                for f in v:
                    f = ctx.fs.canonpath(f)
                    __deps_configs(ctx, f, seen, inputs)

    ctx.actions.fix(inputs = cmd.inputs + inputs)

__handlers = {
    "android_compile_resources": __android_compile_resources_handler,
    "android_compile_java": __android_compile_java_handler,
    "android_dex": __android_dex_handler,
    "android_turbine": __android_turbine_handler,
    "android_write_build_config": __android_write_build_config_handler,
}

android = module(
    "android",
    enabled = __enabled,
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
