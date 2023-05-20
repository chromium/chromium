#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Automates running sysroot-creator.sh for each supported arch.
"""


import hashlib
import json
import multiprocessing
import os
import shlex
import subprocess
import sys

ARCHES = ("amd64", "i386", "armhf", "arm64", "armel", "mipsel", "mips64el")


def sha1sumfile(filename):
  sha1 = hashlib.sha1()
  with open(filename, "rb") as f:
    while True:
      data = f.read(65536)
      if not data:
        break
      sha1.update(data)
  return sha1.hexdigest()


def get_proc_output(args):
  return subprocess.check_output(args, encoding="utf-8").strip()


def build_and_upload(script_path, distro, release, key, arch, lock):
  script_dir = os.path.dirname(os.path.realpath(__file__))

  subprocess.check_output([script_path, "build", arch])
  subprocess.check_output([script_path, "upload", arch])

  tarball = "%s_%s_%s_sysroot.tar.xz" % (distro, release, arch.lower())
  tarxz_path = os.path.join(script_dir, "..", "..", "..", "out",
                            "sysroot-build", release, tarball)
  sha1sum = sha1sumfile(tarxz_path)
  sysroot_dir = "%s_%s_%s-sysroot" % (distro, release, arch.lower())

  sysroot_metadata = {
      "Key": key,
      "Sha1Sum": sha1sum,
      "SysrootDir": sysroot_dir,
      "Tarball": tarball,
  }
  with lock:
    fname = os.path.join(script_dir, "sysroots.json")
    sysroots = json.load(open(fname))
    with open(fname, "w") as f:
      sysroots["%s_%s" % (release, arch.lower())] = sysroot_metadata
      f.write(
          json.dumps(sysroots, sort_keys=True, indent=4,
                     separators=(",", ": ")))
      f.write("\n")


def main():
  script_dir = os.path.dirname(os.path.realpath(__file__))
  script_path = os.path.join(script_dir, "sysroot-creator.sh")
  lexer = shlex.shlex(open(script_path).read(), posix=True)
  lexer.wordchars += "="
  vars = dict(kv.split("=") for kv in list(lexer) if "=" in kv)
  distro = vars["DISTRO"]
  release = vars["RELEASE"]
  key = "%s-%s" % (vars["ARCHIVE_TIMESTAMP"], vars["SYSROOT_RELEASE"])

  procs = []
  lock = multiprocessing.Lock()
  for arch in ARCHES:
    proc = multiprocessing.Process(
        target=build_and_upload,
        args=(script_path, distro, release, key, arch, lock),
    )
    procs.append(("%s %s (%s)" % (distro, release, arch), proc))
    proc.start()
  for _, proc in procs:
    proc.join()

  print("SYSROOT CREATION SUMMARY")
  failures = 0
  for name, proc in procs:
    if proc.exitcode:
      failures += 1
    status = "FAILURE" if proc.exitcode else "SUCCESS"
    print("%s sysroot creation\t%s" % (name, status))
  return failures


if __name__ == "__main__":
  sys.exit(main())
