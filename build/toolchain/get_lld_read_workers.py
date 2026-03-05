#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import multiprocessing
import sys


def main():
  try:
    cpu_count = multiprocessing.cpu_count()
  except:
    cpu_count = 1

  # We use cpu_count / 4 as a heuristic.
  # We might want to cap this at 8 (e.g. min(8, cpu_count // 4)) if we observe
  # excessive lock contention on high-core machines, but for now we just
  # scale it linearly.
  workers = max(1, cpu_count // 4)
  print(workers)
  return 0


if __name__ == '__main__':
  sys.exit(main())
