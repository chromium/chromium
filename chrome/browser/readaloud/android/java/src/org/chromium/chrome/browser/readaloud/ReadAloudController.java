// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelTabObserver;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;

/**
 * The main entrypoint component for Read Aloud feature. It's responsible for
 * checking its availability and triggering playback.
 **/
public class ReadAloudController {
    private static final String TAG = "ReadAloudController";

    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Map<String, Boolean> mReadabilityMap = new HashMap<>();
    private final Map<String, Boolean> mTimepointsSupportedMap = new HashMap<>();
    private final HashSet<String> mPendingRequests = new HashSet<>();
    private final TabModel mTabModel;
    private TabModelTabObserver mTabObserver;

    public ReadAloudController(ObservableSupplier<Profile> profileSupplier, TabModel tabModel) {
        mProfileSupplier = profileSupplier;
        mTabModel = tabModel;
    }

    /**
     * Checks if Read Aloud is supported which is true iff: user is not in the
     * incognito mode and user opted into "Make searches and browsing better".
     */
    public boolean isAvailable() {
        Profile profile = mProfileSupplier.get();
        if (profile == null || profile.isOffTheRecord()) {
            return false;
        }
        // Check whether the user has enabled anonymous URL-keyed data collection.
        // This is surfaced on the relatively new "Make searches and browsing better"
        // user setting.
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(profile);
    }

    /**
     * Returns true if the web contents within current Tab is readable.
     */
    public boolean isReadable(Tab tab) {
        if (isAvailable() && tab.getUrl().isValid()) {
            Boolean isReadable = mReadabilityMap.get(tab.getUrl().getSpec());
            return isReadable == null ? false : isReadable;
        }
        return false;
    }

    /**
     * Whether or not timepoints are supported for the tab's content.
     * Timepoints are needed for word highlighting.
     */
    public boolean timepointsSupported(Tab tab) {
        if (isAvailable() && tab.getUrl().isValid()) {
            Boolean timepointsSuported = mTimepointsSupportedMap.get(tab.getUrl().getSpec());
            return timepointsSuported == null ? false : timepointsSuported;
        }
        return false;
    }

    /** Cleanup: unregister listeners. */
    public void destroy() {
        if (mTabObserver != null) {
            mTabObserver.destroy();
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public TabModelTabObserver getTabModelTabObserver() {
        return mTabObserver;
    }
}
