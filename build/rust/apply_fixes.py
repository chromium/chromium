#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" A script for automatically applying fix suggestions provided by `rustc`
    or `clippy-driver`.

    See the "Automatically applying fix suggestions" section in
    `//docs/rust/clippy.md` for more information.
"""

import argparse
import itertools
import json
import os
import pathlib
import subprocess
import sys
import tempfile

REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent

# Get helpers from `//build/rust/gni_impl/rustc_wrapper.py`.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), 'gni_impl'))
from rustc_wrapper import (ConvertPathsToAbsolute, LoadRustEnvAndFlags)


class Highlight:
  """ Holds a preparsed element of `"text"` from JSON output of `rustc`.

      JSON schema can be found in
      https://doc.rust-lang.org/beta/rustc/json.html#diagnostics
  """

  def __init__(self, text):
    self.text = text["text"]
    self.start = text["highlight_start"]
    self.end = text["highlight_end"]


class Fix:
  """ Holds a preparsed element of `"spans"` from JSON output of `rustc`.

      JSON schema can be found in
      https://doc.rust-lang.org/beta/rustc/json.html#diagnostics
  """

  def __init__(self, span):
    self.file_name = span["file_name"]
    self.byte_start = span["byte_start"]
    self.byte_end = span["byte_end"]
    self.line_start = span["line_start"]
    self.line_end = span["line_end"]
    self.suggestion_applicability = span["suggestion_applicability"]
    self.suggested_replacement = span["suggested_replacement"]
    self.highlights = [Highlight(h) for h in span["text"]]


def _GetMachineApplicableFixes(errors):
  result = []
  for e in errors:
    result += _GetMachineApplicableFixes(e["children"])
    for fix in e["spans"]:
      fix = Fix(fix)
      if fix.suggestion_applicability == "MachineApplicable":
        result.append(fix)
  return result


def _IsOldTextStillPresent(content, fix):
  assert fix.byte_start <= len(content)
  end = min(len(content), fix.byte_end)
  for h in fix.highlights:
    start = fix.byte_start - h.start + 1
    current_text = content[start:end]
    old_text = h.text[:h.end - 1]
    if old_text not in current_text:
      print(f"  Fix on line {fix.line_start} doesn't apply!?")
      return False

  return True


def _ApplyFixes(content, fixes):
  fixes = list(fixes)

  # Check if all `fixes` apply to the same `file_name` (the `content` of which
  # we presumably received as one of the arguments).
  assert len(set([fix.file_name for fix in fixes])) == 1

  # Reverse sort by `byte_start` so one fix does not shift the offsets used by
  # other, not-yet-applied fixes.  Sorting by `byte_end` doesn't help to prevent
  # the shifting/overlapping problem, but seems desirable for extra determinism.
  fixes = sorted(fixes,
                 reverse=True,
                 key=lambda fix: (fix.byte_start, fix.byte_end))

  for fix in fixes:
    if _IsOldTextStillPresent(content, fix):
      print(f"  Applying a fix on line {fix.line_start}...")
      orig_prefix = content[:fix.byte_start]
      orig_suffix = content[fix.byte_end:]
      content = orig_prefix + fix.suggested_replacement + orig_suffix

  return content


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('chromium_build_dir', type=pathlib.Path)
  parser.add_argument('tool', type=pathlib.Path)
  parser.add_argument('rustc_env_and_flags', type=pathlib.Path)
  args = parser.parse_args()

  if not args.chromium_build_dir.is_dir():
    args.chromium_build_dir = REPOSITORY_ROOT / args.chromium_build_dir
  args.chromium_build_dir = args.chromium_build_dir.resolve(strict=True)
  os.chdir(args.chromium_build_dir)

  if not args.tool.is_file():
    args.tool = \
        REPOSITORY_ROOT / 'third_party' / 'rust-toolchain' / 'bin' / args.tool
  args.tool = args.tool.resolve(strict=True)

  (rustenv, rustflags) = LoadRustEnvAndFlags(args.rustc_env_and_flags)
  ConvertPathsToAbsolute(rustenv)

  # `apply_fixes.py` should not write any files into the build directory (e.g.
  # into `out/`).  `--emit=metadata` asks `rustc` and Clippy to only emit
  # `.rmeta`.
  assert not [x for x in rustflags if x.startswith("--emit")]
  assert not [x for x in rustflags if x.startswith("-o")]
  assert not [x for x in rustflags if x.startswith("--out-dir")]
  temp_dir = tempfile.TemporaryDirectory()
  rustflags += ["--out-dir", temp_dir.name]
  rustflags += ["--emit=metadata"]

  # Ask for machine-readable error reports and fix suggestions
  rustflags += ["--error-format=json"]

  print(f"Invoking {os.path.basename(args.tool)} to repro the build problem...")
  r = subprocess.run([args.tool, *rustflags],
                     env=rustenv,
                     check=False,
                     capture_output=True)
  if r.returncode == 0:
    print("Unexpectedly didn't repro any build problem.")
    return r.returncode

  errors = [json.loads(line) for line in r.stderr.splitlines()]
  fixes = _GetMachineApplicableFixes(errors)
  if not fixes:
    print("No machine-applicable fixes found...")
    return 0

  fixes = sorted(fixes, key=lambda f: f.file_name)
  fixes = itertools.groupby(fixes, key=lambda f: f.file_name)
  for file_name, fixes in fixes:
    print(f"Applying fixes to `{file_name}`...")
    with open(file_name) as f:
      content = f.read()
    content = _ApplyFixes(content, fixes)
    with open(file_name, 'w') as f:
      f.write(content)

  return 0


if __name__ == '__main__':
  sys.exit(main())
