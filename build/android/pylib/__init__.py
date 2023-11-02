# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import sys


_SRC_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))

_THIRD_PARTY_PATH = os.path.join(_SRC_PATH, 'third_party')

_CATAPULT_PATH = os.path.join(_THIRD_PARTY_PATH, 'catapult')

_DEVIL_PATH = os.path.join(_CATAPULT_PATH, 'devil')

_PYTRACE_PATH = os.path.join(_CATAPULT_PATH, 'common', 'py_trace_event')

_PY_UTILS_PATH = os.path.join(_CATAPULT_PATH, 'common', 'py_utils')

_SIX_PATH = os.path.join(_THIRD_PARTY_PATH, 'six', 'src')

_TRACE2HTML_PATH = os.path.join(_CATAPULT_PATH, 'tracing')

_BUILD_UTIL_PATH = os.path.join(_SRC_PATH, 'build', 'util')

if _DEVIL_PATH not in sys.path:
  sys.path.append(_DEVIL_PATH)

if _PYTRACE_PATH not in sys.path:
  sys.path.append(_PYTRACE_PATH)

if _PY_UTILS_PATH not in sys.path:
  sys.path.append(_PY_UTILS_PATH)

if _TRACE2HTML_PATH not in sys.path:
  sys.path.append(_TRACE2HTML_PATH)

if _SIX_PATH not in sys.path:
  sys.path.append(_SIX_PATH)

if _BUILD_UTIL_PATH not in sys.path:
  sys.path.insert(0, _BUILD_UTIL_PATH)
