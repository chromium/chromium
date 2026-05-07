# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Checks that public manifest fields are matching the internal Chrome-branded
manifest fields except for externally_connectable.
"""

import json
import os
import sys


def _GetDirAbove(dirname: str):
    path = os.path.abspath(__file__)
    while True:
        path, tail = os.path.split(path)
        if not tail:
            return None
        if tail == dirname:
            return path


SRC_ROOT = _GetDirAbove('chrome')


def _LoadManifest(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    clean_lines = []
    for line in content.splitlines():
        stripped = line.strip()
        if stripped.startswith('//'):
            continue
        clean_lines.append(line)
    return json.loads('\n'.join(clean_lines))


def main(manifest_path=None, internal_manifest_path=None):
    manifest_rel = 'chrome/browser/resources/glic/extension/manifest.json'
    internal_manifest_rel = 'internal/extensions/glic/manifest_internal.json'

    if not manifest_path:
        manifest_path = os.path.join(SRC_ROOT, manifest_rel)
    if not internal_manifest_path:
        internal_manifest_path = os.path.join(SRC_ROOT, internal_manifest_rel)

    if not os.path.exists(manifest_path):
        print(f"Error: Public manifest file does not exist at {manifest_path}")
        return 1

    if not os.path.exists(internal_manifest_path):
        # Gracefully skip if no internal checkout is present.
        return 0

    try:
        manifest = _LoadManifest(manifest_path)
        internal_manifest = _LoadManifest(internal_manifest_path)
    except Exception as e:
        print(f"Error: Failed to parse extension manifest JSON files: {e}")
        return 1

    manifest.pop('externally_connectable', None)
    internal_manifest.pop('externally_connectable', None)

    if manifest == internal_manifest:
        return 0

    errors = []
    keys_manifest = set(manifest.keys())
    keys_internal = set(internal_manifest.keys())

    if keys_manifest != keys_internal:
        only_manifest = keys_manifest - keys_internal
        only_internal = keys_internal - keys_manifest
        if only_manifest:
            errors.append(f"Fields found in public manifest only: "
                          f"{', '.join(only_manifest)}")
        if only_internal:
            errors.append(f"Fields found in internal manifest only: "
                          f"{', '.join(only_internal)}")

    for key in keys_manifest & keys_internal:
        if manifest[key] != internal_manifest[key]:
            errors.append(f"Field '{key}' differs:\n"
                          f"Public ({manifest_rel}):\n  {manifest[key]}\n"
                          f"Internal ({internal_manifest_rel}):\n"
                          f"  {internal_manifest[key]}")

    if errors:
        print(f"Error: manifest.json and manifest_internal.json"
              f" do not match except externally_connectable.\n" +
              '\n'.join(errors))
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
