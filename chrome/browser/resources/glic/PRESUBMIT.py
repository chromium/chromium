# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

# Runs PRESUBMIT.py in py3 mode by git cl presubmit.
USE_PYTHON3 = True

DEBUG = False

API_FILE = 'chrome/browser/resources/glic/glic_api/glic_api.ts'

TRIGGERING_FILE_PREFIXES = [
    'chrome/browser/resources/glic/glic_api/',
    'chrome/browser/resources/glic/presubmit/',
]


def CheckApiChangesAreBackwardsCompatible(input_api, output_api, api_file,
                                          on_upload):
    """
    Sets up a temporary directory with a copy of the glic_api.ts before
    modification, and a tsconfig.json file to build
    chrome/browser/resources/glic/presubmit/check_api_compatibility.ts.
    """
    skip_compatibility_check = (
        'Bypass-Glic-Api-Compatibility-Check'
        in input_api.change.GitFootersFromDescription())
    if skip_compatibility_check:
        return []

    src_root = input_api.os_path.join(os.getcwd(), '../../../../')
    tmp_dir = input_api.tempfile.TemporaryDirectory()
    tmp_dir_name = tmp_dir.name

    # For debugging, use a temporary directory that won't be deleted.
    if DEBUG:
        tmp_dir_name = input_api.tempfile.mkdtemp()

    # If API_FILE was modified, get its old contents. Otherwise, use its current
    # contents to confirm any modified checks still pass.
    if api_file:
        old_contents = '\n'.join(api_file.OldContents())
    else:
        with open(input_api.os_path.join(src_root, API_FILE), 'r') as f:
            old_contents = f.read()
    with open(input_api.os_path.join(tmp_dir_name, 'old_glic_api.ts'),
              'w') as oldfile:
        oldfile.write(old_contents)

    tsconfig_path = input_api.os_path.join(tmp_dir_name, 'tsconfig.json')
    with open(input_api.os_path.join(tmp_dir_name, 'tsconfig.json'),
              'w') as tsconfigfile:
        tsconfigfile.write('''{
  "extends": "$ROOT/chrome/browser/resources/glic/presubmit/tsconfig.json",
    "compilerOptions": {
      "baseUrl": "$ROOT",
      "paths": {
        "@tmp/*": ["$TMP/*"]
      }
    }
}
'''.replace("$TMP",
            tmp_dir_name.replace('\\',
                                 '/')).replace('$ROOT',
                                               src_root.replace('\\', '/')))

    message = (
        '** Your changelist is a backwards-incompatible Glic API change!\n' +
        '** Did you add a non-optional field or function, or change the\n' +
        '** type of an existing field or function?\n' +
        '** Please fix, or add Bypass-Glic-Api-Compatibility-Check: <reason>' +
        ' to your changelist description if this is intended. Error:\n  ')

    tsc_cmd = [
        input_api.python_executable,
        input_api.os_path.join(src_root, 'third_party/node/node.py'),
        input_api.os_path.join(
            src_root, 'third_party/node/node_modules/typescript/bin/tsc'),
        '--noEmit', '-p', tsconfig_path
    ]

    if DEBUG:
        print('Running', ' '.join(tsc_cmd))

    try:
        input_api.subprocess.check_output(tsc_cmd,
                                          stderr=input_api.subprocess.STDOUT)
    except input_api.subprocess.CalledProcessError as e:
        message = message + e.output.decode('utf-8')
        if on_upload:
            return [output_api.PresubmitPromptWarning(message)]
        else:
            return [output_api.PresubmitError(message)]
    return []


def CheckApiChangesAreBackwardsCompatibleIfModified(input_api, output_api,
                                                    on_upload):
    os_path = input_api.os_path
    api_file_affected = None
    need_api_check = False
    results = []
    for f in input_api.AffectedFiles():
        if any([
                os_path.normcase(f.LocalPath()).startswith(
                    os_path.normcase(prefix))
                for prefix in TRIGGERING_FILE_PREFIXES
        ]):
            need_api_check = True
        if f.LocalPath() == API_FILE:
            api_file_affected = f
            break

    if need_api_check:
        results.extend(
            CheckApiChangesAreBackwardsCompatible(input_api, output_api,
                                                  api_file_affected,
                                                  on_upload))
    return results


def _CommonChecks(input_api, output_api, on_upload):
    old_path = input_api.sys.path[:]
    try:
        input_api.sys.path.insert(0, "../../../..")
        from chrome.browser.resources.glic.common_checks import GlicCommonChecks
        return sum([
            CheckApiChangesAreBackwardsCompatibleIfModified(
                input_api, output_api, on_upload),
            GlicCommonChecks(input_api, output_api),
        ], [])
    finally:
        # Restore the original path, or other presubmits may fail.
        input_api.sys.path = old_path


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api, True)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api, False)
