# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pylib.base import test_instance
from pylib.utils import test_filter


class HostsideTestInstance(test_instance.TestInstance):
  def __init__(self, args, _):
    super().__init__()
    self.suite = args.test_suite
    self.instant_mode = args.test_apk_as_instant
    self.tradefed_executable = args.tradefed_executable or 'cts-tradefed'
    self.aapt_path = args.tradefed_aapt_path or ''
    self.adb_path = args.tradefed_adb_path or ''
    self.additional_apks = args.additional_apks
    self.use_webview_provider = args.use_webview_provider
    self.max_tries = 1 if args.repeat else args.num_retries + 1
    self.test_filters = test_filter.InitializeFiltersFromArgs(args)
    self.external_shard_index = args.test_launcher_shard_index
    self.total_external_shards = args.test_launcher_total_shards

  #override
  def TestType(self):
    return 'hostside'

  #override
  def SetUp(self):
    pass

  #override
  def TearDown(self):
    pass
