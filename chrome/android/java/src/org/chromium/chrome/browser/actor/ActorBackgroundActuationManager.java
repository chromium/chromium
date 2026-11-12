// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Activity;
import android.text.TextUtils;
import android.util.ArrayMap;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.actor.background_tabs.ActorTabStateHelper;
import org.chromium.chrome.browser.actor.background_tabs.BackgroundSession;
import org.chromium.chrome.browser.actor.background_tabs.OffscreenRenderingManager;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabwindow.TabWindowInfo;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Orchestrates background actuation of agent tasks by provisioning offscreen tabs and windows.
 * Created before a FGS is started and destroyed when the FGS is destroyed.
 */
@NullMarked
public class ActorBackgroundActuationManager {
    private static final String TAG = "ActorBackgroundMgr";

    /** Constant representing an invalid task ID. */
    public static final int INVALID_TASK_ID = -1;

    // List of active background sessions.
    private final List<BackgroundSession> mBackgroundSessions = new ArrayList<>();
    private final OffscreenTabProvisioner mTabProvisioner = new OffscreenTabProvisioner();
    // Set of tab IDs originally provisioned via FCM background triggering.
    private final Set<Integer> mFcmInitiatedTabIds = new HashSet<>();

    /** Returns the list of currently active background sessions. */
    public List<BackgroundSession> getBackgroundSessions() {
        return Collections.unmodifiableList(mBackgroundSessions);
    }

    /** Removes the specified background sessions from the active list. */
    public void removeBackgroundSessions(List<BackgroundSession> sessionsToRemove) {
        mBackgroundSessions.removeAll(sessionsToRemove);
        ActorMetrics.getInstance().refreshActiveTaskModes();
    }

    /** Default constructor. */
    public ActorBackgroundActuationManager() {}

    /**
     * Returns the window holding the tab handed off to background actuation for the given task, or
     * {@link TabWindowManager#INVALID_WINDOW_ID} if there is none.
     */
    public int getWindowIdForTask(int taskId) {
        for (BackgroundSession session : mBackgroundSessions) {
            if (session.getTaskId() == null || session.getTaskId() != taskId) continue;

            for (BackgroundSession.BackgroundTabData tabData : session.getTabDataList()) {
                int tabWindowId = tabData.getTabWindowId();
                if (tabWindowId != TabWindowManager.INVALID_WINDOW_ID) {
                    return tabWindowId;
                }
                Integer placeholderTabId = tabData.getPlaceholderTabId();
                if (placeholderTabId != null && placeholderTabId != Tab.INVALID_TAB_ID) {
                    TabWindowInfo placeholderInfo =
                            TabWindowManagerSingleton.getInstance()
                                    .getTabWindowInfoById(placeholderTabId);
                    if (placeholderInfo != null) return placeholderInfo.windowId;
                }
            }
        }
        return TabWindowManager.INVALID_WINDOW_ID;
    }

