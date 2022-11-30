# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pylib.base import environment


class LocalMachineEnvironment(environment.Environment):

  def __init__(self, _args, output_manager, _error_func):
    super().__init__(output_manager)

  #override
  def SetUp(self):
    pass

  #override
  def TearDown(self):
    pass
