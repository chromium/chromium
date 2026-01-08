// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.util.ArraySet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.InstanceStateObserver;
import org.chromium.chrome.browser.ntp.RecentlyClosedWindow;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.Set;

/** Implements {@link RecentlyClosedEntriesManagerTracker} as a singleton. */
@NullMarked
final class RecentlyClosedEntriesManagerTrackerImpl
        implements RecentlyClosedEntriesManagerTracker, InstanceStateObserver {
    private static @Nullable RecentlyClosedEntriesManagerTrackerImpl sInstance;

    private final Set<RecentlyClosedEntriesManager> mManagers = new ArraySet<>();

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
                new RecentlyClosedEntriesManager(multiInstanceManager, tabModelSelector, this);
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
    public void onInstanceClosed(InstanceInfo instanceInfo, boolean isPermanentDeletion) {
        RecentlyClosedWindow window =
                new RecentlyClosedWindow(
                        instanceInfo.lastAccessedTime,
                        instanceInfo.instanceId,
                        instanceInfo.url,
                        instanceInfo.customTitle,
                        instanceInfo.title,
                        instanceInfo.tabCount);
        for (RecentlyClosedEntriesManager manager : mManagers) {
            manager.onWindowClosed(window, isPermanentDeletion);
        }
    }

    public Set<RecentlyClosedEntriesManager> getManagersForTesting() {
        return mManagers;
    }
}
