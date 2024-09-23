#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copied from Skia's //gn/cp.py

import os
import shutil
import sys

src, dst = sys.argv[1:]

if os.path.exists(dst):
  if os.path.isdir(dst):
    shutil.rmtree(dst)
  else:
    os.remove(dst)

if os.path.isdir(src):
  shutil.copytree(src, dst)
else:
  shutil.copy2(src, dst)
  #work around https://github.com/ninja-build/ninja/issues/1554
  os.utime(dst, None)