    /**
     * Starts background actuation for the given profile and context.
     *
     * @param profile The profile to use.
     * @param glicTriggerMessageId The unique identifier for the triggering message.
     */
    public void startBackgroundActuation(Profile profile, String glicTriggerMessageId) {
        Log.d(
                TAG,
                "startBackgroundActuation called. glicTriggerMessageId=%s",
                glicTriggerMessageId);
        ThreadUtils.assertOnUiThread();

        mTabProvisioner.setupBackgroundTab(
                profile,
                glicTriggerMessageId,
                (tab) -> {
                    ThreadUtils.assertOnUiThread();
                    if (tab == null) {
                        notifySetupFailed(profile, glicTriggerMessageId);
                        return;
                    }
                    Log.d(
                            TAG,
                            "Background tab ready for JNI call. messageId=%s",
                            glicTriggerMessageId);
                    ActorKeyedService actorService =
                            ActorKeyedServiceFactory.getForProfile(profile);
                    if (actorService == null) {
                        Log.d(TAG, "Failed to get ActorKeyedService during callback");
                        notifySetupFailed(profile, glicTriggerMessageId);
                        return;
                    }
                    mTabProvisioner.releasePendingTab(glicTriggerMessageId);
                    mFcmInitiatedTabIds.add(tab.getId());
                    tab.addObserver(
                            new TabObserver() {
                                @Override
                                public void onDestroyed(Tab destroyedTab) {
                                    mFcmInitiatedTabIds.remove(destroyedTab.getId());
                                    destroyedTab.removeObserver(this);
                                }
                            });
                    BackgroundSession session = new BackgroundSession(tab, glicTriggerMessageId);
                    mBackgroundSessions.add(session);
                    ingestSessionsIntoPool(profile, List.of(session));
                    actorService.setTabReady(tab, glicTriggerMessageId);
                    ActorMetrics.getInstance().refreshActiveTaskModes();
                    ActorMetrics.recordBackgroundActuationTrigger(
                            ActorMetrics.ActorBackgroundActuationTrigger.TASK_INIT_BY_FCM);
                    ActorMetrics.recordTabCreatedInBackground(
                            ActorMetrics.ActorBackgroundTabCreationStatus.TASK_INIT_BY_FCM_SUCCESS);
                });
    }

    /**
     * Provisions an offscreen tab on demand for the specified task ID.
     *
     * @param profile The profile to use.
     * @param taskId The task ID for which to provision the background tab.
     * @param callback Callback invoked with the prepared tab, or null if setup failed.
     */
    public void provisionBackgroundTabForTask(
            Profile profile, int taskId, Callback<@Nullable Tab> callback) {
        ThreadUtils.assertOnUiThread();
        Tab tab = mTabProvisioner.createOffscreenTab(profile);

        mTabProvisioner.loadBlankThenCallback(
                tab,
                (preparedTab) -> {
                    if (preparedTab == null) {
                        ActorMetrics.recordTabCreatedInBackground(
                                ActorMetrics.ActorBackgroundTabCreationStatus
                                        .TASK_INIT_BY_CHROME_FAILURE);
                        OffscreenRenderingManager.getInstance().stopOffscreenRendering(tab);
                        tab.destroy();
                        callback.onResult(null);
                        return;
                    }
                    BackgroundSession session =
                            BackgroundSession.getSessionForTask(mBackgroundSessions, taskId);
                    if (session == null) {
                        session = new BackgroundSession(preparedTab, taskId);
                        mBackgroundSessions.add(session);
                    } else {
                        session.addTab(preparedTab);
                    }
                    ingestSessionsIntoPool(profile, List.of(session));
                    ActorMetrics.getInstance().refreshActiveTaskModes();
                    ActorMetrics.recordTabCreatedInBackground(
                            ActorMetrics.ActorBackgroundTabCreationStatus
                                    .TASK_INIT_BY_CHROME_SUCCESS);
                    callback.onResult(preparedTab);
                });
    }

    /**
     * Transitions active tasks from foreground activity to background rendering.
     *
     * @param selector The TabModelSelector of the stopping activity.
     */
    public void transitionActiveTasksToBackground(TabModelSelector selector) {
        ThreadUtils.assertOnUiThread();
        int windowId = TabWindowManagerSingleton.getInstance().getWindowIdForSelector(selector);
        TabModel model = selector.getModel(/* incognito= */ false);
        if (model == null) return;
        Profile profile = model.getProfile();
        if (profile == null || profile.isOffTheRecord()) return;

        List<BackgroundSession> detachedSessions =
                ActorTabStateHelper.detachActiveBackgroundSessions(
                        selector, windowId, mTabProvisioner::startOffscreenRendering);
        if (detachedSessions.isEmpty()) {
            return;
        }

        Set<Integer> fcmTaskIds = new HashSet<>();
        for (BackgroundSession session : detachedSessions) {
            if (session.getTaskId() != null && isFcmInitiatedSession(session)) {
                fcmTaskIds.add(session.getTaskId());
            }
        }

        for (BackgroundSession session : detachedSessions) {
            Integer taskId = session.getTaskId();
            if (taskId != null) {
                if (fcmTaskIds.contains(taskId)) {
                    ActorMetrics.markBackgroundActuationTriggerRecorded(taskId);
                    for (Tab tab : session.getTabs()) {
                        if (tab != null) {
                            mFcmInitiatedTabIds.remove(tab.getId());
                        }
                    }
                } else {
                    ActorMetrics.maybeRecordBackgroundActuationTrigger(
                            taskId,
                            ActorMetrics.ActorBackgroundActuationTrigger.TASK_INIT_BY_CHROME);
                }
            }
        }

        mBackgroundSessions.addAll(detachedSessions);
        ingestSessionsIntoPool(profile, detachedSessions);
        ActorMetrics.getInstance().refreshActiveTaskModes();
    }

