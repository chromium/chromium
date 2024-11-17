# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

PRESUBMIT_VERSION = '2.0.0'


# This is mostly copied and adapted from
# tools/web_dev_style/{presubmit_support.py, js_checker.py, eslint.py}.
# We roll our own ESLint check in presubmit, since the Chromium ESLint
# check use the global eslint config, and we want to use our own (more
# strict) config.
def CheckESLint(input_api, output_api):
    should_check = lambda f: f.LocalPath().endswith(('.js', '.ts'))
    files_to_check = input_api.AffectedFiles(file_filter=should_check,
                                             include_deletes=False)
    if not files_to_check:
        return []

    files_paths = [f.AbsoluteLocalPath() for f in files_to_check]

    try:
        _RunESLint(input_api, files_paths)
    except RuntimeError as err:
        return [output_api.PresubmitError(str(err))]

    return []


def _RunESLint(input_api, args):
    os_path = input_api.os_path
    cra_path = os_path.realpath(input_api.PresubmitLocalPath())
    src_path = os_path.normpath(os_path.join(cra_path, '..', '..', '..', '..'))
    node_path = os_path.join(src_path, 'third_party', 'node')

    old_sys_path = sys.path[:]
    try:
        sys.path.append(node_path)
        import node, node_modules
    finally:
        sys.path = old_sys_path

    eslint_flat_config_key = "ESLINT_USE_FLAT_CONFIG"
    orig_flat_config_value = os.environ.get(eslint_flat_config_key, None)
    # Set ESLINT_USE_FLAT_CONFIG to true since we're using flat config, and
    # some other presubmit check still use legacy config and would set
    # ESLINT_USE_FLAT_CONFIG environment variable to false.
    os.environ[eslint_flat_config_key] = "true"
    try:
        return node.RunNode([
            node_modules.PathToEsLint(),
            '--quiet',
            '-c',
            os_path.join(cra_path, 'eslint.config.mjs'),
        ] + args)
    finally:
        if orig_flat_config_value is None:
            del os.environ[eslint_flat_config_key]
        else:
            os.environ[eslint_flat_config_key] = orig_flat_config_value
