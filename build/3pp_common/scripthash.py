# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import logging
import os
import pathlib
import sys

_SRC_ROOT = pathlib.Path(__file__).resolve().parents[2]


def _find_deps():
    module_paths = (os.path.abspath(m.__file__) for m in sys.modules.values()
                    if m and getattr(m, '__file__', None))
    ret = set()
    for path in module_paths:
        if path.startswith(str(_SRC_ROOT)):
            if (path.endswith('.pyc')
                    or (path.endswith('c') and not os.path.splitext(path)[1])):
                path = path[:-1]
            ret.add(path)
    return list(ret)


def compute(extra_paths=None):
    """Compute a hash of loaded Python modules and given |extra_paths|."""
    all_paths = _find_deps() + (extra_paths or [])
    all_paths = [os.path.relpath(p, _SRC_ROOT) for p in all_paths]
    all_paths.sort()
    md5 = hashlib.md5()
    for path in all_paths:
        md5.update((_SRC_ROOT / path).read_bytes())
        md5.update(path.encode('utf-8'))
    logging.info('Script hash from: \n%s\n', '\n'.join(all_paths))
    return md5.hexdigest()[:10]
