// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * The JNI bridge for Quick Delete on Android to fetch browsing history data.
 * TODO(crbug.com/406230204): Update bridge name to more accurately reflect it's purpose, which is
 * to fetch History count.
 */
@NullMarked
class QuickDeleteBridge {
    private long mNativeQuickDeleteBridge;

    /** Called when the lastVisitedDomain and domainCount are fetched. */
    private final DomainVisitsCallback mCallback;

    /** Interface for a class that is fetching visited domains information. */
    public interface DomainVisitsCallback {
        /**
         * Called when the domain count and last visited domain are fetched from local history.
         *
         * @param lastVisitedDomain The synced last visited domain on all devices within the
         *     selected time period.
         * @param domainCount The number of synced unique domains visited on all devices within the
         *     selected time period.
         */
        void onLastVisitedDomainAndUniqueDomainCountReady(
                String lastVisitedDomain, int domainCount);
    }

    /**
     * Creates a {@link QuickDeleteBridge} for accessing browsing history data for the current user.
     *
     * @param profile {@link Profile} The profile for which to fetch the browsing history.
     * @param callback The callback to call with the last visited domain and domain count.
     */
    public QuickDeleteBridge(Profile profile, DomainVisitsCallback callback) {
        mNativeQuickDeleteBridge = QuickDeleteBridgeJni.get().init(this, profile);
        mCallback = callback;
    }

    /** Destroys this instance so no further calls can be executed. */
    public void destroy() {
        if (mNativeQuickDeleteBridge != 0) {
            QuickDeleteBridgeJni.get().destroy(mNativeQuickDeleteBridge);
            mNativeQuickDeleteBridge = 0;
        }
    }

    /**
     * Restarts the HistoryCounter with the selected time period.
     *
     * @param timePeriod The time period to fetch the results for.
     */
    public void restartCounterForTimePeriod(@TimePeriod int timePeriod) {
        QuickDeleteBridgeJni.get()
                .restartCounterForTimePeriod(mNativeQuickDeleteBridge, timePeriod);
    }

    @CalledByNative
    private void onLastVisitedDomainAndUniqueDomainCountReady(
            String lastVisitedDomain, int domainCount) {
        mCallback.onLastVisitedDomainAndUniqueDomainCountReady(lastVisitedDomain, domainCount);
    }

    @NativeMethods
    interface Natives {
        long init(QuickDeleteBridge self, @JniType("Profile*") @Nullable Profile profile);

        void destroy(long nativeQuickDeleteBridge);

        void restartCounterForTimePeriod(long nativeQuickDeleteBridge, int timePeriod);
    }
}
