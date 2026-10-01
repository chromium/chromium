# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _CheckChangeOnUploadOrCommit(input_api, output_api):
  import sys
  old_sys_path, cwd = sys.path[:], input_api.PresubmitLocalPath()
  src_root = input_api.os_path.join(cwd, '..', '..', '..', '..')
  try:
    sys.path += [input_api.os_path.join(src_root, 'tools', 'web_dev_style')]
    import web_dev_style.presubmit_support
  finally:
    sys.path = old_sys_path
  return web_dev_style.presubmit_support.DisallowIncludes(input_api, output_api,
      '<include> does not work in settings; use HTML imports instead')


def CheckChangeOnUpload(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)
