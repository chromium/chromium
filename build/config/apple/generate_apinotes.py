# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

_SRC_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
sys.path.append(os.path.join(_SRC_PATH, 'third_party'))
import pyyaml


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument("--input", required=True)
  parser.add_argument("--output", required=True)
  args = parser.parse_args(argv)

  with open(args.input, "r") as f:
    data = json.load(f)

  write_single_module_apinotes(data, args.output)


def write_single_module_apinotes(module_apinotes, output_path):
  assert len(module_apinotes) == 1
  module_apinotes = module_apinotes[0]
  with open(output_path, "w") as f:
    f.write(f"Name: {module_apinotes['module_name']}\n")
    f.write(pyyaml.dump(module_apinotes['apinotes'], indent=2))
    f.write("\n")


if __name__ == "__main__":
  main(sys.argv[1:])
