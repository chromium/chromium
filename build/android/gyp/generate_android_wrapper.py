#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.

sys.path.append(
    os.path.abspath(
        os.path.join(os.path.dirname(__file__), '..', '..', 'util')))

import generate_wrapper

_WRAPPED_PATH_LIST_RE = re.compile(r'@WrappedPathList\(([^,]+), ([^)]+)\)')


def ExpandWrappedPathLists(args):
  expanded_args = []
  for arg in args:
    m = _WRAPPED_PATH_LIST_RE.match(arg)
    if m:
      for p in action_helpers.parse_gn_list(m.group(2)):
        expanded_args.extend([m.group(1), '@WrappedPath(%s)' % p])
    else:
      expanded_args.append(arg)
  return expanded_args


def main(raw_args):
  parser = generate_wrapper.CreateArgumentParser()
  expanded_raw_args = build_utils.ExpandFileArgs(raw_args)
  expanded_raw_args = ExpandWrappedPathLists(expanded_raw_args)
  args = parser.parse_args(expanded_raw_args)
  return generate_wrapper.Wrap(args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
