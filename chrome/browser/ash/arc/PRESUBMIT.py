# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def CheckChangeOnUpload(input_api, output_api):
  # Apply the same PRESUBMIT for components/arc.
  presubmit_path = (
      input_api.change.RepositoryRoot() + '/components/arc/PRESUBMIT.py')
  presubmit_content = input_api.ReadFile(presubmit_path)
  global_vars = {}
  exec(presubmit_content, global_vars)
  return global_vars['CheckChangeOnUpload'](input_api, output_api)
