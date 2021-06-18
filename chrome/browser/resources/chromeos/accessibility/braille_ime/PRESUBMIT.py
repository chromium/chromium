# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True

"""Presubmit script for the Braille IME."""

def CheckChangeOnUpload(input_api, output_api):
  def FileFilter(path):
    return path.endswith('.js') or path.endswith('check_braille_ime.py')
  if not any((FileFilter(p) for p in input_api.LocalPaths())):
    return []
  import sys
  if not sys.platform.startswith('linux'):
    return []
  sys.path.insert(0, input_api.PresubmitLocalPath())
  try:
    from check_braille_ime import CheckBrailleIme
  finally:
    sys.path.pop(0)
  success, output = CheckBrailleIme()
  if not success:
    return [output_api.PresubmitError(
        'Braille IME closure compilation failed',
        long_text=output)]
  return []
