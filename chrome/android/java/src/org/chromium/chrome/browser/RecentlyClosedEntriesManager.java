// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.CloseWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
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
import java.util.Collections;
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
    private static @Nullable Integer sMaxEntriesForTests;

    private final TabModel mRegularTabModel;

    private final MultiInstanceManager mMultiInstanceManager;

    private RecentlyClosedTabManager mRecentlyClosedTabManager;

    private List<RecentlyClosedEntry> mRecentlyClosedEntries = new ArrayList<>();
    private @Nullable Callback<List<RecentlyClosedEntry>> mEntriesUpdatedCallback;

    /**
     * @param multiInstanceManager The {@link MultiInstanceManager} instance used to observe window
     *     closures and restore windows.
     * @param tabModelSelector The selector that owns the Tab Model to access restored tabs.
     */
    /* package */ RecentlyClosedEntriesManager(
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
        if (entry instanceof SessionRecentlyClosedEntry) {
            mRecentlyClosedTabManager.openRecentlyClosedEntry(mRegularTabModel, entry);
        } else if (entry instanceof RecentlyClosedWindow closedWindow) {
            if (canRestoreWindow()) {
                mMultiInstanceManager.openWindow(
                        closedWindow.getInstanceId(), NewWindowAppSource.RECENT_TABS);
            } else {
                mMultiInstanceManager.showInstanceCreationLimitMessage();
            }
        }
    }

    /**
     * Opens the most recently closed entry. If the entry is a {@link RecentlyClosedWindow} and max
     * number of instances already exists, this will restore the next most recently closed
     * non-window entry if it exists.
     *
     * <p>Note that we will not use {@link #getRecentlyClosedEntries()} to determine the most
     * recently closed entry. This is because we need to consider pending tab closures in addition
     * to closures managed by TabRestoreService and MultiInstanceManager.
     *
     * @param newWindowSource The {@link NewWindowAppSource} to track the source of window
     *     restoration, used for metrics.
     */
    public void openMostRecentlyClosedEntry(@NewWindowAppSource int newWindowSource) {
        if (!UiUtils.isRecentlyClosedTabsAndWindowsEnabled()) {
            mRegularTabModel.openMostRecentlyClosedEntry();
            return;
        }

        RecentlyClosedEntriesManagerTrackerImpl tracker =
                RecentlyClosedEntriesManagerTrackerImpl.getInstance();
        if (tracker.shouldOpenMostRecentTabEntryNext()) {
            tracker.setOpenMostRecentTabEntryNext(false);
            mRegularTabModel.openMostRecentlyClosedEntry();
            return;
        }

        long mostRecentTabClosureTime = mRegularTabModel.getMostRecentClosureTime();
        List<RecentlyClosedWindow> recentlyClosedWindows = getRecentlyClosedWindows();

        boolean closedWindowExists = !recentlyClosedWindows.isEmpty();
        boolean closedTabEventExists = mostRecentTabClosureTime != TabModel.INVALID_TIMESTAMP;

        // Nothing to restore.
        if (!closedWindowExists && !closedTabEventExists) return;

        // Tab and window entries are both available for restoration.
        if (closedWindowExists && closedTabEventExists) {
            RecentlyClosedWindow mostRecentlyClosedWindow = recentlyClosedWindows.get(0);
            if (mostRecentlyClosedWindow.getDate().getTime() >= mostRecentTabClosureTime
                    && canRestoreWindow()) {
                mMultiInstanceManager.openWindow(
                        mostRecentlyClosedWindow.getInstanceId(), newWindowSource);
                if (mostRecentTabClosureTime == 0) {
                    // Mark tab event with timestamp 0 as next most recent entry to open.
                    tracker.setOpenMostRecentTabEntryNext(true);
                }
            } else {
                mRegularTabModel.openMostRecentlyClosedEntry();
            }
            return;
        }

        // Only window entries are available for restoration.
        if (closedWindowExists && canRestoreWindow()) {
            RecentlyClosedWindow mostRecentlyClosedWindow = recentlyClosedWindows.get(0);
            mMultiInstanceManager.openWindow(
                    mostRecentlyClosedWindow.getInstanceId(), newWindowSource);
            return;
        }

        // Only tab entries are available for restoration.
        mRegularTabModel.openMostRecentlyClosedEntry();
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

    /** Clears the list of recently closed entries. */
    public void clearRecentlyClosedEntries() {
        List<InstanceInfo> instanceInfoList = getAllInactiveInstances();
        List<Integer> instanceIds = new ArrayList<>();
        for (InstanceInfo instanceInfo : instanceInfoList) {
            instanceIds.add(instanceInfo.instanceId);
        }
        mMultiInstanceManager.closeWindows(instanceIds, CloseWindowAppSource.RECENT_TABS);
        mRecentlyClosedTabManager.clearRecentlyClosedEntries();
    }

    @VisibleForTesting
    public int getRecentlyClosedMaxEntry() {
        if (sMaxEntriesForTests != null) {
            return sMaxEntriesForTests;
        }
        return UiUtils.isRecentlyClosedTabsAndWindowsEnabled()
                ? RECENTLY_CLOSED_MAX_ENTRY_COUNT_WITH_WINDOW
                : RECENTLY_CLOSED_MAX_ENTRY_COUNT;
    }

    /**
     * Notifies relevant listeners (for e.g. Recent Tabs page) when a window is closed.
     *
     * @param window The window that was just closed.
     * @param isPermanentDeletion Whether the window is permanently deleted. If {@code false}, the
     *     window will be added as the most recently closed entry.
     */
    @VisibleForTesting
    public void onWindowClosed(RecentlyClosedWindow window, boolean isPermanentDeletion) {
        // First, remove the entry from the current position in the list if it exists.
        removeWindowEntry(window.getInstanceId());

        // If an inactive window was explicitly closed by the user, add it to the top of the list.
        if (!isPermanentDeletion) {
            mRecentlyClosedEntries.add(0, window);
        }

        // Remove the excess entry from the list, and clean up the storage.
        int size = mRecentlyClosedEntries.size();
        if (size > RECENTLY_CLOSED_MAX_ENTRY_COUNT_WITH_WINDOW) {
            RecentlyClosedEntry excessEntry = mRecentlyClosedEntries.remove(size - 1);
            if (excessEntry instanceof SessionRecentlyClosedEntry) {
                mRecentlyClosedTabManager.clearLeastRecentlyUsedClosedEntries(/* numToRemove= */ 1);
            } else if (excessEntry instanceof RecentlyClosedWindow excessWindow) {
                mMultiInstanceManager.closeWindows(
                        Collections.singletonList(excessWindow.getInstanceId()),
                        CloseWindowAppSource.RECENT_TABS);
            }
        }
        assert mRecentlyClosedEntries.size() <= RECENTLY_CLOSED_MAX_ENTRY_COUNT_WITH_WINDOW;

        if (mEntriesUpdatedCallback != null) {
            mEntriesUpdatedCallback.onResult(mRecentlyClosedEntries);
        }
    }

    /**
     * Notifies relevant listeners (for e.g. Recent Tabs page) when a window is restored.
     *
     * @param instanceId The id of the instance that was restored.
     */
    @VisibleForTesting
    public void onWindowRestored(int instanceId) {
        removeWindowEntry(instanceId);
        if (mEntriesUpdatedCallback != null) {
            mEntriesUpdatedCallback.onResult(mRecentlyClosedEntries);
        }
    }

    private void removeWindowEntry(int instanceId) {
        for (RecentlyClosedEntry entry : mRecentlyClosedEntries) {
            if (entry instanceof RecentlyClosedWindow window
                    && window.getInstanceId() == instanceId) {
                mRecentlyClosedEntries.remove(entry);
                return;
            }
        }
    }

    /**
     * Should be called when this object is no longer needed. Performs necessary listener tear down.
     */
    @SuppressWarnings("NullAway")
    /* package */ void destroy() {
        if (mRecentlyClosedTabManager != null) {
            mRecentlyClosedTabManager.destroy();
            mRecentlyClosedTabManager = null;
        }

        mEntriesUpdatedCallback = null;
    }

    private List<InstanceInfo> getAllInactiveInstances() {
        return mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE);
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

            long t1 = window.getDate().getTime();
            long t2 = tab.getDate().getTime();
            boolean isWindowNewer = t2 > 0 && t1 >= t2;
            if (isWindowNewer) {
                // Window is more recently closed than tab entry with a valid timestamp.
                mRecentlyClosedEntries.add(window);
                windowCount++;
            } else if (t2 > 0) {
                // Tab entry with a valid timestamp is more recently closed than the window.
                mRecentlyClosedEntries.add(tab);
                sessionEntryCount++;
            } else {
                // Tab entry timestamp is 0. Examine next tab entry.
                if (sessionEntryCount + 1 < sessionEntrySize) {
                    RecentlyClosedEntry nextTab =
                            recentlyClosedSessionEntries.get(sessionEntryCount + 1);

                    long t3 = nextTab.getDate().getTime();
                    boolean isNextTabNewer = t3 > 0 && t3 >= t1;
                    if (isNextTabNewer) {
                        // Prioritize tab entry with timestamp = 0, since next tab entry is more
                        // recent than the most recently closed window.
                        mRecentlyClosedEntries.add(tab);
                        sessionEntryCount++;
                        continue;
                    }
                }

                // If the next tab is not available OR is not more recently closed than the most
                // recently closed window OR has a timestamp of 0 then add both the most recently
                // closed window and the tab entry with timestamp 0.
                mRecentlyClosedEntries.add(window);
                windowCount++;
                if (windowCount + sessionEntryCount < getRecentlyClosedMaxEntry()) {
                    // Add tab entry with timestamp = 0 if within limit.
                    mRecentlyClosedEntries.add(tab);
                    sessionEntryCount++;
                }
            }
        }

        // Clean up the excess least recently used entries.
        if (windowCount < windowEntrySize) {
            List<Integer> instanceIdsToClose = new ArrayList<>();
            for (int i = windowCount; i < recentlyClosedWindows.size(); i++) {
                instanceIdsToClose.add(recentlyClosedWindows.get(i).getInstanceId());
            }
            mMultiInstanceManager.closeWindows(
                    instanceIdsToClose, CloseWindowAppSource.RECENT_TABS);
        }
        if (sessionEntryCount < sessionEntrySize) {
            mRecentlyClosedTabManager.clearLeastRecentlyUsedClosedEntries(
                    /* numToRemove= */ sessionEntrySize - sessionEntryCount);
        }
    }

    private List<RecentlyClosedWindow> getRecentlyClosedWindows() {
        List<InstanceInfo> instanceInfoList = getAllInactiveInstances();
        List<RecentlyClosedWindow> recentlyClosedWindows = new ArrayList<>();

        for (InstanceInfo info : instanceInfoList) {
            recentlyClosedWindows.add(
                    new RecentlyClosedWindow(
                            info.lastAccessedTime,
                            info.instanceId,
                            info.url,
                            info.customTitle,
                            info.title,
                            info.tabCount));
        }
        return recentlyClosedWindows;
    }

    private boolean canRestoreWindow() {
        int instanceCount =
                MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ACTIVE);
        int instanceLimit = MultiWindowUtils.getMaxInstances();
        return instanceCount < instanceLimit;
    }

    public static void setRecentlyClosedTabManagerForTests(
            @Nullable RecentlyClosedTabManager manager) {
        sRecentlyClosedTabManagerForTests = manager;
        ResettersForTesting.register(() -> sRecentlyClosedTabManagerForTests = null);
    }

    public static void setMaxEntriesForTests(int maxEntries) {
        sMaxEntriesForTests = maxEntries;
        ResettersForTesting.register(() -> sMaxEntriesForTests = null);
    }
}
