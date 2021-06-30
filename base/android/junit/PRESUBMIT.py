# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True


def CheckChangeOnUpload(input_api, output_api):
    results = []

    # Dictionary of base/android files with corresponding Robolectric shadows.
    # If new functions are added to the original file, it is very likely that
    # function with the same signature should be added to the shadow.
    impl_to_shadow_paths = {
        'base/android/java/src/org/'
        'chromium/base/metrics/RecordHistogram.java':
            'base/android/junit/src/org/'
            'chromium/base/metrics/test/ShadowRecordHistogram.java'
    }

    for impl_path, shadow_path in impl_to_shadow_paths.items():
        if impl_path in input_api.change.LocalPaths():
            if shadow_path not in input_api.change.LocalPaths():
                results.append(
                    output_api.PresubmitPromptWarning(
                        'You modified the runtime class:\n  {}\n'
                        'without changing the corresponding shadow test class'
                        ':\n  {}\n').format(impl_path, shadow_path))

    return results
