# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def _CommonChecks(input_api, output_api):
    results = []
    try:
        import sys
        old_sys_path = sys.path[:]

        sys.path += [input_api.os_path.join(input_api.change.RepositoryRoot(),
                                            'ui', 'chromeos')]
        import styles.presubmit_support
        results += styles.presubmit_support._CheckSemanticColors(
            input_api, output_api)
    finally:
        sys.path = old_sys_path
    return results
