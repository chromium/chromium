# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""autoninja wrapper to ensure agent is not flooded with build output.

Add to your .gemini/settings.json:
  "excludeTools": [
    "run_shell_command(autoninja)", "use agent_autoninja instead"
  ],

And be sure to include in your //GEMINI.md:
@agents/prompts/templates/autoninja.md

We can probably remove this script if gemini-cli allows us hooks to ensure that
"autoninja" is always run with "--quiet".
"""

import os
import sys
import subprocess

MAX_LINES_HEAD = 260
MAX_LINES_TAIL = 140

cmd = ['autoninja']
if '--quiet' not in sys.argv:
  cmd += ['--quiet']
cmd += sys.argv[1:]

output_dir = '.'
for i, arg in enumerate(cmd):
  if arg == '-C':
    output_dir = sys.argv[i + 1]
    break
  if arg.startswith('-C'):
    output_dir = arg[2:]
    break

proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
lines = proc.stdout.readlines()

# Limit output so as to not confuse the model. It generally can work (well) on
# only a few errors at time anyways.
sys.stdout.writelines(lines[:MAX_LINES_HEAD])
lines = lines[MAX_LINES_HEAD:]

if lines:
  omit_count = len(lines) - MAX_LINES_TAIL
  if omit_count > 0:
    print()
    print(f'Skipping {omit_count} lines of output...')
    print()
  sys.stdout.writelines(lines[-MAX_LINES_TAIL:])

sys.exit(proc.wait())
