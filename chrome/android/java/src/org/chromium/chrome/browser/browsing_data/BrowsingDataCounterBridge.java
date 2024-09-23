// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

/**
 * Communicates between BrowsingDataCounter (C++ backend) and ClearBrowsingDataFragment (Java UI).
 */
public class BrowsingDataCounterBridge {
    /** Can receive a callback from a BrowsingDataCounter. */
    public interface BrowsingDataCounterCallback {
        /**
         * The callback to be called when a BrowsingDataCounter is finished.
         * @param result A string describing how much storage space will be reclaimed by clearing
         *      this data type.
         */
        public void onCounterFinished(String result);
    }

    private long mNativeBrowsingDataCounterBridge;
    private BrowsingDataCounterCallback mCallback;

    /**
     * Initializes BrowsingDataCounterBridge.
     *
     * @param profile The {@link Profile} owning the browsing data.
     * @param callback A callback to call with the result when the counter finishes.
     * @param dataType The browsing data type to be counted (from the shared enum
     * @param prefType The type of preference that should be handled (Default, Basic or Advanced
     *     from {@link org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTab}).
     */
    public BrowsingDataCounterBridge(
            Profile profile, BrowsingDataCounterCallback callback, int dataType, int prefType) {
        mCallback = callback;
        mNativeBrowsingDataCounterBridge =
                BrowsingDataCounterBridgeJni.get()
                        .init(BrowsingDataCounterBridge.this, profile, dataType, prefType);
    }

    /** Destroys the native counterpart of this class. */
    public void destroy() {
        if (mNativeBrowsingDataCounterBridge != 0) {
            BrowsingDataCounterBridgeJni.get()
                    .destroy(mNativeBrowsingDataCounterBridge, BrowsingDataCounterBridge.this);
            mNativeBrowsingDataCounterBridge = 0;
        }
    }

    @CalledByNative
    private void onBrowsingDataCounterFinished(@JniType("std::u16string") String result) {
        mCallback.onCounterFinished(result);
    }

    @NativeMethods
    interface Natives {
        long init(
                BrowsingDataCounterBridge caller,
                @JniType("Profile*") Profile profile,
                int dataType,
                int prefType);

        void destroy(long nativeBrowsingDataCounterBridge, BrowsingDataCounterBridge caller);
    }
}
