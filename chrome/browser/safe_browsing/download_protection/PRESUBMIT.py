# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True


def CheckTailoredWarningVersionUpdate(input_api, output_api):
    results = []

    tailored_version_file_name = 'download_request_maker.cc'
    version_variable_name = 'kTailoredWarningVersion'
    proto_path = 'components/safe_browsing/core/common/proto/csd.proto'

    has_changed_proto = proto_path in input_api.change.LocalPaths()

    def IsTailoredVersionFile(x):
        return input_api.os_path.basename(
            x.LocalPath()) == tailored_version_file_name

    tailored_version_files = input_api.AffectedFiles(
        file_filter=IsTailoredVersionFile)
    if not tailored_version_files:
        return []

    has_changed_version = False
    for _, line in tailored_version_files[0].ChangedContents():
        if version_variable_name in line.strip():
            has_changed_version = True
            break

    if has_changed_version and not has_changed_proto:
        results.append(
            output_api.PresubmitPromptWarning(
                'You changed the tailored warning info version but didn\'t ' +
                'update the description in the ' + proto_path + ' proto file.'))

    return results


def CheckChangeOnUpload(input_api, output_api):
    return CheckTailoredWarningVersionUpdate(input_api, output_api)