    private boolean isFcmInitiatedSession(BackgroundSession session) {
        if (!TextUtils.isEmpty(session.getGlicTriggerMessageId())) {
            return true;
        }
        for (Tab tab : session.getTabs()) {
            if (tab != null && mFcmInitiatedTabIds.contains(tab.getId())) {
                return true;
            }
        }
        return false;
    }

    private void ingestSessionsIntoPool(Profile profile, List<BackgroundSession> sessions) {
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(profile);
        try {
            for (BackgroundSession session : sessions) {
                for (BackgroundSession.BackgroundTabData tabData : session.getTabDataList()) {
                    Tab tab = tabData.getTab();
                    if (tab != null && !tab.isDestroyed() && pool.getLiveTab(tab.getId()) == null) {
                        Integer placeholderId = tabData.getPlaceholderTabId();
                        int placeholderTabId =
                                placeholderId != null ? placeholderId : Tab.INVALID_TAB_ID;
                        LiveBackgroundTab liveTab =
                                new LiveBackgroundTab(
                                        pool, tab, placeholderTabId, tabData.getOriginalTabIndex());
                        LiveBackgroundTab.markDirty(tab);
                        pool.addLiveTab(liveTab);
                    }
                }
            }
        } finally {
            BackgroundTabPoolManager.release(pool);
        }
    }

    /**
     * Cleans up the background session and associated resources for a given context.
     *
     * @param glicTriggerMessageId The unique identifier for the context to clean up.
     */
    public void cleanupContext(String glicTriggerMessageId) {
        ThreadUtils.assertOnUiThread();
        if (mTabProvisioner.cancelPendingTab(glicTriggerMessageId)) {
            return;
        }

        BackgroundSession session = findSessionByMessageId(glicTriggerMessageId);
        if (session != null) {
            for (Tab tab : session.getTabs()) {
                if (tab != null) {
                    mFcmInitiatedTabIds.remove(tab.getId());
                }
            }
            restoreWarmSession(session);
            if (mBackgroundSessions.contains(session)) {
                for (Tab tab : session.getTabs()) {
                    if (tab != null) {
                        if (!tab.isDestroyed()) {
                            LiveBackgroundTab.markDirty(tab);
                        }
                        OffscreenRenderingManager.getInstance().stopOffscreenRendering(tab);
                    }
                }
            }
        }
    }

    /**
     * Returns whether the given task currently has an active background actuation session.
     *
     * @param taskId The ID of the task to check.
     * @return True if the task has an active background session, false otherwise.
     */
    public boolean hasBackgroundSessionForTask(int taskId) {
        ThreadUtils.assertOnUiThread();
        if (BackgroundSession.getSessionForTask(mBackgroundSessions, taskId) != null) {
            return true;
        }
        // TODO(crbug.com/570050447): Bind message-scoped BackgroundSessions to their taskId once
        // the task is created so this tab-ID fallback can be replaced by
        // BackgroundSession.getSessionForTask.
        Map<Profile, @Nullable Set<Integer>> taskTabIdsByProfile = new ArrayMap<>();
        for (BackgroundSession session : mBackgroundSessions) {
            if (session.getTaskId() != null) continue;
            for (Tab tab : session.getTabs()) {
                if (tab == null || tab.isDestroyed()) continue;
                Profile profile = tab.getProfile();
                if (profile == null) continue;
                Profile originalProfile = profile.getOriginalProfile();
                Set<Integer> taskTabIds;
                if (taskTabIdsByProfile.containsKey(originalProfile)) {
                    taskTabIds = taskTabIdsByProfile.get(originalProfile);
                } else {
                    taskTabIds = getTaskTabIdsForProfile(originalProfile, taskId);
                    taskTabIdsByProfile.put(originalProfile, taskTabIds);
                }
                if (taskTabIds != null && taskTabIds.contains(tab.getId())) {
                    return true;
                }
            }
        }
        return false;
    }

