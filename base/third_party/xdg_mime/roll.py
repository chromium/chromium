#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shlex
import shutil
import subprocess
import tempfile

_REPO_URL = "https://gitlab.freedesktop.org/xdg/xdgmime.git"
_FILES_TO_COPY = (
    "README",
    "src/xdgmimealias.c",
    "src/xdgmime.c",
    "src/xdgmimecache.h",
    "src/xdgmimeglob.h",
    "src/xdgmimeicon.c",
    "src/xdgmimeint.c",
    "src/xdgmimemagic.c",
    "src/xdgmimeparent.c",
    "src/xdgmimealias.h",
    "src/xdgmimecache.c",
    "src/xdgmimeglob.c",
    "src/xdgmime.h",
    "src/xdgmimeicon.h",
    "src/xdgmimeint.h",
    "src/xdgmimemagic.h",
    "src/xdgmimeparent.h",
)

_PATCHES = ("000-have-mmap.patch",)


def main():
  out_dir = os.path.dirname(os.path.realpath(__file__))

  with open(os.path.join(out_dir, "README.chromium")) as readme_file:
    _VERSION_PREFIX = "Version: "
    for line in readme_file:
      if not line.startswith(_VERSION_PREFIX):
        continue
      old_commit = line[len(_VERSION_PREFIX):].strip()
  with tempfile.TemporaryDirectory() as staging_dir:
    os.chdir(staging_dir)

    print(f"Cloning from {_REPO_URL}...")
    subprocess.check_call([
        "git",
        "clone",
        _REPO_URL,
        ".",
    ])

    for f in _FILES_TO_COPY:
      shutil.copy(os.path.join(staging_dir, f), out_dir)

    new_commit = subprocess.check_output([
        "git",
        "rev-parse",
        "HEAD",
    ]).decode("ascii").strip()

    # This is cargo-culted from depot_tool's roll_dep.py
    log_command = (
        "git",
        "log",
        f"{old_commit}..{new_commit}",
        "--date=short",
        "--no-merges",
        "--format=%ad %ae %s",
    )
    diffs = subprocess.check_output(log_command).decode("utf-8")

  os.chdir(os.path.join(out_dir, "..", "..", ".."))

  for p in _PATCHES:
    print(f"Applying patch {p}...")
    with open(os.path.join(out_dir, "patches", p)) as patch_file:
      subprocess.check_call(["patch", "-p1"], stdin=patch_file)

  print(f"Done! Updated from {old_commit} to {new_commit}")
  print("Changes:")
  print(
      f"$ git log {old_commit[:9]}..{new_commit[:9]} "
      f"--date=short --no-merges --format='%ad %ae %s'"
  )
  print(diffs)


if __name__ == "__main__":
  main()
