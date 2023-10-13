#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def main():
  if len(sys.argv) != 2:
    print("This should have a path to reclient config file in its args.",
          file=sys.stderr)
    return 1

  # Check path to rbe_cc_cfg_file.
  if os.path.isfile(sys.argv[1]):
    return 0

  print("""
  reclient config file "%s" doesn't exist, you may need to set
  "download_remoteexec_cfg" in .gclient like
  ```
  solutions = [
    {
      "name"        : "src",
      # ...
      "custom_vars" : {
        "download_remoteexec_cfg": True,
      },
    },
  ]
  ```
  and re-run `gclient sync`.

  Or you may not set appropriate `rbe_cfg_dir` value in args.gn.

  See
  https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md#use-reclient
  for more details.
  """ % (sys.argv[1]),
        file=sys.stderr)

  return 1


if __name__ == "__main__":
  sys.exit(main())
