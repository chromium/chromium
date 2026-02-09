// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.util.ArraySet;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.ntp.RecentlyClosedWindow;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Implements {@link RecentlyClosedEntriesManagerTracker} as a singleton. */
@NullMarked
public final class RecentlyClosedEntriesManagerTrackerImpl
        implements RecentlyClosedEntriesManagerTracker {
    private static @Nullable RecentlyClosedEntriesManagerTrackerImpl sInstance;

    private final Set<RecentlyClosedEntriesManager> mManagers = new ArraySet<>();
    private boolean mOpenMostRecentTabEntryNext;

    static RecentlyClosedEntriesManagerTrackerImpl getInstance() {
        if (sInstance == null) {
            sInstance = new RecentlyClosedEntriesManagerTrackerImpl();
        }
        return sInstance;
    }

    @Override
    public RecentlyClosedEntriesManager obtainManager(
            MultiInstanceManager multiInstanceManager, TabModelSelector tabModelSelector) {
        RecentlyClosedEntriesManager manager =
                new RecentlyClosedEntriesManager(multiInstanceManager, tabModelSelector);
        assert !mManagers.contains(manager)
                : "A RecentlyClosedEntriesManager for this activity already exists.";
        mManagers.add(manager);
        return manager;
    }

    @Override
    public void destroy(RecentlyClosedEntriesManager manager) {
        mManagers.remove(manager);
        manager.destroy();
    }

    @Override
    public void onInstancesClosed(
            List<InstanceInfo> instanceInfoList, boolean isPermanentDeletion) {
        List<RecentlyClosedWindow> windows = new ArrayList<>();
        for (InstanceInfo instanceInfo : instanceInfoList) {
            // Use lastAccessedTime as the closure time if a valid closureTime is not available.
            long closureTime =
                    instanceInfo.closureTime > 0
                            ? instanceInfo.closureTime
                            : instanceInfo.lastAccessedTime;
            assert isPermanentDeletion || closureTime > 0 : "Expected a valid window closure time.";
            RecentlyClosedWindow window =
                    new RecentlyClosedWindow(
                            closureTime,
                            instanceInfo.instanceId,
                            instanceInfo.url,
                            instanceInfo.customTitle,
                            instanceInfo.title,
                            instanceInfo.tabCount);
            windows.add(window);
        }
        for (RecentlyClosedEntriesManager manager : mManagers) {
            manager.onWindowsClosed(windows, isPermanentDeletion);
        }
    }

    @Override
    public void onInstanceRestored(int instanceId) {
        for (RecentlyClosedEntriesManager manager : mManagers) {
            manager.onWindowRestored(instanceId);
        }
    }

    @VisibleForTesting
    public void setOpenMostRecentTabEntryNext(boolean openMostRecentTab) {
        mOpenMostRecentTabEntryNext = openMostRecentTab;
    }

    /* package */ boolean shouldOpenMostRecentTabEntryNext() {
        return mOpenMostRecentTabEntryNext;
    }

    /* package */ Set<RecentlyClosedEntriesManager> getManagers() {
        return mManagers;
    }
}
