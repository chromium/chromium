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


def _RemoveComments(text: str):
    # Remove multi-line comments first, then single-line comments.
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    text = re.sub(r'//.*', '', text)
    return text


def _FindScopeBoundaries(scope_type: str, scope_name: str, mojom_text: str):
    m = re.search(f'\\s*{scope_type}\\s+{scope_name}\\s*\\{{', mojom_text)
    if not m:
        return None
    start_index = m.end()
    brace_counter = 1
    end_index = -1
    for i in range(start_index, len(mojom_text)):
        if mojom_text[i] == '{':
            brace_counter += 1
        elif mojom_text[i] == '}':
            brace_counter -= 1
        if brace_counter == 0:
            end_index = i
            break
    if end_index == -1:
        return None

    return start_index, end_index


def _ExcludeTextWithin(scope_type: str, scope_name: str, mojom_text: str):
    boundaries = _FindScopeBoundaries(scope_type, scope_name, mojom_text)
    if boundaries:
        return mojom_text[:boundaries[0]] + mojom_text[boundaries[1]:]
    return ''


SRC_ROOT = _GetDirAbove('chrome')

def _Main():
    error = False
    # Find methods marked as checked in glic_api_client.ts.
    api_client_path = ('chrome/browser/resources/glic/glic_api_impl/'
                       'client/glic_api_client.ts')
    with open(os.path.join(SRC_ROOT, api_client_path), 'r') as f:
        client_src = f.read()

    checked_methods = set(
        m.group(1) for m in re.finditer(
            r'//\s*MOJO_RUNTIME_FEATURE_GATED\s+(\S*)', client_src))

    # Find methods gated with RuntimeFeature annotations in glic.mojom.
    mojo_file_path = 'chrome/browser/glic/host/glic.mojom'
    with open(os.path.join(SRC_ROOT, mojo_file_path), 'r') as f:
        mojom_text = f.read()
    mojom_text = _RemoveComments(mojom_text)

    for excluded_interface in ['WebClient']:
        new_mojom_text = _ExcludeTextWithin('interface', excluded_interface,
                                            mojom_text)
        if new_mojom_text:
            mojom_text = new_mojom_text
        else:
            print('Error: Could not find excluded interface '
                  f'"{excluded_interface}" in {mojo_file_path}.')
            error = True
    if error:
        sys.exit(1)

    gated_methods = set(
        m.group(1) for m in re.finditer(
            r'^\s*\[[^\]]*RuntimeFeature\s*=[^\]]*]\s*(\S+)\s*\(',
            mojom_text,
            flags=re.M | re.DOTALL))

    # Match checked and gated methods against each other.
    unchecked = gated_methods - checked_methods
    ungated = checked_methods - gated_methods
    for method in unchecked:
        decl = f'// MOJO_RUNTIME_FEATURE_GATED {method}'
        print('Error: missing feature gating code for feature'
              f' gated Mojo method {method} from {mojo_file_path}.'
              f' Please update {api_client_path} with the line:\n'
              f'  {decl}')
        error = True
    for method in ungated:
        decl = f'// MOJO_RUNTIME_FEATURE_GATED {method}'
        print(f'Error: found "{decl}" in {api_client_path},'
              ' but this method was not found or is not gated by'
              f' a RuntimeFeature in {mojo_file_path}')
        error = True

    if error:
        sys.exit(1)


if __name__ == '__main__':
    _Main()
