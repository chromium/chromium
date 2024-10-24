# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for protoc_wrapper and protoc_java.

scripts:
- https://chromium.googlesource.com/chromium/src/+/refs/heads/main/tools/protoc_wrapper/protoc_wrapper.py
- https://chromium.googlesource.com/chromium/src/+/refs/heads/main/build/protoc_java.py

gn:
- https://chromium.googlesource.com/chromium/src/+/refs/heads/main/third_party/protobuf/proto_library.gni
- https://chromium.googlesource.com/chromium/src/+/refs/heads/main/build/config/android/rules.gni

sample command line:
  command = python3 ../../tools/protoc_wrapper/protoc_wrapper.py test.proto --protoc ./protoc --proto-in-dir ../../base/test --descriptor-set-out gen/base/test/test_proto.descriptor --descriptor-set-dependency-file gen/base/test/test_proto.descriptor.d

  command = python3 ../../build/protoc_java.py --depfile gen/android_webview/proto/aw_variations_seed_proto_java__protoc_java.d --protoc ../../third_party/android_build_tools/protoc/cipd/protoc --proto-path ../../android_webview/proto --srcjar gen/android_webview/proto/aw_variations_seed_proto_java__protoc_java.srcjar ../../android_webview/proto/aw_variations_seed.proto
"""

load("@builtin//struct.star", "module")
load("@builtin//path.star", "path")

def __protoc_scandeps(ctx, proto, proto_paths = []):
    inputs = [proto]
    for line in str(ctx.fs.read(proto)).split("\n"):
        if line.startswith("import "):
            line = line.removeprefix("import ").strip()
            line = line.removeprefix("public ").strip()
            line = line.removeprefix("\"")
            i = line.find("\"")
            name = line[:i]
            for d in proto_paths:
                fname = path.join(d, name)
                if ctx.fs.exists(fname):
                    inputs.extend(__protoc_scandeps(ctx, fname, proto_paths))
                    break
    return inputs

def __scandeps(ctx, args):
    protoc_wrapper_args = []
    for i, arg in enumerate(args):
        if path.base(arg) in ["protoc_wrapper.py", "protoc_java.py"]:
            protoc_wrapper_args = args[i + 1:]
            break
    inputs = []
    protos = []
    proto_in_dir = ctx.fs.canonpath(".")
    inc_dirs = []
    flag = ""
    flags = (
        "--proto-in-dir",
        "--import-dir",
        "--protoc",
        "--protoc-gen-js",
        "--cc-out-dir",
        "--py-out-dir",
        "--js-out-dir",
        "--plugin-out-dir",
        "--plugin",
        "--plugin-options",
        "--cc-options",
        "--include",
        "--descriptor-set-out",
        "--descriptor-set-dependency-file",
        "--depfile",  # protoc_java
        "--protoc",  # protoc_java
        "--proto-path",  # protoc_java
        "--srcjar",  # protoc_java
    )
    for i, arg in enumerate(protoc_wrapper_args):
        if flag == "--proto-in-dir":
            proto_in_dir = ctx.fs.canonpath(arg)
            flag = ""
            continue
        elif flag == "--import-dir":
            inc_dirs.append(ctx.fs.canonpath(arg))
            flag = ""
            continue
        elif flag == "--plugin":
            inputs.append(ctx.fs.canonpath(arg))
            flag = ""
            continue
        elif flag == "--protoc":
            inputs.append(ctx.fs.canonpath(arg))
            flag = ""
            continue
        elif flag == "--protoc-gen-js":
            inputs.append(ctx.fs.canonpath(arg))
            flag = ""
            continue
        elif flag == "--proto-path":
            inc_dirs = [ctx.fs.canonpath(arg)] + inc_dirs
            flag = ""
            continue
        elif flag != "":
            flag = ""
            continue
        elif arg in flags:
            flag = arg
            continue
        elif arg.startswith("--import-dir="):
            inc_dirs.append(ctx.fs.canonpath(arg.removeprefix("--import-dir=")))
            continue
        elif arg.startswith("-"):
            continue
        protos.append(arg)
    proto_paths = [proto_in_dir] + inc_dirs
    protos = [path.join(proto_in_dir, name) for name in protos]
    for proto in protos:
        inputs.extend(__protoc_scandeps(ctx, proto, proto_paths = proto_paths))
    return inputs

protoc_wrapper = module(
    "protoc_wrapper",
    scandeps = __scandeps,
)
