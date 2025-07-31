#!/usr/bin/env python

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import subprocess

_CURRENT_DIR = pathlib.Path(__file__).parent
_OUT_OF_DATE_ERROR = """Your libcxx_headers.gni is out of date.

If you synced without running hooks, run `gclient sync`

If you were messing around with the libc++ repository, run:
`buildtools/third_party/libc++/generate_libcxx_headers.py`

In any other scenario, this *should not* happen. You can temporarily solve the
problem by running the above command, but please file a bug and assign it to
msta@ with reproduction details.""".replace('\n', '$0x0A')


def _get_headers(include_dir: pathlib.Path) -> list[str]:
    headers = []
    for (dirpath, _, filenames) in os.walk(include_dir):
        dirpath = pathlib.Path(dirpath).relative_to(include_dir)
        for f in filenames:
            path = dirpath / f
            if f != 'CMakeLists.txt' and '__cxx03' not in path.parts:
                headers.append(str(path))
    headers.sort()
    return headers


def _get_libcxx_revision(path: pathlib.Path) -> str:
    return subprocess.run(
        ['git', 'rev-parse', 'HEAD'],
        cwd=path,
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    ).stdout.strip()


def _write_headers(path: pathlib.Path, headers: list[str],
                   libcxx_revision: str):
    lines = [f'  "//third_party/libc++/src/include/{hdr}",' for hdr in headers]
    header_lines = '\n'.join(lines)
    path.write_text(f"""# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# DO NOT EDIT. This file is generated.
# This file should automatically be generated as a gclient hook.
# To manually regenerate, run:
# buildtools/third_party/libc++/generate_libcxx_headers.py

import("//buildtools/deps_revisions.gni")

# Enconding newlines is a pain.
# See https://gn.googlesource.com/gn/+/refs/heads/main/docs/language.md#Strings
assert(libcxx_revision == "{libcxx_revision}", "{_OUT_OF_DATE_ERROR}")

libcxx_headers = [
{header_lines}
]
""")


if __name__ == '__main__':
    include_dir = _CURRENT_DIR / '../../../third_party/libc++/src/include'
    _write_headers(
        _CURRENT_DIR / 'generated_libcxx_headers.gni',
        _get_headers(include_dir),
        _get_libcxx_revision(include_dir),
    )
