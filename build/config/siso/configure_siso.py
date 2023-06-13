#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script is used to configure siso."""

import argparse
import os
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))


def main():
  parser = argparse.ArgumentParser(description="configure siso")
  parser.add_argument("--rbe_instance", help="RBE instance to use for Siso")
  args = parser.parse_args()

  project = None
  rbe_instance = args.rbe_instance
  if rbe_instance:
    elems = rbe_instance.split("/")
    if len(elems) == 4 and elems[0] == "projects":
      project = elems[1]
      rbe_instance = elems[-1]

  siso_env_path = os.path.join(THIS_DIR, ".sisoenv")
  with open(siso_env_path, "w") as f:
    if project:
      f.write("SISO_PROJECT=%s\n" % project)
    if rbe_instance:
      f.write("SISO_REAPI_INSTANCE=%s\n" % rbe_instance)
  return 0


if __name__ == "__main__":
  sys.exit(main())