    private static @Nullable Set<Integer> getTaskTabIdsForProfile(Profile profile, int taskId) {
        ActorKeyedService service = ActorKeyedServiceFactory.getForProfile(profile);
        ActorTask task = service != null ? service.getTask(taskId) : null;
        if (task == null) {
            return null;
        }
        Set<Integer> tabIds = new HashSet<>(task.getTabs());
        int lastActuatedTabId = task.getLastActuatedTabId();
        if (lastActuatedTabId != Tab.INVALID_TAB_ID) {
            tabIds.add(lastActuatedTabId);
        }
        return tabIds;
    }

    /**
     * Handles task completion by restoring any warm background sessions for the task and stopping
     * offscreen rendering.
     *
     * @param taskId The ID of the task that completed.
     */
    public void onTaskCompleted(int taskId) {
        ThreadUtils.assertOnUiThread();
        if (ProfileManager.isInitialized()) {
            Profile profile = ProfileManager.getLastUsedRegularProfile();
            ActorKeyedService service = ActorKeyedServiceFactory.getForProfile(profile);
            if (service != null) {
                ActorTask task = service.getTask(taskId);
                if (task != null) {
                    mFcmInitiatedTabIds.removeAll(task.getTabs());
                    int lastActuatedTabId = task.getLastActuatedTabId();
                    if (lastActuatedTabId != Tab.INVALID_TAB_ID) {
                        mFcmInitiatedTabIds.remove(lastActuatedTabId);
                    }
                }
            }
        }

        BackgroundSession session =
                BackgroundSession.getSessionForTask(mBackgroundSessions, taskId);
        if (session == null) {
            return;
        }

        for (Tab tab : session.getTabs()) {
            if (tab != null) {
                mFcmInitiatedTabIds.remove(tab.getId());
            }
        }
        restoreWarmSession(session);
        if (mBackgroundSessions.contains(session)) {
            // If no activity was alive to restore into, stop offscreen rendering on all
            // session tabs. Persistence is handled by the caller
            // (ActorForegroundServiceControllerImpl).
            for (Tab tab : session.getTabs()) {
                if (tab != null) {
                    OffscreenRenderingManager.getInstance().stopOffscreenRendering(tab);
                }
            }
        }
    }

    /** Destroys all active background sessions. */
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        mTabProvisioner.destroyPendingTabs();
        mFcmInitiatedTabIds.clear();

