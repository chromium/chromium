# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class TestException(Exception):
  """Base class for exceptions thrown by the test runner."""


class InvalidShardingSettings(TestException):
  def __init__(self, shard_index, total_shards):
    super().__init__(
        'Invalid sharding settings. shard_index: %d total_shards: %d' %
        (shard_index, total_shards))


class InstallationError(TestException):
  """When installation of apk, apex, etc., has any error."""


class InstallationFailedError(InstallationError):
  """When installation of apk, apex, etc., fails."""


class InstallationTimeoutError(InstallationError):
  """When installation of apk, apex, etc., times out."""


class StartInstrumentationError(TestException):
  """When "am instrument" command has any error."""


class StartInstrumentationFailedError(StartInstrumentationError):
  """When "am instrument" command fails."""


class StartInstrumentationTimeoutError(StartInstrumentationError):
  """When "am instrument" command times out."""


class StartInstrumentationStdoutError(StartInstrumentationError):
  """When the command to read the instrumentation stdout file."""
