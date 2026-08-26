# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Filters compiler arguments to make them suitable for libclang-based tools.

Rust tools such as `bindgen` and Crubit (`rs_bindings_from_cc`) use `libclang`
to parse C/C++ headers for FFI binding generation. GN forwards full `{{cflags}}`
to these tools, which may contain arguments unsuitable for libclang:
1. Clang plugin arguments (`-Xclang`, `-add-plugin`, `-plugin-arg-*`)
2. Build-profiling arguments (`-ftime-trace`).
3. Compiler-specific warning flags (see https://crbug.com/552283279) and
   warning-as-error flags (`-Werror`, `/WX`) across all platforms (including
   clang-cl on Windows).

These arguments are filtered out because:
* Plugin arguments cannot be loaded outside full compiler invocations.
* Profiling arguments are not applicable to binding generation.
* Warnings in C/C++ headers should be reported during regular compilation
  (rather than when invoking `bindgen` or Crubit), and warnings have no
  impact on memory layout, ABI, nor other things that affect generated
  bindings.

Additionally, `-w` is appended to suppress all default warnings, and
`-Wno-unknown-warning-option`, `-Wno-unused-command-line-argument`, and
`-Wno-unknown-argument` are appended to prevent libclang from failing on unused
options (e.g. linker inputs or `/TP` in driver-mode; see crbug.com/40226863) or
unknown synthesized arguments.
"""


def filter_clang_args(clangargs):

    def do_filter(args):
        i = 0
        while i < len(args):
            arg = args[i]

            # Intercept plugin arguments:
            # -Xclang -add-plugin -Xclang <plugin_name>
            # -Xclang -plugin-arg-<plugin_name> -Xclang <arg>
            # -Xclang -plugin-arg-<plugin_name>
            # -Xclang -load -Xclang <plugin_path>
            if arg == '-Xclang' and i + 1 < len(args):
                next_arg = args[i + 1]
                if next_arg in (
                    '-add-plugin',
                    '-load',
                    '-plugin',
                ) or next_arg.startswith('-plugin-arg'):
                    if i + 3 < len(args) and args[i + 2] == '-Xclang':
                        i += 4
                        continue
                    i += 2
                    continue

            if arg == '-ftime-trace':
                i += 1
                continue

            # Filter all warning flags (Clang/GCC -W*, clang-cl /W*, /wd*,
            # /clang:-W*) and warning-as-error flags (-Werror, /WX).
            if (
                arg.startswith('-W')
                or arg.startswith('/clang:-W')
                or arg.startswith('/W')
                or arg.startswith('/wd')
            ):
                i += 1
                continue

            i += 1
            yield arg

    filtered = list(do_filter(clangargs))
    filtered.extend(
        [
            '-w',
            '-Wno-unknown-argument',
            '-Wno-unknown-warning-option',
            '-Wno-unused-command-line-argument',
        ]
    )
    return filtered
