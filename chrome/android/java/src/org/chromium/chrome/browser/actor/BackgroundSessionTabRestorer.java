// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.actor.background_tabs.ActorTabStateHelper;
import org.chromium.chrome.browser.actor.background_tabs.BackgroundSession;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

import java.util.Iterator;
import java.util.List;
import java.util.Set;

/**
 * Restores background tabs from {@link BackgroundSession} instances back into browser windows
 * across both warm and cold window starts, flushes updated tab state to disk, and records
 * restoration latency metrics.
 *
 * <p>Disk-backed {@link ColdBackgroundTab} state loading during {@link TabPersistentStore} startup
 * is handled separately by {@link BackgroundTabRestorationHelper}.
 *
 * <p>Note: This helper resides in {@code //chrome/android:chrome_java} rather than in {@link
 * ActorTabStateHelper} (in {@code //chrome/browser/actor/android:java}) because {@link
 * BackgroundTabPool} and {@link BackgroundTabPoolManager} reside in {@code
 * //chrome/android:chrome_java}, which depends on {@code //chrome/browser/actor/android:java}.
 * Moving this logic into {@link ActorTabStateHelper} would introduce an illegal circular GN build
 * dependency.
 */
@NullMarked
public final class BackgroundSessionTabRestorer {
    private static final String TAG = "BgSessionRestorer";

    private BackgroundSessionTabRestorer() {}

