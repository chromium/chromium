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
    apinotes_by_submodule = json.load(f)

  # Merge apinotes from all submodules
  all_apinotes = {"Name": "CxxImports"}
  for submodule_apinotes in apinotes_by_submodule:
    for key, value in recursive_sort_dicts(submodule_apinotes).items():
      if key in all_apinotes:
        all_apinotes[key].extend(value)
      else:
        all_apinotes[key] = value
  with open(args.output, "w") as f:
    f.write(pyyaml.dump(all_apinotes, indent=2))


def key_for_sorting(item):
  # This will put the key "Name" first, then sort the rest alphabetically.
  # Note: False < True in Python sorting.
  return (item[0] != "Name", item[0])


def recursive_sort_dicts(data):
  if isinstance(data, list):
    return [recursive_sort_dicts(x) for x in data]
  elif isinstance(data, dict):
    return {
        k: recursive_sort_dicts(v)
        for k, v in sorted(data.items(), key=key_for_sorting)
    }
  else:
    return data


if __name__ == "__main__":
  main(sys.argv[1:])
