# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import inspect
import os
import sys

USE_PYTHON3 = True
WEBVIEW_DATA_DIR = os.path.dirname(inspect.stack()[0][1])

if WEBVIEW_DATA_DIR not in sys.path:
  sys.path.append(WEBVIEW_DATA_DIR)

# pylint: disable=wrong-import-position
from exposed_webview_interfaces_presubmit import (
    PresubmitCheckNotWebViewExposedInterfaces)


def CheckChangeOnUpload(input_api, output_api):
  return PresubmitCheckNotWebViewExposedInterfaces(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return PresubmitCheckNotWebViewExposedInterfaces(input_api, output_api)
