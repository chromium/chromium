// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/** The JNI bridge for Quick Delete on Android to fetch browsing history data. */
class QuickDeleteBridge {
    private long mNativeQuickDeleteBridge;

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
     * Creates a {@link QuickDeleteBridge} for accessing browsing history data for the current
     * user.
     *
     * @param profile {@link Profile} The profile for which to fetch the browsing history.
     */
    public QuickDeleteBridge(@NonNull Profile profile) {
        mNativeQuickDeleteBridge = QuickDeleteBridgeJni.get().init(QuickDeleteBridge.this, profile);
    }

    /** Destroys this instance so no further calls can be executed. */
    public void destroy() {
        if (mNativeQuickDeleteBridge != 0) {
            QuickDeleteBridgeJni.get().destroy(mNativeQuickDeleteBridge, QuickDeleteBridge.this);
            mNativeQuickDeleteBridge = 0;
        }
    }

    /**
     * Gets the synced last visited domain and unique domain count on all devices within the time
     * period.
     *
     * @param timePeriod The time period to fetch the results for.
     * @param callback The callback to call with the last visited domain and domain count.
     */
    public void getLastVisitedDomainAndUniqueDomainCount(
            @TimePeriod int timePeriod, @NonNull DomainVisitsCallback callback) {
        QuickDeleteBridgeJni.get()
                .getLastVisitedDomainAndUniqueDomainCount(
                        mNativeQuickDeleteBridge, timePeriod, callback);
    }

    /**
     * Attempt to trigger the HaTS survey if appropriate.
     *
     * @param webContents web contents of the tab to trigger the survey on.
     */
    public void showSurvey(WebContents webContents) {
        QuickDeleteBridgeJni.get().showSurvey(mNativeQuickDeleteBridge, webContents);
    }

    @CalledByNative
    private static void onLastVisitedDomainAndUniqueDomainCountReady(
            DomainVisitsCallback callback, String lastVisitedDomain, int domainCount) {
        callback.onLastVisitedDomainAndUniqueDomainCountReady(lastVisitedDomain, domainCount);
    }

    @NativeMethods
    interface Natives {
        long init(QuickDeleteBridge caller, @JniType("Profile*") Profile profile);

        void destroy(long nativeQuickDeleteBridge, QuickDeleteBridge caller);

        void getLastVisitedDomainAndUniqueDomainCount(
                long nativeQuickDeleteBridge, int timePeriod, DomainVisitsCallback callback);

        void showSurvey(long nativeQuickDeleteBridge, WebContents webContents);
    }
}
