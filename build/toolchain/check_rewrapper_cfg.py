#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def missing_cfg_error_message():
  """This assumes that corp machine has gcert binary in known location."""
  import shutil
  if shutil.which("gcert") is not None:
    return """
To build with gn arg 'use_remoteexec=true' as a googler on a corp machine
set "download_remoteexec_cfg" in .gclient like

solutions = [
    {
        "name"        : "src",
        # ...
        "custom_vars" : {
            "download_remoteexec_cfg": True,
        },
    },
]

and re-run `gclient sync`.

See http://go/chrome-linux-build#setup-remote-execution
for more details."""
  elif sys.platform == 'linux':
    return """
To build with gn arg 'use_remoteexec=true' as a googler on a non corp machine
see http://go/chrome-linux-build#setup-remote-execution for setup instructions.

To build with gn arg 'use_remoteexec=true' as a non-googler set the appropriate
`reclient_cfg_dir` value in args.gn.
See
https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md#use-reclient
for more details."""
  else:
    return """
To build with gn arg 'use_remoteexec=true' as a googler on a non corp machine
see http://go/chrome-linux-build#setup-remote-execution for setup instructions.

Building with gn arg 'use_remoteexec=true' as a non-googler is not currently
supported on your os (%s).
""" % sys.platform


def main():
  if len(sys.argv) != 2:
    print("This should have a path to reclient config file in its args.",
          file=sys.stderr)
    return 1

  # Check path to reclient_cc_cfg_file.
  if os.path.isfile(sys.argv[1]):
    return 0

  print("reclient config file '%s' doesn't exist" %
        (os.path.abspath(sys.argv[1])),
        file=sys.stderr)
  print(missing_cfg_error_message(), file=sys.stderr)

  return 1


if __name__ == "__main__":
  sys.exit(main())
