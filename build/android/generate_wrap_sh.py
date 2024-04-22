#! /usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import pathlib


# Disable memcmp overlap check.There are blobs (gl drivers)
# on some android devices that use memcmp on overlapping regions,
# nothing we can do about that.
#EXTRA_OPTIONS = 'strict_memcmp=0,use_sigaltstack=1'
def _generate(arch):
  return f"""\
#!/system/bin/sh
# See: https://github.com/google/sanitizers/wiki/AddressSanitizerOnAndroid/\
01f8df1ac1a447a8475cdfcb03e8b13140042dbd#running-with-wrapsh-recommended
HERE="$(cd "$(dirname "$0")" && pwd)"
log "Launching with ASAN enabled: $0 $@"
export ASAN_OPTIONS=log_to_syslog=false,allow_user_segv_handler=1
export LD_PRELOAD=$HERE/libclang_rt.asan-{arch}-android.so
exec "$@"
"""


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--arch', required=True)
  parser.add_argument('--output', required=True)
  args = parser.parse_args()

  pathlib.Path(args.output).write_text(_generate(args.arch))


if __name__ == '__main__':
  main()
