#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import devil_chromium
from devil.android.tools import video_recorder

if __name__ == '__main__':
  devil_chromium.Initialize()
  sys.exit(video_recorder.main())
