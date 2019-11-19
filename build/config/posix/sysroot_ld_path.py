# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file takes two arguments, the relative location of the shell script that
# does the checking, and the name of the sysroot.

# TODO(brettw) the build/linux/sysroot_ld_path.sh script should be rewritten in
# Python in this file.

from __future__ import print_function

import subprocess
import sys

if len(sys.argv) != 3:
  print("Need two arguments")
  sys.exit(1)

result = subprocess.check_output([sys.argv[1],
                                  sys.argv[2]]).strip().decode("utf-8")
result = result.replace(" ", "\n")
if result != "":
  print(result)
