# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility methods for Skia Gold functionality unittests."""

import argparse
import collections
import typing
from typing import Optional

import dataclasses  # Built-in, but pylint gives an ordering false positive.


@dataclasses.dataclass
class _SkiaGoldArgs():
  local_pixel_tests: Optional[bool] = None
  no_luci_auth: Optional[bool] = None
  service_account: Optional[str] = None
  code_review_system: Optional[str] = None
  continuous_integration_system: Optional[str] = None
  git_revision: Optional[str] = None
  gerrit_issue: Optional[int] = None
  gerrit_patchset: Optional[int] = None
  buildbucket_id: Optional[int] = None
  bypass_skia_gold_functionality: Optional[bool] = None
  skia_gold_local_png_write_directory: Optional[str] = None


def createSkiaGoldArgs(*args, **kwargs):
  return typing.cast(argparse.Namespace, _SkiaGoldArgs(*args, **kwargs))
