# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for determining if a test is being run locally or not."""

import os


def IsOnSwarming():
  """Determines whether we are on swarming or not.

  Returns:
    True if the test is being run on swarming, otherwise False.
  """
  # Look for the presence of the SWARMING_SERVER environment variable as a
  # heuristic to determine whether we're running on a workstation or a bot.
  # This should always be set on swarming, but would be strange to be set on
  # a workstation.
  return 'SWARMING_SERVER' in os.environ
