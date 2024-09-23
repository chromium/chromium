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
