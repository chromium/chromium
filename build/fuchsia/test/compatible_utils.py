# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Functions used in both v1 and v2 scripts."""

_FILTER_DIR = 'testing/buildbot/filters'


# TODO(crbug.com/1279803): Until one can send files to the device when running
# a test, filter files must be read from the test package.
def map_filter_file_to_package_file(filter_file: str) -> str:
    """Returns the path to |filter_file| within the test component's package."""

    if not _FILTER_DIR in filter_file:
        raise ValueError('CFv2 tests only support registered filter files '
                         'present in the test package')
    return '/pkg/' + filter_file[filter_file.index(_FILTER_DIR):]
