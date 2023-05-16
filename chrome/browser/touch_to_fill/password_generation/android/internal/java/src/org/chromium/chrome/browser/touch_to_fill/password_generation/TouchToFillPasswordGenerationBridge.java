// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * JNI wrapper for C++ TouchToFillPasswordGenerationBridge. Delegates calls from native to Java.
 */
class TouchToFillPasswordGenerationBridge {
    @CalledByNative
    private static TouchToFillPasswordGenerationBridge create(WindowAndroid windowAndroid) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        return new TouchToFillPasswordGenerationBridge(bottomSheetController);
    }

    private TouchToFillPasswordGenerationBridge(BottomSheetController bottomSheetController) {
        // TODO(crbug.com/1421753): create the coordinator.
    }

    @CalledByNative
    private void show() {
        // TODO(crbug.com/1421753): implement showing the password generation bottom sheet.
    }
}
