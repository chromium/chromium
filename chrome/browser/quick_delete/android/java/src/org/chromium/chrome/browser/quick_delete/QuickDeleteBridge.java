// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

/** The JNI bridge for Quick Delete on Android to fetch browsing history data. */
class QuickDeleteBridge {
    private long mNativeQuickDeleteBridge;

    /**
     * Creates a {@link QuickDeleteBridge} for accessing browsing history data for the current
     * user.
     *
     * @param profile {@link Profile} The profile for which to fetch the browsing history.
     */
    public QuickDeleteBridge(@NonNull Profile profile) {
        mNativeQuickDeleteBridge = QuickDeleteBridgeJni.get().init(QuickDeleteBridge.this, profile);
    }

    /**
     * Destroys this instance so no further calls can be executed.
     */
    public void destroy() {
        if (mNativeQuickDeleteBridge != 0) {
            QuickDeleteBridgeJni.get().destroy(mNativeQuickDeleteBridge, QuickDeleteBridge.this);
            mNativeQuickDeleteBridge = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(QuickDeleteBridge caller, Profile profile);
        void destroy(long nativeQuickDeleteBridge, QuickDeleteBridge caller);
    }
}
