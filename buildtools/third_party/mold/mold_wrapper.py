#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import shlex
import sys

LINKER_PATH_PREFIX = '--linker-path='

# These flags are added by clang, and there seems to be no way to prevent
# it from adding them.
TO_STRIP = {
    '--use-android-relr-tags': None,
    '--pack-dyn-relocs=android+relr': '--pack-dyn-relocs=relr',
}


# Removes --use-android-relr-tags from the mold invocation.
# https://github.com/rui314/mold/issues/1544
def main():
  new_argv = []
  linker_path = None
  for arg in sys.argv:
    if arg.startswith(LINKER_PATH_PREFIX):
      linker_path = arg[len(LINKER_PATH_PREFIX):]
      continue
    arg = TO_STRIP.get(arg, arg)
    if arg is None:
      continue
    new_argv.append(arg)
    if arg[0] == '@':
      rsp_path = pathlib.Path(arg[1:])
      content = rsp_path.read_text()

      if LINKER_PATH_PREFIX in content:
        idx = content.index(LINKER_PATH_PREFIX)
        idx_end = content.index(' ', idx)
        linker_path = content[idx + len(LINKER_PATH_PREFIX):idx_end].rstrip('"')
        if content[idx - 1] == '"':
          idx -= 1
        content = content[:idx] + content[idx_end:]

      for k, v in TO_STRIP.items():
        if v is None:
          content = content.replace(f'"{k}"', '').replace(k, '')
        else:
          content = content.replace(k, v)
      rsp_path.write_text(content)

  assert linker_path
  assert os.path.exists(linker_path), linker_path
  new_argv[0] = linker_path
  os.execv(linker_path, new_argv)


if __name__ == '__main__':
  main()
