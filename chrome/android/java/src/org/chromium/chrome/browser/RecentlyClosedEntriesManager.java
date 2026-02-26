// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.Callback;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.JniRepeatingCallback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
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
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

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
    private static final long SIX_MONTHS_MS = TimeUnit.DAYS.toMillis(6 * 30);
    private static @Nullable RecentlyClosedTabManager sRecentlyClosedTabManagerForTests;
    private static @Nullable Integer sMaxEntriesForTests;

    private final TabModel mRegularTabModel;
    private final Profile mProfile;

    private final MultiInstanceManager mMultiInstanceManager;

    private RecentlyClosedTabManager mRecentlyClosedTabManager;

    private List<RecentlyClosedEntry> mRecentlyClosedEntries = new ArrayList<>();
    private @Nullable Callback<List<RecentlyClosedEntry>> mEntriesUpdatedCallback;

    /** Callback to native for updates to the entry list. */
    private @Nullable JniRepeatingCallback<Long> mNativeUpdatedCallback;

    /**
     * Helper class for the getRecentlyClosedWindowInternal() method. Calls a callback when the tab
     * state is initialized. Similar to {@code TabModelUtils.runOnTabStateInitialized()} but calls
     * the callback when destroyed so the C++ side is always notified. The timestamp is the time in
     * milliseconds since the UNIX epoch when the window containing the tab model was closed. The
     * instance ID is from the Chrome Activity for the window.
     */
    static class TabStateInitializedObserver implements TabModelSelectorObserver {
        private final TabModelSelector mTabModelSelector;
        private final long mTimestamp;
        private final int mInstanceId;
        private @Nullable JniOnceCallback<@Nullable RecentlyClosedWindowMetadata> mCallback;

        TabStateInitializedObserver(
                TabModelSelector selector,
                long timestamp,
                int instanceId,
                JniOnceCallback<@Nullable RecentlyClosedWindowMetadata> callback) {
            assert callback != null;
            mTabModelSelector = selector;
            mTimestamp = timestamp;
            mInstanceId = instanceId;
            mCallback = callback;
            mTabModelSelector.addObserver(this);
        }

        @Override
        public void onTabStateInitialized() {
            // Call the callback with the TabModelSelector.
            assumeNonNull(mCallback);
            RecentlyClosedWindowMetadata result = new RecentlyClosedWindowMetadata();
            result.tabModel = mTabModelSelector.getCurrentModel();
            result.timestamp = mTimestamp;
            result.instanceId = mInstanceId;
            mCallback.onResult(result);
            // Set the callback to null to indicate we ran it.
            mCallback = null;
            mTabModelSelector.removeObserver(this);
        }

        @Override
        public void onDestroyed() {
            // If the callback wasn't called, notify the C++ side so it can continue.
            if (mCallback != null) {
                mCallback.onResult(null);
                mCallback = null;
            }
            mTabModelSelector.removeObserver(this);
        }
    }

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
        mProfile = assumeNonNull(mRegularTabModel.getProfile());
        mRecentlyClosedTabManager =
                sRecentlyClosedTabManagerForTests != null
                        ? sRecentlyClosedTabManagerForTests
                        : new RecentlyClosedBridge(mProfile, tabModelSelector);
        mRecentlyClosedTabManager.setEntriesUpdatedRunnable(this::updateRecentlyClosedEntries);
    }

    /**
     * @return Most up-to-date list of recently closed entries.
     */
    public List<RecentlyClosedEntry> getRecentlyClosedEntries() {
        return mRecentlyClosedEntries;
    }

    /**
     * Sets a callback to be fired on updates. Callbacks are scoped to the provided {@code profile}.
     * The callback is fired with the native browser context of the RecentlyClosedEntriesManager
     * being updated.
     */
    @CalledByNative
    public static void setNativeUpdatedCallback(
            @JniType("Profile*") Profile profile, @Nullable JniRepeatingCallback<Long> callback) {
        // All managers are notified about each window update, so just use the first one that
        // matches the browser context.
        Set<RecentlyClosedEntriesManager> managers =
                RecentlyClosedEntriesManagerTrackerImpl.getInstance().getManagers();
        for (RecentlyClosedEntriesManager manager : managers) {
            if (manager.mProfile == profile) {
                manager.mNativeUpdatedCallback = callback;
                return;
            }
        }
    }

    /**
     * Returns the TabModel and other metadata via callback for a recently closed window with the
     * given instance ID. If the instance ID is {@code TabWindowManager.INVALID_WINDOW_ID}, the most
     * recently closed window is returned. If no window is found the callback is invoked with null.
     */
    @CalledByNative
    public static void getRecentlyClosedWindow(
            int instanceId, JniOnceCallback<@Nullable RecentlyClosedWindowMetadata> callback) {
        // This function requires the kRecentlyClosedTabsAndWindows feature.
        if (!UiUtils.isRecentlyClosedTabsAndWindowsEnabled()) {
            callback.onResult(null);
            return;
        }

        Set<RecentlyClosedEntriesManager> managers =
                RecentlyClosedEntriesManagerTrackerImpl.getInstance().getManagers();
        if (managers.size() == 0) {
            callback.onResult(null);
            return;
        }

        // All managers are notified about each window close, so pick the first one.
        RecentlyClosedEntriesManager manager = managers.iterator().next();

        // Move from static to instance method to simplify using inner classes.
        manager.getRecentlyClosedWindowInternal(instanceId, callback);
    }

    @VisibleForTesting
    public void getRecentlyClosedWindowInternal(
            int instanceId, JniOnceCallback<@Nullable RecentlyClosedWindowMetadata> callback) {
        // Look up recently closed windows.
        List<RecentlyClosedWindow> windows = getRecentlyClosedWindows();
        if (windows.size() == 0) {
            callback.onResult(null);
            return;
        }

        // Look for the window.
        RecentlyClosedWindow window = null;
        if (instanceId == TabWindowManager.INVALID_WINDOW_ID) {
            // Use the first window. Entries are sorted by close time, most recently closed first.
            window = windows.get(0);
        } else {
            // Search for a window with matching instance id.
            for (RecentlyClosedWindow w : windows) {
                if (w.getInstanceId() == instanceId) {
                    window = w;
                    break;
                }
            }
        }

        // Return an error if no window was found.
        if (window == null) {
            callback.onResult(null);
            return;
        }

        // Milliseconds since UNIX epoch when this entry was created.
        final long timestamp = window.getDate().getTime();

        // Get the window's instance ID in case we looked it up with instanceId == -1.
        final int windowInstanceId = window.getInstanceId();

        // Get the TabModelSelector for the closed window.
        TabModelSelector selector =
                TabWindowManagerSingleton.getInstance()
                        .getTabModelSelectorById(window.getInstanceId());
        if (selector == null) {
            callback.onResult(null);
            return;
        }

        // If the tab state is initialized, run the callback now. Post it as a task so it's
        // always async from the C++ side, which simplifies the calling code.
        if (selector.isTabStateInitialized()) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        RecentlyClosedWindowMetadata result = new RecentlyClosedWindowMetadata();
                        result.tabModel = selector.getCurrentModel();
                        result.timestamp = timestamp;
                        result.instanceId = windowInstanceId;
                        callback.onResult(result);
                    });
            return;
        }

        // Otherwise wait for tab state to be initialized. The observer adds and removes itself.
        new TabStateInitializedObserver(selector, timestamp, windowInstanceId, callback);
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
        List<InstanceInfo> instanceInfoList = mMultiInstanceManager.getRecentlyClosedInstances();
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
     * Notifies relevant listeners (for e.g. Recent Tabs page) when windows are closed.
     *
     * @param windows The windows that were just closed.
     * @param isPermanentDeletion Whether the windows are permanently deleted. If {@code false}, the
     *     windows will be added as the most recently closed entries.
     */
    @VisibleForTesting
    public void onWindowsClosed(List<RecentlyClosedWindow> windows, boolean isPermanentDeletion) {
        // First, remove the entries from the current position in the list if they exist.
        List<Integer> instanceIds = new ArrayList<>(windows.size());
        for (RecentlyClosedWindow window : windows) {
            instanceIds.add(window.getInstanceId());
        }
        removeWindowEntries(instanceIds);

        // If inactive windows were explicitly closed by the user, add them to the top of the list.
        if (!isPermanentDeletion) {
            mRecentlyClosedEntries.addAll(0, windows);
        }

        // Remove the excess entries from the list, and clean up the storage.
        if (mRecentlyClosedEntries.size() > RECENTLY_CLOSED_MAX_ENTRY_COUNT_WITH_WINDOW) {
            List<Integer> excessInstanceIds = new ArrayList<>();
            int excessSessionEntriesCount = 0;

            while (mRecentlyClosedEntries.size() > RECENTLY_CLOSED_MAX_ENTRY_COUNT_WITH_WINDOW) {
                RecentlyClosedEntry excessEntry =
                        mRecentlyClosedEntries.remove(mRecentlyClosedEntries.size() - 1);
                if (excessEntry instanceof SessionRecentlyClosedEntry) {
                    excessSessionEntriesCount++;
                } else if (excessEntry instanceof RecentlyClosedWindow excessWindow) {
                    excessInstanceIds.add(excessWindow.getInstanceId());
                }
            }

            if (excessSessionEntriesCount > 0) {
                mRecentlyClosedTabManager.clearLeastRecentlyUsedClosedEntries(
                        /* numToRemove= */ excessSessionEntriesCount);
            }
            if (excessInstanceIds.size() > 0) {
                mMultiInstanceManager.closeWindows(
                        excessInstanceIds, CloseWindowAppSource.RECENT_TABS);
            }

            assert mRecentlyClosedEntries.size() <= RECENTLY_CLOSED_MAX_ENTRY_COUNT_WITH_WINDOW;
        }

        if (mEntriesUpdatedCallback != null) {
            mEntriesUpdatedCallback.onResult(mRecentlyClosedEntries);
        }

        if (mNativeUpdatedCallback != null) {
            mNativeUpdatedCallback.onResult(mProfile.getNativeBrowserContextPointer());
        }
    }

    /**
     * Notifies relevant listeners (for e.g. Recent Tabs page) when a window is restored.
     *
     * @param instanceId The id of the instance that was restored.
     */
    @VisibleForTesting
    public void onWindowRestored(int instanceId) {
        removeWindowEntries(Collections.singletonList(instanceId));
        if (mEntriesUpdatedCallback != null) {
            mEntriesUpdatedCallback.onResult(mRecentlyClosedEntries);
        }

        if (mNativeUpdatedCallback != null) {
            mNativeUpdatedCallback.onResult(mProfile.getNativeBrowserContextPointer());
        }
    }

    private void removeWindowEntries(List<Integer> instanceIds) {
        mRecentlyClosedEntries.removeIf(
                entry ->
                        entry instanceof RecentlyClosedWindow window
                                && instanceIds.contains(window.getInstanceId()));
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

        if (mNativeUpdatedCallback != null) {
            mNativeUpdatedCallback.destroy();
            mNativeUpdatedCallback = null;
        }
    }

    private void getRecentlyClosedTabsAndWindows(
            @Nullable List<RecentlyClosedEntry> recentlyClosedSessionEntries) {
        List<RecentlyClosedWindow> recentlyClosedWindows = getRecentlyClosedWindows();
        // Cleanup old session entries.
        recentlyClosedSessionEntries = removeInvalidSessionEntries(recentlyClosedSessionEntries);

        mRecentlyClosedEntries = new ArrayList<>();

        int windowEntrySize = recentlyClosedWindows.size();
        int sessionEntrySize = recentlyClosedSessionEntries.size();
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

    /* Removes session entries whose retention period has expired and clears them from storage. */
    private List<RecentlyClosedEntry> removeInvalidSessionEntries(
            @Nullable List<RecentlyClosedEntry> sessionEntries) {
        if (sessionEntries == null) return new ArrayList<>();
        int currentIndex = 0;
        int size = sessionEntries.size();
        while (currentIndex < size) {
            long timestampMs = sessionEntries.get(currentIndex).getDate().getTime();
            if (timestampMs > 0 && (TimeUtils.currentTimeMillis() - timestampMs) > SIX_MONTHS_MS) {
                break;
            }
            currentIndex++;
        }

        // Clear the current and all subsequent entries since their retention period has expired.
        if (currentIndex < size) {
            mRecentlyClosedTabManager.clearLeastRecentlyUsedClosedEntries(size - currentIndex);
        }

        return sessionEntries.subList(0, currentIndex);
    }

    private List<RecentlyClosedWindow> getRecentlyClosedWindows() {
        List<InstanceInfo> instanceInfoList = mMultiInstanceManager.getRecentlyClosedInstances();
        List<RecentlyClosedWindow> recentlyClosedWindows = new ArrayList<>();

        for (InstanceInfo info : instanceInfoList) {
            // Use lastAccessedTime as the closure time if a valid closureTime is not available.
            long closureTime = info.closureTime > 0 ? info.closureTime : info.lastAccessedTime;
            assert closureTime > 0 : "Expected a valid window closure time.";
            recentlyClosedWindows.add(
                    new RecentlyClosedWindow(
                            closureTime,
                            info.instanceId,
                            info.url,
                            info.customTitle,
                            info.title,
                            info.tabCount));
        }

        recentlyClosedWindows.sort(
                (window1, window2) ->
                        Long.compare(window2.getDate().getTime(), window1.getDate().getTime()));
        return recentlyClosedWindows;
    }

    private static boolean canRestoreWindow() {
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
