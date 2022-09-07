# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit tests for ProductionSupportedFlagList.java
"""

USE_PYTHON3 = True


import os
import sys

def _SetupImportPath(input_api):
  android_webview_common_dir = input_api.PresubmitLocalPath()
  _CHROMIUM_SRC = os.path.join(android_webview_common_dir, os.pardir, os.pardir,
      os.pardir, os.pardir, os.pardir, os.pardir, os.pardir)
  sys.path.append(os.path.join(_CHROMIUM_SRC, 'android_webview', 'tools'))

def CheckChangeOnUpload(input_api, output_api):
  _SetupImportPath(input_api)
  import generate_flag_labels
  results = []
  results.extend(generate_flag_labels.CheckMissingWebViewEnums(input_api,
                                                               output_api))
  return results
