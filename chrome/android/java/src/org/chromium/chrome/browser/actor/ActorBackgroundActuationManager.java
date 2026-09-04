// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Activity;
import android.util.DisplayMetrics;
import android.view.View;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.compositor.CompositorViewHolderSupplier;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

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

    /** Returns the list of currently active background sessions. */
    public List<BackgroundSession> getBackgroundSessions() {
        return Collections.unmodifiableList(mBackgroundSessions);
    }

    /** Removes the specified background sessions from the active list. */
    public void removeBackgroundSessions(List<BackgroundSession> sessionsToRemove) {
        mBackgroundSessions.removeAll(sessionsToRemove);
    }

    /** Default constructor. */
    public ActorBackgroundActuationManager() {}

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

        setupBackgroundTab(
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
                    if (findSessionByMessageId(glicTriggerMessageId) == null) {
                        // TODO(crbug.com/534401462): Revisit if notifySetupFailed is needed here.
                        // This depends on whether the handler expects a failure callback when
                        // setup is cancelled.
                        Log.d(
                                TAG,
                                "Context %s was cleaned up before page load finished.",
                                glicTriggerMessageId);
                        return;
                    }
                    ActorKeyedService actorService =
                            ActorKeyedServiceFactory.getForProfile(profile);
                    if (actorService == null) {
                        Log.d(TAG, "Failed to get ActorKeyedService during callback");
                        notifySetupFailed(profile, glicTriggerMessageId);
                        return;
                    }
                    actorService.setPreparedBackgroundTab(tab, glicTriggerMessageId);
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
        Tab tab = createOffscreenTab(profile);

        loadBlankThenCallback(
                tab,
                (preparedTab) -> {
                    if (preparedTab == null) {
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
        mBackgroundSessions.addAll(
                ActorTabStateHelper.detachActiveBackgroundSessions(
                        selector, windowId, this::startOffscreenRendering));
    }

    /**
     * Cleans up the background session and associated resources for a given context.
     *
     * @param glicTriggerMessageId The unique identifier for the context to clean up.
     */
    public void cleanupContext(String glicTriggerMessageId) {
        ThreadUtils.assertOnUiThread();
        BackgroundSession session = findSessionByMessageId(glicTriggerMessageId);
        if (session != null) {
            restoreWarmSession(session);
            if (mBackgroundSessions.remove(session)) {
                Tab lastActiveTab = session.getLastActiveTab();
                if (lastActiveTab != null) {
                    OffscreenRenderingManager.getInstance().stopOffscreenRendering(lastActiveTab);
                }
            }
        }
    }

    /**
     * Handles task completion by restoring any warm background sessions for the task and stopping
     * offscreen rendering.
     *
     * @param taskId The ID of the task that completed.
     */
    public void onTaskCompleted(int taskId) {
        ThreadUtils.assertOnUiThread();
        BackgroundSession session =
                BackgroundSession.getSessionForTask(mBackgroundSessions, taskId);
        if (session != null) {
            restoreWarmSession(session);
            if (mBackgroundSessions.remove(session)) {
                Tab lastActiveTab = session.getLastActiveTab();
                if (lastActiveTab != null) {
                    OffscreenRenderingManager.getInstance().stopOffscreenRendering(lastActiveTab);
                }
            }
        }
    }

    /** Destroys all active background sessions. */
    public void destroy() {
        ThreadUtils.assertOnUiThread();
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
    }

    private void restoreWarmSessions() {
        restoreWarmSessions(mBackgroundSessions);
    }

    private void restoreWarmSession(BackgroundSession session) {
        restoreWarmSessions(Collections.singletonList(session));
    }

    private void restoreWarmSessions(List<BackgroundSession> targetSessions) {
        if (targetSessions.isEmpty() || mBackgroundSessions.isEmpty()) return;

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

            List<BackgroundSession> restoredSessions =
                    ActorTabStateHelper.restoreActiveWindowBackgroundTabs(
                            selector, windowId, windowAndroid, targetSessions, tabDelegateFactory);
            mBackgroundSessions.removeAll(restoredSessions);

            if (!restoredSessions.isEmpty()) {
                // Synchronously save tab state to disk upon restoring warm background
                // sessions. When Chrome is in the background or stopped, deferred async saves
                // may not execute before process or Foreground Service terminates, which
                // could cause tab loss on the next cold start.
                flushTabStateToDisk(activity);
            }

            if (mBackgroundSessions.isEmpty()) {
                return;
            }
        }
    }

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

    private Tab createOffscreenTab(Profile profile) {
        WindowAndroid window = OffscreenRenderingManager.getInstance().getOffscreenWindow();
        // TODO(crbug.com/548875143): Persist TabState to disk whenever the background tab's
        // URL/navigation updates so that on-disk state remains continuously accurate.
        Tab tab =
                TabBuilder.createLiveTab(profile, true)
                        .setWindow(window)
                        .setLaunchType(TabLaunchType.FROM_CHROME_UI)
                        .setDelegateFactory(new ActorTabDelegateFactory())
                        .build();
        startOffscreenRendering(tab);
        return tab;
    }

    private void startOffscreenRendering(Tab tab) {
        View compositorView =
                CompositorViewHolderSupplier.getValueOrNullFrom(tab.getWindowAndroid());

        int width;
        int height;
        if (compositorView != null
                && compositorView.getWidth() > 0
                && compositorView.getHeight() > 0) {
            width = compositorView.getWidth();
            height = compositorView.getHeight();
        } else {
            // Fallback to display metrics might behave incorrectly for floating window or
            // split screen mode, but is sufficient for this use case.
            DisplayMetrics displayMetrics =
                    ContextUtils.getApplicationContext().getResources().getDisplayMetrics();
            width = displayMetrics.widthPixels;
            height = displayMetrics.heightPixels;
        }

        OffscreenRenderingManager.getInstance().startOffscreenRendering(tab, width, height);
    }

    private void setupBackgroundTab(
            Profile profile, String glicTriggerMessageId, Callback<@Nullable Tab> callback) {
        ThreadUtils.assertOnUiThread();
        Log.d(TAG, "Provisioning offscreen tab for message: %s", glicTriggerMessageId);
        Tab tab = createOffscreenTab(profile);

        BackgroundSession session = new BackgroundSession(tab, glicTriggerMessageId);
        mBackgroundSessions.add(session);

        loadBlankThenCallback(tab, callback);
    }

    private void loadBlankThenCallback(Tab tab, Callback<@Nullable Tab> callback) {
        ThreadUtils.assertOnUiThread();
        TabObserver observer =
                new TabObserver() {
                    private boolean mInitialLoadFinished;

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        Log.d(TAG, "Actor: Offscreen page load finished: %s", url.getSpec());
                        if (!mInitialLoadFinished) {
                            mInitialLoadFinished = true;
                            callback.onResult(tab);
                            tab.removeObserver(this);
                        }
                    }

                    @Override
                    public void onPageLoadFailed(Tab tab, int errorCode) {
                        Log.d(TAG, "Actor: Offscreen page load failed, error: %d", errorCode);
                        if (!mInitialLoadFinished) {
                            mInitialLoadFinished = true;
                            callback.onResult(null);
                            tab.removeObserver(this);
                        }
                    }

                    @Override
                    public void onCrash(Tab tab) {
                        Log.d(TAG, "Actor: Offscreen tab crashed");
                        if (!mInitialLoadFinished) {
                            mInitialLoadFinished = true;
                            callback.onResult(null);
                            tab.removeObserver(this);
                        }
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        if (!mInitialLoadFinished) {
                            mInitialLoadFinished = true;
                            callback.onResult(null);
                        }
                        tab.removeObserver(this);
                    }
                };
        tab.addObserver(observer);
        tab.loadUrl(new LoadUrlParams("about:blank"));
    }

    private void notifySetupFailed(Profile profile, String glicTriggerMessageId) {
        Log.d(TAG, "Background setup failed, notifying native. messageId=%s", glicTriggerMessageId);
        ActorKeyedService actorService = ActorKeyedServiceFactory.getForProfile(profile);
        if (actorService != null) {
            actorService.notifyBackgroundSetupFailed(glicTriggerMessageId);
        }
        cleanupContext(glicTriggerMessageId);
    }
}
