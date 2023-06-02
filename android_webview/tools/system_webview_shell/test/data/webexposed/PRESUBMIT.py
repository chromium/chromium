# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys

def _SetupImportPath(input_api):
  webview_data_dir = input_api.PresubmitLocalPath()
  if webview_data_dir not in sys.path:
    sys.path.append(webview_data_dir)


def CheckCommonSteps(input_api, output_api):
  _SetupImportPath(input_api)
  # pylint: disable=import-outside-toplevel
  from exposed_webview_interfaces_presubmit import (
      CheckNotWebViewExposedInterfaces)
  return CheckNotWebViewExposedInterfaces(input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
  return CheckCommonSteps(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckCommonSteps(input_api, output_api)
