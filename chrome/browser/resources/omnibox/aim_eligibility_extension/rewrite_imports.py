# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

in_dir = sys.argv[1]
out_dir = sys.argv[2]
os.makedirs(out_dir, exist_ok=True)

filename = sys.argv[3]
in_file = os.path.join(in_dir, filename)
out_file = os.path.join(out_dir, filename)

with open(in_file, 'r') as f:
  content = f.read()

content = content.replace('//resources/', 'chrome://resources/')

with open(out_file, 'w') as f:
  f.write(content)
