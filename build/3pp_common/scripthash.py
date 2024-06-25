# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import os
import pathlib
import sys

_SRC_ROOT = str(pathlib.Path(__file__).resolve().parents[2])


def _find_deps():
    module_paths = (os.path.abspath(m.__file__) for m in sys.modules.values()
                    if m and getattr(m, '__file__', None))
    ret = set()
    for path in module_paths:
        if path.startswith(_SRC_ROOT):
            if (path.endswith('.pyc')
                    or (path.endswith('c') and not os.path.splitext(path)[1])):
                path = path[:-1]
            ret.add(path)
    return sorted(ret)


def compute(extra_paths=None):
    """Compute a hash of loaded Python modules and given |extra_paths|."""
    md5 = hashlib.md5()
    for path in _find_deps():
        md5.update(pathlib.Path(path).read_bytes())
    if extra_paths:
        for path in extra_paths:
            md5.update(pathlib.Path(path).read_bytes())
    return md5.hexdigest()[:10]
