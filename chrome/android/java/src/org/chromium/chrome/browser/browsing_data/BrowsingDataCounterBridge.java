// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Communicates between BrowsingDataCounter (C++ backend) and ClearBrowsingDataFragment (Java UI).
 */
@NullMarked
public class BrowsingDataCounterBridge {
    /** Can receive a callback from a BrowsingDataCounter. */
    public interface BrowsingDataCounterCallback {
        /**
         * The callback to be called when a BrowsingDataCounter is finished and an update to the
         * summary is required.
         *
         * @param result For example, a string describing how much storage space will be reclaimed
         *     by clearing this data type.
         */
        void onCounterFinished(String summary);
    }

    private long mNativeBrowsingDataCounterBridge;
    private final BrowsingDataCounterCallback mCallback;

    /**
     * Initializes BrowsingDataCounterBridge.
     *
     * @param profile The {@link Profile} owning the browsing data.
     * @param callback A callback to call with the result when the counter finishes.
     * @param selectedTimePeriod The time period selected in the UI.
     * @param dataType The browsing data type to be counted (from the shared enum
     */
    public BrowsingDataCounterBridge(
            Profile profile,
            BrowsingDataCounterCallback callback,
            @TimePeriod int selectedTimePeriod,
            int dataType) {
        mCallback = callback;
        mNativeBrowsingDataCounterBridge =
                BrowsingDataCounterBridgeJni.get()
                        .initWithoutPeriodPref(this, profile, selectedTimePeriod, dataType);
    }

    public void setSelectedTimePeriod(@TimePeriod int selectedTimePeriod) {
        if (mNativeBrowsingDataCounterBridge != 0) {
            BrowsingDataCounterBridgeJni.get()
                    .setSelectedTimePeriod(mNativeBrowsingDataCounterBridge, selectedTimePeriod);
        }
    }

    /** Destroys the native counterpart of this class. */
    public void destroy() {
        if (mNativeBrowsingDataCounterBridge != 0) {
            BrowsingDataCounterBridgeJni.get().destroy(mNativeBrowsingDataCounterBridge);
            mNativeBrowsingDataCounterBridge = 0;
        }
    }

    @CalledByNative
    private void onBrowsingDataCounterFinished(@JniType("std::u16string") String summary) {
        mCallback.onCounterFinished(summary);
    }

    @NativeMethods
    interface Natives {
        long initWithoutPeriodPref(
                BrowsingDataCounterBridge self,
                @JniType("Profile*") Profile profile,
                int selectedTimePeriod,
                int dataType);

        void setSelectedTimePeriod(long nativeBrowsingDataCounterBridge, int selectedTimePeriod);

        void destroy(long nativeBrowsingDataCounterBridge);
    }
}
