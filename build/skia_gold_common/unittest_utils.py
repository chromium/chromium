# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility methods for Skia Gold functionality unittests."""

import collections

_SkiaGoldArgs = collections.namedtuple('_SkiaGoldArgs', [
    'local_pixel_tests',
    'no_luci_auth',
    'code_review_system',
    'continuous_integration_system',
    'git_revision',
    'gerrit_issue',
    'gerrit_patchset',
    'buildbucket_id',
    'bypass_skia_gold_functionality',
])


def createSkiaGoldArgs(local_pixel_tests=None,
                       no_luci_auth=None,
                       code_review_system=None,
                       continuous_integration_system=None,
                       git_revision=None,
                       gerrit_issue=None,
                       gerrit_patchset=None,
                       buildbucket_id=None,
                       bypass_skia_gold_functionality=None):
  return _SkiaGoldArgs(local_pixel_tests, no_luci_auth, code_review_system,
                       continuous_integration_system, git_revision,
                       gerrit_issue, gerrit_patchset, buildbucket_id,
                       bypass_skia_gold_functionality)
