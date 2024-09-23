// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import android.util.ArraySet;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;

import java.util.Set;

/**
 * Tracks navigations on that were started from sync. These navigations will be ignored to prevent
 * looping notifications back to sync.
 */
public class NavigationTracker {
    /** A counter to generate unique IDs to track navigations within a chrome session. */
    private int mRequestIdCounter;

    /**
     * The navigations that have been started but yet to complete. When completed, they will be
     * cleared from this set.
     */
    private final Set<Integer> mPendingNavigations = new ArraySet<>();

    /**
     * Starts tracking a navigation.
     *
     * @param host The {@link UserDataHost} associated with this navigation.
     */
    public void setNavigationWasFromSync(UserDataHost host) {
        int nextRequestId = mRequestIdCounter++;
        host.setUserData(SyncNavigationUserData.class, new SyncNavigationUserData(nextRequestId));
        mPendingNavigations.add(nextRequestId);
    }

    /**
     * Checks if a navigation was started from sync. If it was started from sync, the function
     * returns true, and removes the navigation from the tracking set.
     *
     * @param host The {@link UserDataHost} associated with the navigation.
     * @return True if this navigation was started from sync, false otherwise.
     */
    public boolean wasNavigationFromSync(UserDataHost host) {
        SyncNavigationUserData data = host.getUserData(SyncNavigationUserData.class);
        if (data == null) return false;
        boolean isFromSync = mPendingNavigations.contains(data.mRequestID);
        mPendingNavigations.remove(data.mRequestID);
        return isFromSync;
    }

    /**
     * Helper class to attach a request ID along with the navigation so that it can be identified
     * later.
     */
    private static class SyncNavigationUserData implements UserData {
        /** An ID that can be used to track the navigation. */
        private int mRequestID;

        /** Constructor. */
        public SyncNavigationUserData(int requestID) {
            this.mRequestID = requestID;
        }
    }
}
