// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Activity;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.actor.background_tabs.ActorTabStateHelper;
import org.chromium.chrome.browser.actor.background_tabs.BackgroundSession;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/**
 * Restores live background tabs from {@link BackgroundSession} instances back into open foreground
 * browser windows and records warm restoration latency metrics.
 */
@NullMarked
final class WarmBackgroundTabRestorer {
    private static final String TAG = "ActorBackgroundMgr";

    private WarmBackgroundTabRestorer() {}

    static int restoreTabsForWindow(
            TabModelSelector selector,
            int activeWindowId,
            WindowAndroid window,
            List<BackgroundSession> targetSessions,
            TabDelegateFactory tabDelegateFactory,
            long waitDurationMs,
            boolean isColdStart,
            Callback<List<BackgroundSession>> onSessionsRestored) {
        long startTimeMs = TimeUtils.uptimeMillis();
        List<BackgroundSession> sessionsToRemove = new ArrayList<>();
        int restoredTabCount =
                restoreActiveWindowBackgroundTabsInternal(
                        selector,
                        activeWindowId,
                        window,
                        targetSessions,
                        tabDelegateFactory,
                        sessionsToRemove);
        onSessionsRestored.onResult(sessionsToRemove);
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
     * Restores background tabs belonging to the active window context from the given sessions.
     *
     * <p>Note: This helper resides in {@code //chrome/android:chrome_java} rather than in {@link
     * ActorTabStateHelper} (in {@code //chrome/browser/actor/android:java}) because {@link
     * BackgroundTabPool} and {@link BackgroundTabPoolManager} reside in {@code
     * //chrome/android:chrome_java}, which depends on {@code //chrome/browser/actor/android:java}.
     * Moving this logic into {@link ActorTabStateHelper} would introduce an illegal circular GN
     * build dependency.
     */
    static List<BackgroundSession> restoreActiveWindowBackgroundTabs(
            TabModelSelector selector,
            int activeWindowId,
            WindowAndroid window,
            List<BackgroundSession> backgroundSessions,
            TabDelegateFactory tabDelegateFactory) {
        List<BackgroundSession> sessionsToRemove = new ArrayList<>();
        restoreActiveWindowBackgroundTabsInternal(
                selector,
                activeWindowId,
                window,
                backgroundSessions,
                tabDelegateFactory,
                sessionsToRemove);
        return sessionsToRemove;
    }

    // TODO(crbug.com/585091500): Drop activeSessions parameter and derive stop condition from
    // targetSessions.
    // TODO(crbug.com/585189803): Move flushTabStateToDisk into this class and drop
    // flushStateCallback once the ChromeTabbedActivity DEPS restriction is removed.
    static void restoreWarmSessions(
            List<BackgroundSession> targetSessions,
            List<BackgroundSession> activeSessions,
            Callback<List<BackgroundSession>> onSessionsRestored,
            Callback<Activity> flushStateCallback) {
        if (targetSessions.isEmpty() || activeSessions.isEmpty()) return;

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

            if (restoreTabsForWindow(
                            selector,
                            windowId,
                            windowAndroid,
                            targetSessions,
                            tabDelegateFactory,
                            /* waitDurationMs= */ 0,
                            /* isColdStart= */ false,
                            onSessionsRestored)
                    > 0) {
                // Synchronously save tab state to disk upon restoring warm background
                // sessions. When Chrome is in the background or stopped, deferred async saves
                // may not execute before process or Foreground Service terminates, which
                // could cause tab loss on the next cold start.
                flushStateCallback.onResult(activity);
            }

            if (activeSessions.isEmpty()) {
                return;
            }
        }
    }

    private static int restoreActiveWindowBackgroundTabsInternal(
            TabModelSelector selector,
            int activeWindowId,
            WindowAndroid window,
            List<BackgroundSession> backgroundSessions,
            TabDelegateFactory tabDelegateFactory,
            List<BackgroundSession> sessionsToRemove) {
        ThreadUtils.assertOnUiThread();
        TabModel model = selector.getModel(/* incognito= */ false);
        if (model == null) {
            return 0;
        }
        Profile profile = model.getProfile();
        if (profile == null || profile.isOffTheRecord()) {
            return 0;
        }

        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(profile);
        int restoredTabCount = 0;
        Set<Integer> usableWindowIds =
                MultiWindowUtils.getUsableInstanceIds(PersistedInstanceType.ACTIVE);
        try {
            for (BackgroundSession session : backgroundSessions) {
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

                if (session.getTabDataList().isEmpty()) {
                    sessionsToRemove.add(session);
                }
            }
        } finally {
            BackgroundTabPoolManager.release(pool);
        }

        return restoredTabCount;
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
        } else if (tabData.getPlaceholderTabId() == null) {
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
        } else {
            Log.w(
                    TAG,
                    "LiveBackgroundTab %d was evicted or destroyed; skipping warm attach.",
                    originalTab.getId());
            Integer placeholderId = tabData.getPlaceholderTabId();
            if (placeholderId != null) {
                ActorTabStateHelper.removePlaceholderTab(model, placeholderId);
            }
            return false;
        }
    }
}
