#!/usr/bin/env python

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib

_CURRENT_DIR = pathlib.Path(__file__).parent


def _get_headers(include_dir: pathlib.Path) -> list[str]:
    headers = []
    for (dirpath, _, filenames) in include_dir.walk():
        dirpath = dirpath.relative_to(include_dir)
        for f in filenames:
            path = dirpath / f
            if f != 'CMakeLists.txt' and '__cxx03' not in path.parts:
                headers.append(str(path))
    headers.sort()
    return headers


def _write_headers(path: pathlib.Path, headers: list[str]):
    lines = [f'  "//third_party/libc++/src/include/{hdr}",' for hdr in headers]
    path.write_text(f"""# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# DO NOT EDIT. This file is generated.
# To regenerate, run buildtools/third_party/libc++/generate_libcxx_headers.py

libcxx_headers = [
{'\n'.join(lines)}
]
""")


if __name__ == '__main__':
    _write_headers(
        _CURRENT_DIR / 'libcxx_headers.gni',
        _get_headers(_CURRENT_DIR / '../../../third_party/libc++/src/include'),
    )
