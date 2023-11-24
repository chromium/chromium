// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;

/** A class that tracks the Home surface NTP. */
public class HomeSurfaceTracker {
    @Nullable private Tab mHomeSurfaceTab;
    @Nullable private Tab mLastActiveTabToTrack;

    public HomeSurfaceTracker() {}

    /**
     * Updates the Tab of the home surface NTP and the last active Tab it tracks.
     * @param homeSurfaceTab The Tab of the home surface NTP
     * @param tabToTrack The last active Tab that the home surface NTP tracks.
     */
    public void updateHomeSurfaceAndTrackingTabs(Tab homeSurfaceTab, Tab tabToTrack) {
        mHomeSurfaceTab = homeSurfaceTab;
        mLastActiveTabToTrack = tabToTrack;
    }

    /**
     * Gets the last active Tab which the home surface NTP tracks. Returns null if this Tab was
     * deleted or the home surface NTP didn't track any Tab.
     */
    public Tab getLastActiveTabToTrack() {
        if (mLastActiveTabToTrack == null) return null;

        if (mLastActiveTabToTrack.isDestroyed() || mLastActiveTabToTrack.isClosing()) {
            mLastActiveTabToTrack = null;
        }

        return mLastActiveTabToTrack;
    }

    /** Returns whether the given Tab is the Tab of home surface NTP. */
    public boolean isHomeSurfaceTab(Tab tab) {
        if (tab == null || mHomeSurfaceTab == null) return false;

        return mHomeSurfaceTab == tab;
    }

    /**
     * Returns whether a home surface UI can be shown on the given Tab. This requires:
     * 1) The given Tab is a NTP;
     * 2) it is the current Home surface NTP saved in the HomeSurfaceTracker;
     * 3) it has a valid last active Tab to track.
     */
    public boolean canShowHomeSurface(Tab ntpTab) {
        return isHomeSurfaceTab(ntpTab) && getLastActiveTabToTrack() != null;
    }

    public Tab getHomeSurfaceTabForTesting() {
        return mHomeSurfaceTab;
    }

    public Tab getLastActiveTabToTrackForTesting() {
        return mLastActiveTabToTrack;
    }
}
