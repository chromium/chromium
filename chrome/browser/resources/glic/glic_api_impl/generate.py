#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Entry point for generating glic_api_generated.ts and updating glic_api.ts.
Runs the mojo parser and then the TypeScript generator.

### Special Mojom Comment Annotations

The generator parses special annotations in Mojom comments to customize the
TypeScript generation. These annotations should be placed in comments
immediately preceding the struct, enum, field, or interface definition.

#### `@generate glic_api`
Marks a struct, enum, or interface for generation.
*   **For structs/enums**: Generates intermediate `Base` transport types.
*   **For interfaces**: Generates postMessage bridges. Must be combined with
    `bridge=<BridgeName>` parameter (e.g., `@generate glic_api bridge=PocHost`).

#### `@glic_type <TypeScriptType>`
Overrides the generated TypeScript type for a field.
Useful when a Mojom type maps to a custom type or a public API type rather than
the default generated base type.
Example:
```mojom
// @glic_type glicApi.Point
gfx.mojom.Point point;
```
If the type ends with `?` (e.g., `@glic_type MyType?`), it also implies
`@glic_optional`.

#### `@glic_optional`
Marks a field as optional in the generated TS interface (appending `?`
to the field name), even if the field is not nullable in Mojom.

#### `@glic_ignore`
Ignores the field in the generated TS base interface.
"""

import os
import sys
import subprocess
import tempfile

# Add the directory containing the 'generate_impl' package to sys.path
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from generate_impl import parse


def Main():
    source_dir = parse.SOURCE_DIR
    if not source_dir:
        print("Error: Could not find source directory.", file=sys.stderr)
        sys.exit(1)

    this_dir = os.path.dirname(os.path.abspath(__file__))
    node_py = os.path.join(source_dir, 'third_party', 'node', 'node.py')
    tsc_js = os.path.join(source_dir, 'third_party', 'node', 'node_modules',
                          'typescript', 'bin', 'tsc')
    tsconfig_json = os.path.join(this_dir, 'generate_impl', 'tsconfig.json')

    with tempfile.TemporaryDirectory() as temp_dir:
        tsc_cmd = [
            sys.executable, node_py, tsc_js, '--project', tsconfig_json,
            '--outDir', temp_dir, '--noEmit', 'false'
        ]
        res = subprocess.run(tsc_cmd)
        if res.returncode != 0:
            sys.exit(res.returncode)

        gen_sources_js = os.path.join(temp_dir, 'gen_sources.js')
        node_cmd = [sys.executable, node_py, gen_sources_js] + sys.argv[1:]
        env = os.environ.copy()
        env['GENERATE_IMPL_DIR'] = os.path.join(this_dir, 'generate_impl')
        result = subprocess.run(node_cmd, env=env)
        sys.exit(result.returncode)


if __name__ == '__main__':
    Main()
