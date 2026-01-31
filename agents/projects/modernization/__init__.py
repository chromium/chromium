# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Modernization agent projects."""

import os
import sys

# Add the Chromium root to sys.path to allow for fully qualified imports.
sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..')))
