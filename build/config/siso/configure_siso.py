#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script is used to configure siso."""

import argparse
import os
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))

SISO_PROJECT_CFG = "SISO_PROJECT"
SISO_ENV = os.path.join(THIS_DIR, ".sisoenv")


def ReadConfig():
  entries = {}
  if not os.path.isfile(SISO_ENV):
      print('The .sisoenv file has not been generated yet')
      return entries
  with open(SISO_ENV, 'r') as f:
    for line in f:
      parts = line.strip().split('=')
      if len(parts) > 1:
        entries[parts[0].strip()] = parts[1].strip()
    return entries


def main():
  parser = argparse.ArgumentParser(description="configure siso")
  parser.add_argument("--rbe_instance", help="RBE instance to use for Siso")
  parser.add_argument("--get-siso-project",
                      help="Print the currently configured siso project to "
                      "stdout",
                      action="store_true")
  args = parser.parse_args()

  if args.get_siso_project:
    config = ReadConfig()
    if not SISO_PROJECT_CFG in config:
      return 1
    print(config[SISO_PROJECT_CFG])
    return 0

  project = None
  rbe_instance = args.rbe_instance
  if rbe_instance:
    elems = rbe_instance.split("/")
    if len(elems) == 4 and elems[0] == "projects":
      project = elems[1]
      rbe_instance = elems[-1]

  with open(SISO_ENV, "w") as f:
    if project:
      f.write("%s=%s\n" % (SISO_PROJECT_CFG, project))
    if rbe_instance:
      f.write("SISO_REAPI_INSTANCE=%s\n" % rbe_instance)
  return 0


if __name__ == "__main__":
  sys.exit(main())
