// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.UiUtils;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridge;
import org.chromium.chrome.browser.ntp.RecentlyClosedEntry;
import org.chromium.chrome.browser.ntp.RecentlyClosedTab;
import org.chromium.chrome.browser.ntp.RecentlyClosedTabManager;
import org.chromium.chrome.browser.ntp.RecentlyClosedWindow;
import org.chromium.chrome.browser.ntp.SessionRecentlyClosedEntry;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.List;

/**
 * Manages a list of recently closed tabs or windows. Window closure events will be dictated by the
 * MultiInstanceManager, while other single / group / bulk tab closure events will be dictated by
 * the TabRestoreService. This class is responsible for synchronizing and collectively managing
 * entries from both sources.
 */
// TODO:(crbug.com/466442723): Try move RecentTabs related file to a separate package.
@NullMarked
public class RecentlyClosedEntriesManager {
    private static final int RECENTLY_CLOSED_MAX_ENTRY_COUNT = 5;
    private static final int RECENTLY_CLOSED_MAX_ENTRY_COUNT_WITH_WINDOW = 25;
    private static @Nullable RecentlyClosedTabManager sRecentlyClosedTabManagerForTests;

    private final TabModel mRegularTabModel;

    // TODO:(crbug.com/444680856) Use MultiInstanceManager to restore instance.
    @SuppressWarnings("UnusedVariable")
    private final MultiInstanceManager mMultiInstanceManager;

    private RecentlyClosedTabManager mRecentlyClosedTabManager;

    private List<RecentlyClosedEntry> mRecentlyClosedEntries = new ArrayList<>();
    private @Nullable Callback<List<RecentlyClosedEntry>> mEntriesUpdatedCallback;

    /**
     * @param multiInstanceManager The {@link MultiInstanceManager} instance used to observe window
     *     closures and restore windows.
     * @param tabModelSelector The selector that owns the Tab Model to access restored tabs.
     */
    public RecentlyClosedEntriesManager(
            MultiInstanceManager multiInstanceManager, TabModelSelector tabModelSelector) {
        mMultiInstanceManager = multiInstanceManager;
        mRegularTabModel = tabModelSelector.getModel(/* incognito= */ false);
        // TODO: Move this profile extraction logic inside RecentlyClosedTabManager.
        Profile profile = mRegularTabModel.getProfile();
        assumeNonNull(profile);
        mRecentlyClosedTabManager =
                sRecentlyClosedTabManagerForTests != null
                        ? sRecentlyClosedTabManagerForTests
                        : new RecentlyClosedBridge(profile, tabModelSelector);
        mRecentlyClosedTabManager.setEntriesUpdatedRunnable(this::updateRecentlyClosedEntries);
    }

    /**
     * @return Most up-to-date list of recently closed entries.
     */
    public List<RecentlyClosedEntry> getRecentlyClosedEntries() {
        return mRecentlyClosedEntries;
    }

    /**
     * Updates the list of recently closed entries by merging list of closed windows and closed
     * tabs/groups.
     */
    public void updateRecentlyClosedEntries() {
        List<RecentlyClosedEntry> sessionRecentlyClosedEntries =
                assumeNonNull(
                        mRecentlyClosedTabManager.getRecentlyClosedEntries(
                                getRecentlyClosedMaxEntry()));

        if (!UiUtils.isRecentlyClosedTabsAndWindowsEnabled()) {
            mRecentlyClosedEntries = sessionRecentlyClosedEntries;
        } else {
            getRecentlyClosedTabsAndWindows(sessionRecentlyClosedEntries);
        }

        if (mEntriesUpdatedCallback != null) {
            mEntriesUpdatedCallback.onResult(mRecentlyClosedEntries);
        }
    }

    /**
     * Restores a recently closed tab into the current regular tab model.
     *
     * @param tab The recently closed tab o be restored.
     * @param windowDisposition Indicating how to open the tab (new window, current tab, etc).
     */
    public void openRecentlyClosedTab(RecentlyClosedTab tab, int windowDisposition) {
        mRecentlyClosedTabManager.openRecentlyClosedTab(mRegularTabModel, tab, windowDisposition);
    }

