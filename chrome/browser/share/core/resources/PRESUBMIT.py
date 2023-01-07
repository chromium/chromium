# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for Share share_targets."""

USE_PYTHON3 = True


def CheckVersionUpdatedInShareTargetList(input_api, output_api):
    # Don't report errors for "git cl presubmit --all/--files"
    if input_api.no_diffs:
        return []

    def IsShareTargetList(x):
        return (input_api.os_path.basename(
            x.LocalPath()) == 'share_targets.asciipb')

    share_targets = input_api.AffectedFiles(file_filter=IsShareTargetList)
    if not share_targets:
        return []
    for _, line in share_targets[0].ChangedContents():
        if line.strip().startswith('version_id: '):
            return []

    return [
        output_api.PresubmitError(
            'Increment |version_id| in share_targets.asciipb if you are '
            'updating the share_targets proto.')
    ]


def CheckChangeOnUpload(input_api, output_api):
    return CheckVersionUpdatedInShareTargetList(input_api, output_api)
