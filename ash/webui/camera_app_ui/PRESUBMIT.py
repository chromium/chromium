# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)


BASE_DIRECTORY = 'ash/webui/camera_app_ui/'
STRING_RESOURCE_FILES = [
    os.path.join(BASE_DIRECTORY, f) for f in [
        'resources.h',
        'resources/js/i18n_string.ts',
        'resources/strings/camera_strings.grd',
    ]
]


def _CommonChecks(input_api, output_api):
    results = []
    affected = input_api.AffectedFiles()
    if any(f for f in affected if f.LocalPath().endswith('.html')):
        results += _CheckHtml(input_api, output_api)
    if any(f for f in affected if f.LocalPath() in STRING_RESOURCE_FILES):
        results += _CheckStringResouce(input_api, output_api)
    if any(f for f in affected if f.LocalPath().endswith('.css')
           or f.LocalPath().endswith('.svg')):
        results += _CheckColorTokens(input_api, output_api)
    if any(f for f in affected if f.LocalPath().endswith('metrics.ts')):
        results += _CheckModifyMetrics(input_api, output_api)
    results += _CheckESLint(input_api, output_api)

    return results


def _CheckHtml(input_api, output_api):
    return input_api.canned_checks.CheckLongLines(
        input_api, output_api, 80, lambda x: x.LocalPath().endswith('.html'))


def _CheckStringResouce(input_api, output_api):
    rv = input_api.subprocess.call([
        input_api.python3_executable, './resources/utils/cca.py',
        'check-strings'
    ])

    if rv:
        return [
            output_api.PresubmitPromptWarning(
                'String resources check failed, ' +
                'please make sure the relevant string files are all modified.')
        ]

    return []


def _CheckColorTokens(input_api, output_api):
    rv = input_api.subprocess.call([
        input_api.python3_executable, './resources/utils/cca.py',
        'check-color-tokens'
    ])

    if rv:
        return [
            output_api.PresubmitPromptWarning(
                'Color token check failed, ' +
                'please only use new dynamic color tokens in new CSS rules.')
        ]

    return []


def _CheckModifyMetrics(input_api, output_api):
    if input_api.no_diffs or input_api.change.METRICS_DOCUMENTATION_UPDATED:
        return []
    return [
        output_api.PresubmitPromptWarning(
            'Metrics are modified but `METRICS_DOCUMENTATION_UPDATED=true` is '
            + 'not found in the commit messages.\n' +
            'The CL author should confirm CCA metrics are still synced in ' +
            'PDD (go/cca-metrics-pdd) and Schema (go/cca-metrics-schema).\n' +
            'Once done, the CL author should explicitly claim it by including '
            + '`METRICS_DOCUMENTATION_UPDATED=true` in the commit messages.')
    ]


# This is mostly copied and adapted from
# tools/web_dev_style/{presubmit_support.py, js_checker.py, eslint.py}.
# We roll our own ESLint check in presubmit, since the Chromium ESLint
# check use the global eslint config, and we want to use our own (more
# strict) config.
def _CheckESLint(input_api, output_api):
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
    cca_path = os_path.realpath(input_api.PresubmitLocalPath())
    src_path = os_path.normpath(os_path.join(cca_path, '..', '..', '..'))
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
            os_path.join(cca_path, 'resources/eslint.config.mjs'),
        ] + args)
    finally:
        if orig_flat_config_value is None:
            del os.environ[eslint_flat_config_key]
        else:
            os.environ[eslint_flat_config_key] = orig_flat_config_value