    /**
     * Opens a recently closed entry, delegating to the {@link MultiInstanceManager} if entry is a
     * window, otherwise delegating to {@link RecentlyClosedTabManager}.
     *
     * @param entry The entry (window, bulk event, or group) to be restored.
     */
    public void openRecentlyClosedEntry(RecentlyClosedEntry entry) {
        assert entry instanceof SessionRecentlyClosedEntry;
        mRecentlyClosedTabManager.openRecentlyClosedEntry(mRegularTabModel, entry);
    }

    /**
     * Sets the callback to be invoked when the list of recently closed entries has been updated.
     *
     * @param callback The {@link Callback} that will receive the updated list of {@link
     *     RecentlyClosedEntry} objects.
     */
    public void setEntriesUpdatedCallback(Callback<List<RecentlyClosedEntry>> callback) {
        mEntriesUpdatedCallback = callback;
    }

    /** Clears the list of recently closed entris. */
    public void clearRecentlyClosedEntries() {
        // TODO(crbug.com/444681612): Add logic to close all inactive and least used windows from
        //  MultiInstanceManager.
        mRecentlyClosedTabManager.clearRecentlyClosedEntries();
    }

    public int getRecentlyClosedMaxEntry() {
        return UiUtils.isRecentlyClosedTabsAndWindowsEnabled()
                ? RECENTLY_CLOSED_MAX_ENTRY_COUNT_WITH_WINDOW
                : RECENTLY_CLOSED_MAX_ENTRY_COUNT;
    }

    /**
     * Should be called when this object is no longer needed. Performs necessary listener tear down.
     */
    @SuppressWarnings("NullAway")
    public void destroy() {
        if (mRecentlyClosedTabManager != null) {
            mRecentlyClosedTabManager.destroy();
            mRecentlyClosedTabManager = null;
        }
        mEntriesUpdatedCallback = null;
    }

    private void getRecentlyClosedTabsAndWindows(
            List<RecentlyClosedEntry> recentlyClosedSessionEntries) {
        List<RecentlyClosedWindow> recentlyClosedWindows = getRecentlyClosedWindows();
        mRecentlyClosedEntries = new ArrayList<>();

        int windowEntrySize = recentlyClosedWindows.size();
        int sessionEntrySize =
                recentlyClosedSessionEntries == null ? 0 : recentlyClosedSessionEntries.size();
        int windowCount = 0;
        int sessionEntryCount = 0;
        while (windowCount + sessionEntryCount < getRecentlyClosedMaxEntry()
                && (windowCount < windowEntrySize || sessionEntryCount < sessionEntrySize)) {
            RecentlyClosedEntry window = null;
            if (windowCount < windowEntrySize) {
                window = recentlyClosedWindows.get(windowCount);
            }

            RecentlyClosedEntry tab = null;
            if (sessionEntryCount < sessionEntrySize) {
                tab = recentlyClosedSessionEntries.get(sessionEntryCount);
            }

            assert window != null || tab != null;
            if (window == null) {
                mRecentlyClosedEntries.add(tab);
                sessionEntryCount++;
                continue;
            }

            if (tab == null) {
                mRecentlyClosedEntries.add(window);
                windowCount++;
                continue;
            }

            assumeNonNull(window);
            assumeNonNull(tab);
            // TODO(crbug.com/467412288): Decide how to resolve unavailable `tab` timestamp.
            boolean isWindowNewer = window.getDate().getTime() >= tab.getDate().getTime();
            if (isWindowNewer) {
                mRecentlyClosedEntries.add(window);
                windowCount++;
            } else {
                mRecentlyClosedEntries.add(tab);
                sessionEntryCount++;
            }
        }
        // TODO(crbug.com/444681612): Cleanup excess entries.
    }

    private List<RecentlyClosedWindow> getRecentlyClosedWindows() {
        List<InstanceInfo> instanceInfoList =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE);
        List<RecentlyClosedWindow> recentlyClosedWindows = new ArrayList<>();

        for (InstanceInfo info : instanceInfoList) {
            recentlyClosedWindows.add(
                    new RecentlyClosedWindow(
                            info.lastAccessedTime,
                            info.instanceId,
                            info.url,
                            info.customTitle,
                            info.tabCount));
        }
        return recentlyClosedWindows;
    }

    public static void setRecentlyClosedTabManagerForTests(
            @Nullable RecentlyClosedTabManager manager) {
        sRecentlyClosedTabManagerForTests = manager;
        ResettersForTesting.register(() -> sRecentlyClosedTabManagerForTests = null);
    }
}