    /**
     * Restores tabs belonging to {@code activeWindowId} (or to closed/unassociated windows) from
     * {@code sessions} into {@code selector}'s regular tab model, removing claimed entries from
     * each {@link BackgroundSession} and recording restoration latency once for the window.
     *
     * @return Total number of tabs attached into the foreground window.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static int restoreSessionsIntoWindow(
            List<BackgroundSession> sessions,
            TabModelSelector selector,
            int activeWindowId,
            WindowAndroid window,
            TabDelegateFactory tabDelegateFactory,
            long waitDurationMs,
            boolean isColdStart) {
        ThreadUtils.assertOnUiThread();
        if (!hasRemainingTabs(sessions)) {
            return 0;
        }
        TabModel model = selector.getModel(/* incognito= */ false);
        if (model == null) {
            return 0;
        }
        Profile profile = model.getProfile();
        if (profile == null || profile.isOffTheRecord()) {
            return 0;
        }

        long startTimeMs = TimeUtils.uptimeMillis();
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(profile);
        int restoredTabCount = 0;
        Set<Integer> usableWindowIds =
                MultiWindowUtils.getUsableInstanceIds(PersistedInstanceType.ACTIVE);
        try {
            for (BackgroundSession session : sessions) {
                Iterator<BackgroundSession.BackgroundTabData> iterator =
                        session.getTabDataList().iterator();
                while (iterator.hasNext()) {
                    BackgroundSession.BackgroundTabData tabData = iterator.next();
                    int tabWindowId = tabData.getTabWindowId();

                    // A window whose task is gone can never claim its tabs back.
                    boolean shouldRestoreIntoThisWindow =
                            (tabWindowId == TabWindowManager.INVALID_WINDOW_ID
                                    || tabWindowId == activeWindowId
                                    || !usableWindowIds.contains(tabWindowId));

                    if (shouldRestoreIntoThisWindow) {
                        if (restoreSessionTab(tabData, pool, model, window, tabDelegateFactory)) {
                            restoredTabCount++;
                        }
                        iterator.remove();
                    }
                }
            }
        } finally {
            BackgroundTabPoolManager.release(pool);
        }

        if (restoredTabCount > 0) {
            long restoreDurationMs = TimeUtils.uptimeMillis() - startTimeMs;
            if (isColdStart) {
                ActorMetrics.recordWaitForTabStateInitializedLatency(waitDurationMs);
            }
            ActorMetrics.recordTotalRestoreLatency(
                    (isColdStart ? waitDurationMs : 0) + restoreDurationMs, isColdStart);
        }
        return restoredTabCount;
    }

    /**
     * Restores matching tabs from {@code sessions} into any running, initialized browser windows,
     * flushing updated tab state to disk once per window that receives tabs and stopping once all
     * sessions in {@code sessions} have no tabs left.
     */
    static void restoreSessions(List<BackgroundSession> sessions) {
        if (!hasRemainingTabs(sessions)) {
            return;
        }

        TabWindowManager windowManager = TabWindowManagerSingleton.getInstance();
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (!(activity instanceof AsyncInitializationActivity asyncActivity)) continue;
            if (asyncActivity.isFinishing() || asyncActivity.isDestroyed()) continue;

            // Only tabbed activities managed by TabWindowManager have valid window IDs;
            // non-tabbed activities and CCTs return INVALID_WINDOW_ID and are safely skipped.
            int windowId = windowManager.getIdForWindow(activity);
            if (windowId == TabWindowManager.INVALID_WINDOW_ID) continue;

            ActivityWindowAndroid windowAndroid = asyncActivity.getWindowAndroid();
            if (windowAndroid == null) continue;

            TabModelSelector selector = windowManager.getTabModelSelectorById(windowId);
            if (selector == null || !selector.isTabStateInitialized()) continue;

            TabDelegateFactory tabDelegateFactory =
                    selector.getTabCreatorManager()
                            .getTabCreator(/* incognito= */ false)
                            .createDefaultTabDelegateFactory();
            if (tabDelegateFactory == null) continue;

            if (restoreSessionsIntoWindow(
                            sessions,
                            selector,
                            windowId,
                            windowAndroid,
                            tabDelegateFactory,
                            /* waitDurationMs= */ 0,
                            /* isColdStart= */ false)
                    > 0) {
                // Synchronously save tab state to disk upon restoring background session tabs.
                // When Chrome is in the background or stopped, deferred async saves may not
                // execute before the process or Foreground Service terminates, which could cause
                // tab loss on the next cold start.
                flushTabStateToDisk(activity);
            }

            if (!hasRemainingTabs(sessions)) {
                return;
            }
        }
    }

    /**
     * Restores a single task's {@link BackgroundSession} into any running, initialized browser
     * windows, exiting once {@code session} has no tabs left.
     *
     * @return True if {@code session} has no remaining tabs after restoration.
     */
    static boolean restoreSession(BackgroundSession session) {
        restoreSessions(List.of(session));
        return session.getTabDataList().isEmpty();
    }

    private static boolean hasRemainingTabs(List<BackgroundSession> sessions) {
        for (BackgroundSession session : sessions) {
            if (!session.getTabDataList().isEmpty()) {
                return true;
            }
        }
        return false;
    }

    private static boolean restoreSessionTab(
            BackgroundSession.BackgroundTabData tabData,
            BackgroundTabPool pool,
            TabModel model,
            WindowAndroid window,
            TabDelegateFactory tabDelegateFactory) {
        Tab originalTab = tabData.getTab();
        if (originalTab == null) return false;

        LiveBackgroundTab liveTab = pool.getLiveTab(originalTab.getId());
        if (liveTab != null) {
            liveTab.attachToForeground(model, window, tabDelegateFactory);
            return true;
        }

        @TabId Integer placeholderId = tabData.getPlaceholderTabId();
        if (placeholderId == null) {
            if (!originalTab.isDestroyed()) {
                ActorTabStateHelper.restoreSessionTabToForeground(
                        originalTab,
                        Tab.INVALID_TAB_ID,
                        tabData.getOriginalTabIndex(),
                        model,
                        window,
                        tabDelegateFactory);
                return true;
            }
            return false;
        }

        Log.w(
                TAG,
                "LiveBackgroundTab %d was evicted or destroyed; skipping attach.",
                originalTab.getId());
        ActorTabStateHelper.removePlaceholderTab(model, placeholderId);
        return false;
    }

    private static void flushTabStateToDisk(Activity activity) {
        if (!(activity instanceof ChromeTabbedActivity cta)) {
            return;
        }
        TabModelOrchestrator orchestrator = cta.getTabModelOrchestratorSupplier().get();
        if (orchestrator != null) {
            orchestrator.getTabPersistentStore().saveState();
        }
    }
}
