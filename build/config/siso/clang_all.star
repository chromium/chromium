# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang."""

load("@builtin//struct.star", "module")

__filegroups = {
    "third_party/libc++/src/include:headers": {
        "type": "glob",
        "includes": ["*"],
        # can't use "*.h", because c++ headers have no extension.
    },
    "third_party/libc++abi/src/include:headers": {
        "type": "glob",
        "includes": ["*.h"],
    },

    # toolchain root
    # :headers for compiling
    "third_party/llvm-build/Release+Asserts:headers": {
        "type": "glob",
        "includes": [
            "*.h",
            "bin/clang",
            "bin/clang++",
            "bin/clang-cl.exe",
        ],
    },
}

__input_deps = {
    # need this because we use
    # third_party/libc++/src/include:headers,
    # but scandeps doesn't scan `__config` file, which uses
    # `#include <__config_site>`
    "third_party/libc++/src/include": [
        "buildtools/third_party/libc++/__config_site",
    ],
}

clang_all = module(
    "clang_all",
    filegroups = __filegroups,
    input_deps = __input_deps,
)