        restoreWarmSessions();
        // Copy to avoid ConcurrentModificationException when onDestroyed triggers callback
        List<BackgroundSession> sessions = new ArrayList<>(mBackgroundSessions);
        mBackgroundSessions.clear();
        for (BackgroundSession session : sessions) {
            Tab lastActiveTab = session.getLastActiveTab();
            if (lastActiveTab != null) {
                OffscreenRenderingManager.getInstance().stopOffscreenRendering(lastActiveTab);
            }
        }
        ActorMetrics.getInstance().refreshActiveTaskModes();
    }

    /**
     * Restores background tabs belonging to the active window context from the manager's active
     * background sessions and removes any fully restored sessions.
     *
     * @param selector The TabModelSelector of the active foreground window.
     * @param activeWindowId The WindowId of the active foreground window.
     * @param window The WindowAndroid instance of the active foreground window.
     * @param tabDelegateFactory The delegate factory for the foreground window.
     * @param waitDurationMs Time in milliseconds spent waiting for tab state initialization.
     * @param isColdStart Whether tab state was uninitialized when restoration was requested.
     * @return True if at least one background tab was restored into the foreground window.
     */
    public boolean restoreActiveWindowBackgroundTabs(
            TabModelSelector selector,
            int activeWindowId,
            WindowAndroid window,
            TabDelegateFactory tabDelegateFactory,
            long waitDurationMs,
            boolean isColdStart) {
        return WarmBackgroundTabRestorer.restoreTabsForWindow(
                        selector,
                        activeWindowId,
                        window,
                        mBackgroundSessions,
                        tabDelegateFactory,
                        waitDurationMs,
                        isColdStart,
                        this::removeBackgroundSessions)
                > 0;
    }

    /**
     * Restores background tabs belonging to the active window context from the given sessions.
     *
     * @param selector The TabModelSelector of the active foreground window.
     * @param activeWindowId The WindowId of the active foreground window.
     * @param window The WindowAndroid instance of the active foreground window.
     * @param backgroundSessions The list of currently tracked active background sessions.
     * @param tabDelegateFactory The delegate factory for the foreground window.
     * @return List of BackgroundSession instances that have been completely restored.
     */
    // TODO(crbug.com/585091618): Call WarmBackgroundTabRestorer directly from tests and remove
    // this wrapper.
    public static List<BackgroundSession> restoreActiveWindowBackgroundTabsForTesting(
            TabModelSelector selector,
            int activeWindowId,
            WindowAndroid window,
            List<BackgroundSession> backgroundSessions,
            TabDelegateFactory tabDelegateFactory) {
        return WarmBackgroundTabRestorer.restoreActiveWindowBackgroundTabs(
                selector, activeWindowId, window, backgroundSessions, tabDelegateFactory);
    }

    private void restoreWarmSessions() {
        WarmBackgroundTabRestorer.restoreWarmSessions(
                mBackgroundSessions,
                mBackgroundSessions,
                this::removeBackgroundSessions,
                ActorBackgroundActuationManager::flushTabStateToDisk);
    }

    private void restoreWarmSession(BackgroundSession session) {
        WarmBackgroundTabRestorer.restoreWarmSessions(
                Collections.singletonList(session),
                mBackgroundSessions,
                this::removeBackgroundSessions,
                ActorBackgroundActuationManager::flushTabStateToDisk);
    }

    // TODO(crbug.com/585189803): Move flushTabStateToDisk into WarmBackgroundTabRestorer and drop
    // flushStateCallback once the ChromeTabbedActivity DEPS restriction is removed.
    private static void flushTabStateToDisk(Activity activity) {
        if (!(activity instanceof ChromeTabbedActivity cta)) {
            return;
        }
        TabModelOrchestrator orchestrator = cta.getTabModelOrchestratorSupplier().get();
        if (orchestrator != null) {
            TabPersistentStore store = orchestrator.getTabPersistentStore();
            if (store != null) {
                store.saveState();
            }
        }
    }

    private @Nullable BackgroundSession findSessionByMessageId(String messageId) {
        for (BackgroundSession session : mBackgroundSessions) {
            if (messageId.equals(session.getGlicTriggerMessageId())) {
                return session;
            }
        }
        return null;
    }

    private void notifySetupFailed(Profile profile, String glicTriggerMessageId) {
        Log.d(TAG, "Background setup failed, notifying native. messageId=%s", glicTriggerMessageId);
        ActorMetrics.recordTabCreatedInBackground(
                ActorMetrics.ActorBackgroundTabCreationStatus.TASK_INIT_BY_FCM_FAILURE);
        ActorKeyedService actorService = ActorKeyedServiceFactory.getForProfile(profile);
        if (actorService != null) {
            actorService.notifyBackgroundSetupFailed(glicTriggerMessageId);
        }
        cleanupContext(glicTriggerMessageId);
    }
}
