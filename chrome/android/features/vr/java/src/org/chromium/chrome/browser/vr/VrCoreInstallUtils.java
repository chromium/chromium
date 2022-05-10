// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * Manages logic around VrCore Installation and Versioning
 */
@JNINamespace("vr")
public class VrCoreInstallUtils {

    private long mNativeVrCoreInstallUtils;

    @CalledByNative
    @VisibleForTesting
    protected static VrCoreInstallUtils create(long nativeVrCoreInstallUtils) {
        return new VrCoreInstallUtils(nativeVrCoreInstallUtils);
    }

    /**
     * Returns the current {@VrSupportLevel}.
     */
    @CalledByNative
    public static int getVrSupportLevel() {
        return sVrSupportLevel;
    }

    @CalledByNative
    public static boolean vrSupportNeedsUpdate() {
        return false;
    }

    private VrCoreInstallUtils(long nativeVrCoreInstallUtils) {
        mNativeVrCoreInstallUtils = nativeVrCoreInstallUtils;
    }

    @CalledByNative
    private void onNativeDestroy() {
        mNativeVrCoreInstallUtils = 0;
    }

    /**
     * Prompts the user to install or update VRSupport if needed.
     */
    @CalledByNative
    @VisibleForTesting
    protected void requestInstallVrCore(final WebContents webContents) {
    }

    @NativeMethods
    interface Natives {
        void onInstallResult(long nativeVrCoreInstallHelper, boolean success);
    }
}
