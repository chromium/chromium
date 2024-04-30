#! /usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import pathlib


def _generate(arch):
  return f"""\
#!/system/bin/sh
# See: https://github.com/google/sanitizers/wiki/AddressSanitizerOnAndroid/\
01f8df1ac1a447a8475cdfcb03e8b13140042dbd#running-with-wrapsh-recommended
HERE="$(cd "$(dirname "$0")" && pwd)"
# Options suggested by wiki docs:
_ASAN_OPTIONS="log_to_syslog=false,allow_user_segv_handler=1"
# Chromium-specific option (supposedly for graphics drivers):
_ASAN_OPTIONS="$_ASAN_OPTIONS,strict_memcmp=0,use_sigaltstack=1"
_LD_PRELOAD="$HERE/libclang_rt.asan-{arch}-android.so"
log -t cr_wrap.sh -- "Launching with ASAN enabled."
log -t cr_wrap.sh -- "LD_PRELOAD=$_LD_PRELOAD"
log -t cr_wrap.sh -- "ASAN_OPTIONS=$_ASAN_OPTIONS"
log -t cr_wrap.sh -- "Command: $0 $@"
# Export LD_PRELOAD after running "log" commands to not risk it affecting
# them.
export LD_PRELOAD=$_LD_PRELOAD
export ASAN_OPTIONS=$_ASAN_OPTIONS
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
