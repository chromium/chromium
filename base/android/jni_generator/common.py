# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Common logic needed by other modules."""


def capitalize(value):
  return value[0].upper() + value[1:]


def escape_class_name(fully_qualified_class):
  """Returns an escaped string concatenating the Java package and class."""
  escaped = fully_qualified_class.replace('_', '_1')
  return escaped.replace('/', '_').replace('$', '_00024')
