# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
Checks that each RuntimeFeature gated mojo method used for glic has a
corresponding '// MOJO_RUNTIME_FEATURE_GATED' comment where that method
is excluded from the public API.

Calling RuntimeFeature gated methods when the feature is not enabled
will result in a closed mojo pipe, putting Glic in an unusable state.
This check helps reduce the risk for accidentally calling such methods.
'''

import os
import re
import sys


def _GetDirAbove(dirname: str):
    """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
    path = os.path.abspath(__file__)
    while True:
        path, tail = os.path.split(path)
        if not tail:
            return None
        if tail == dirname:
            return path


SRC_ROOT = _GetDirAbove('chrome')


def _Main():
    with open(os.path.join(SRC_ROOT, 'chrome/browser/glic/host/glic.mojom'),
              'r') as f:
        mojom_text = f.read()
    api_client_path = ('chrome/browser/resources/glic/glic_api_impl/' +
                       'glic_api_client.ts')
    with open(os.path.join(SRC_ROOT, api_client_path), 'r') as f:
        client_src = f.read()
    error = False

    checked_methods = set(
        m.group(1) for m in re.finditer(
            r'//\s*MOJO_RUNTIME_FEATURE_GATED\s+(\S*)', client_src))
    gated_methods = set(
        m.group(1) for m in re.finditer(
            r'^\s*\[[^\]]*RuntimeFeature\s*=[^\]]*]\s*(\S+)\s*\(',
            mojom_text,
            flags=re.M | re.DOTALL))
    unchecked = gated_methods - checked_methods
    ungated = checked_methods - gated_methods

    for method in unchecked:
        decl = f'// MOJO_RUNTIME_FEATURE_GATED {method}'
        print('Error: missing feature gating code for feature',
              f'gated Mojo method {method}. Please update',
              f' {api_client_path} with the line:\n  {decl}')
        error = True
    for method in ungated:
        decl = f'// MOJO_RUNTIME_FEATURE_GATED {method}'
        print(f'Error: found "{decl}", but this'
              ' method was not found in glic.mojom ')
        error = True

    if error:
        sys.exit(1)


if __name__ == '__main__':
    _Main()
