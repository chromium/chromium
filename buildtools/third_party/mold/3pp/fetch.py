#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import sys

_3PP_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _3PP_DIR.parents[3]
sys.path.insert(0, str(_SRC_ROOT / 'build' / '3pp_common'))
import fetch_github_release


if __name__ == '__main__':
    fetch_github_release.main(project='rui314/mold',
                              artifact_filename='mold_linux.tar.gz',
                              artifact_regex=r'mold-.*-x86_64-linux.tar.gz')
