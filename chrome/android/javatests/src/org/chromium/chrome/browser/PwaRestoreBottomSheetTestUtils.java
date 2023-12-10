// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Static methods for use in tests to manipulate the PWA App list for restoring. */
@JNINamespace("webapps")
public class PwaRestoreBottomSheetTestUtils {
    /** Set the app list to use for testing. */
    public static void setAppListForRestoring(String[][] appList) {
        PwaRestoreBottomSheetTestUtilsJni.get().setAppListForRestoring(appList);
    }

    @NativeMethods
    interface Natives {
        void setAppListForRestoring(String[][] appList);
    }
}
