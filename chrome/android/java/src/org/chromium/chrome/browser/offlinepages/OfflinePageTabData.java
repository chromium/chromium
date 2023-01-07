// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import androidx.annotation.NonNull;

import org.chromium.base.UserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.Tab;

/**
 * UserData object that holds the OfflinePage visibility status for the associated tab. This can be
 * used to check the offline status of a tab without calling into native.
 */
public class OfflinePageTabData implements UserData {
    private static final Class<OfflinePageTabData> USER_DATA_KEY = OfflinePageTabData.class;
    private static final double SAMPLE_PROBABILITY = 0.01;

    /**
     * Returns whether the given tab is showing an offline page. Returns {@code false} for a {@code
     * null} or un-initialized tab.
     */
    public static boolean isShowingOfflinePage(Tab tab) {
        if (tab == null || !tab.isInitialized()) return false;
        OfflinePageTabData offlinePageTabData = get(tab);
        if (offlinePageTabData == null) return false;
        boolean result = offlinePageTabData.isTabShowingOfflinePage();
        // Check to see if the cached result is correct, but only once every 100 calls.
        if (Math.random() < SAMPLE_PROBABILITY) {
            boolean freshResult = OfflinePageUtils.isOfflinePage(tab);
            RecordHistogram.recordBooleanHistogram(
                    "OfflinePages.CachedOfflineStatusValid", freshResult == result);
            result = freshResult;
        }

        return result;
    }

    /**
     * Returns whether the given tab is showing a trusted offline page. Returns {@code false} for a
     * {@code null} or un-initialized tab.
     */
    public static boolean isShowingTrustedOfflinePage(Tab tab) {
        if (tab == null || !tab.isInitialized()) return false;
        OfflinePageTabData offlinePageTabData = get(tab);
        return offlinePageTabData != null && offlinePageTabData.isTabShowingTrustedOfflinePage();
    }

    /**
     * Returns the OfflinePageTabData instance for the given Tab object, creating it if does not yet
     * exist. This can never return {@code null}, but is not safe to call if the tab has been
     * destroyed.
     */
    static OfflinePageTabData from(@NonNull Tab tab) {
        assert tab.isInitialized();
        OfflinePageTabData offlinePageTabData = get(tab);
        if (offlinePageTabData == null) {
            offlinePageTabData =
                    tab.getUserDataHost().setUserData(USER_DATA_KEY, new OfflinePageTabData());
        }
        return offlinePageTabData;
    }

    private static OfflinePageTabData get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private boolean mIsTabShowingOfflinePage;
    private boolean mIsTabShowingTrustedOfflinePage;

    OfflinePageTabData() {}

    public boolean isTabShowingOfflinePage() {
        return mIsTabShowingOfflinePage;
    }

    public boolean isTabShowingTrustedOfflinePage() {
        return mIsTabShowingTrustedOfflinePage;
    }

    void setIsTabShowingOfflinePage(boolean isTabShowingOfflinePage) {
        mIsTabShowingOfflinePage = isTabShowingOfflinePage;
    }

    void setIsTabShowingTrustedOfflinePage(boolean isTabShowingTrustedOfflinePage) {
        mIsTabShowingTrustedOfflinePage = isTabShowingTrustedOfflinePage;
    }
}
