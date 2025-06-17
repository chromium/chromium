#! /usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import pathlib


def _generate(arch, hwasan, asan, env):
  ret = """\
#!/system/bin/sh
"""

  if hwasan:
    # https://developer.android.com/ndk/guides/hwasan#wrapsh
    ret += """
# import options file
_HWASAN_OPTIONS=$(cat /data/local/tmp/hwasan.options 2> /dev/null || true)

log -t cr_wrap.sh -- "Launching with HWASAN enabled."
"""
    env['HWASAN_OPTIONS'] = '$_HWASAN_OPTIONS'
    env['LD_HWASAN'] = '1'

  if asan:
    # https://github.com/google/sanitizers/wiki/AddressSanitizerOnAndroid/01f8df1ac1a447a8475cdfcb03e8b13140042dbd#running-with-wrapsh-recommended
    ret += f"""
HERE="$(cd "$(dirname "$0")" && pwd)"
# Options suggested by wiki docs:
_ASAN_OPTIONS="log_to_syslog=false,allow_user_segv_handler=1"
# Chromium-specific option (supposedly for graphics drivers):
_ASAN_OPTIONS="$_ASAN_OPTIONS,strict_memcmp=0,use_sigaltstack=1"
_LD_PRELOAD="$HERE/libclang_rt.asan-{arch}-android.so"

log -t cr_wrap.sh -- "Launching with ASAN enabled."
"""
    env['LD_PRELOAD'] = '$_LD_PRELOAD'
    env['ASAN_OPTIONS'] = '$_ASAN_OPTIONS'

  ret += 'log -t cr_wrap.sh -- "Command: $0 $@"\n'

  # Cannot set env vars before calling "log" commands, because they would
  # affect the "log" executable.
  for key, value in env.items():
    ret += f'log -t cr_wrap.sh -- "{key}={value}"\n'

  for key, value in env.items():
    ret += f'export {key}="{value}"\n'

  ret += """
exec "$@"
"""
  return ret


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output', required=True)
  parser.add_argument('--arch', required=True)
  parser.add_argument('--hwasan', action='store_true', default=False)
  parser.add_argument('--asan', action='store_true', default=False)
  parser.add_argument('--env')
  args = parser.parse_args()

  env = {}
  if args.env:
    for prop in args.env.split():
      key, value = prop.split('=', 1)
      env[key] = value
  pathlib.Path(args.output).write_text(
      _generate(args.arch, args.hwasan, args.asan, env))


if __name__ == '__main__':
  main()
