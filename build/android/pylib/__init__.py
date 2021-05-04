# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
import os
import sys


_THIRD_PARTY_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', 'third_party'))

_CATAPULT_PATH = os.path.join(_THIRD_PARTY_PATH, 'catapult')

_DEVIL_PATH = os.path.join(_CATAPULT_PATH, 'devil')

_PYTRACE_PATH = os.path.join(_CATAPULT_PATH, 'common', 'py_trace_event')

_PY_UTILS_PATH = os.path.join(_CATAPULT_PATH, 'common', 'py_utils')

_SIX_PATH = os.path.join(_THIRD_PARTY_PATH, 'six', 'src')

_TRACE2HTML_PATH = os.path.join(_CATAPULT_PATH, 'tracing')


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
