#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Reads glic.mojom and other referenced mojom files, and outputs generated code to
glic_api.ts.

Translates enums, structs, and unions with a "// @generate glic_api" comment
above them and copies them into glic_api.ts. Comments from mojom files are
copied over, but are ignored if they have a '///' prefix, allowing for internal
documentation to be filtered out.

Supports the following annotations on mojom fields:
- "@glic_type <type>": Overrides the generated TypeScript type.
- "@glic_optional": Marks a property as optional (appending '?').
- "@glic_ignore": Excludes a property from the generated interface.

Ideally, we would output generated code to a new file, but this way is
less likely to break downstream users of glic_api.
"""
import os
import sys

# Add the directory containing the 'generate_impl' package to sys.path
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import generate_impl.gen_sources


def Main():
    generate_impl.gen_sources.Main()


if __name__ == '__main__':
    Main()
