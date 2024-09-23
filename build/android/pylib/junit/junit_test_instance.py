# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from pylib.base import test_instance
from pylib.utils import test_filter


class JunitTestInstance(test_instance.TestInstance):

  def __init__(self, args, _):
    super().__init__()

    self._coverage_dir = args.coverage_dir
    self._debug_socket = args.debug_socket
    self._coverage_on_the_fly = args.coverage_on_the_fly
    self._native_libs_dir = args.native_libs_dir
    self._package_filter = args.package_filter
    self._resource_apk = args.resource_apk
    self._robolectric_runtime_deps_dir = args.robolectric_runtime_deps_dir
    self._runner_filter = args.runner_filter
    self._json_config = args.json_config
    self._shadows_allowlist = args.shadows_allowlist
    self._shards = args.shards
    self._shard_filter = None
    if args.shard_filter:
      self._shard_filter = {int(x) for x in args.shard_filter.split(',')}
    self._test_filters = test_filter.InitializeFiltersFromArgs(args)
    self._test_suite = args.test_suite

  #override
  def TestType(self):
    return 'junit'

  #override
  def SetUp(self):
    pass

  #override
  def TearDown(self):
    pass

  @property
  def coverage_dir(self):
    return self._coverage_dir

  @property
  def coverage_on_the_fly(self):
    return self._coverage_on_the_fly

  @property
  def debug_socket(self):
    return self._debug_socket

  @property
  def native_libs_dir(self):
    return self._native_libs_dir

  @property
  def package_filter(self):
    return self._package_filter

  @property
  def resource_apk(self):
    return self._resource_apk

  @property
  def robolectric_runtime_deps_dir(self):
    return self._robolectric_runtime_deps_dir

  @property
  def runner_filter(self):
    return self._runner_filter

  @property
  def shadows_allowlist(self):
    return self._shadows_allowlist

  @property
  def test_filters(self):
    return self._test_filters

  @property
  def json_config(self):
    return self._json_config

  @property
  def shards(self):
    return self._shards

  @property
  def shard_filter(self):
    return self._shard_filter

  @property
  def suite(self):
    return self._test_suite
