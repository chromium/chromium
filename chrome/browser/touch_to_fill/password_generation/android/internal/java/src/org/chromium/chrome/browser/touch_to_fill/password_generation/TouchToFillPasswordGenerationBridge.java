// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * JNI wrapper for C++ TouchToFillPasswordGenerationBridge. Delegates calls from native to Java.
 */
class TouchToFillPasswordGenerationBridge {
    private TouchToFillPasswordGenerationCoordinator mCoordinator;

    @CalledByNative
    private static TouchToFillPasswordGenerationBridge create(WindowAndroid windowAndroid) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        Context context = windowAndroid.getContext().get();
        return new TouchToFillPasswordGenerationBridge(context, bottomSheetController);
    }

    public TouchToFillPasswordGenerationBridge(
            Context context, BottomSheetController bottomSheetController) {
        mCoordinator = new TouchToFillPasswordGenerationCoordinator(context, bottomSheetController);
    }

    @CalledByNative
    public boolean show() {
        return mCoordinator.show();
    }
}
