// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.util.DisplayMetrics;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Orchestrates background actuation of agent tasks by provisioning offscreen tabs and windows. */
@NullMarked
public class ActorBackgroundActuationManager {
    private static final String TAG = "ActorBackgroundMgr";

    /** Constant representing an invalid task ID. */
    public static final int INVALID_TASK_ID = -1;

    /**
     * Represents a background session (either provisioned or transitioned). It owns the offscreen
     * {@link Tab} and manages its lifecycle.
     */
    // TODO(crbug.com/540473733): Deprecate BackgroundSession in favor of
    // org.chromium.chrome.browser.actor.BackgroundSession.
    public static class BackgroundSession {
        private final Tab mTab;
        private final int mTaskId;
        private final @Nullable String mGlicTriggerMessageId;

        /**
         * Constructor for a transitioned session (Scenario 1).
         *
         * @param tab The tab being transitioned.
         * @param taskId The ID of the task associated with the tab.
         */
        public BackgroundSession(Tab tab, int taskId) {
            mTab = tab;
            mTaskId = taskId;
            mGlicTriggerMessageId = null;
        }

        /**
         * Constructor for a provisioned session (Scenario 2).
         *
         * @param tab The newly provisioned tab.
         * @param glicTriggerMessageId The ID of the triggering message.
         */
        public BackgroundSession(Tab tab, String glicTriggerMessageId) {
            mTab = tab;
            mTaskId = INVALID_TASK_ID;
            mGlicTriggerMessageId = glicTriggerMessageId;
        }

        /** Returns the offscreen tab owned by this session. */
        public Tab getTab() {
            return mTab;
        }

        /** Returns the task ID associated with this session, or {@link #INVALID_TASK_ID}. */
        public int getTaskId() {
            return mTaskId;
        }

        /** Returns the triggering message ID associated with this session, or null. */
        public @Nullable String getGlicTriggerMessageId() {
            return mGlicTriggerMessageId;
        }

        void destroy() {
            OffscreenRenderingManager.getInstance().stopOffscreenRendering(mTab);
            if (!mTab.isDestroyed()) {
                mTab.destroy();
            }
        }
    }

    // List of active background sessions.
    private final List<BackgroundSession> mBackgroundSessions = new ArrayList<>();

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
     * Transitions active actor tasks from foreground to background.
     *
     * @param sessions List of sessions to be moved offscreen.
     */
    public void transitionToBackground(List<BackgroundSession> sessions) {
        // TODO: Implement this.
    }

    /**
     * Cleans up the background session for the given context.
     *
     * @param glicTriggerMessageId The unique identifier for the context to clean up.
     */
    public void cleanupContext(String glicTriggerMessageId) {
        ThreadUtils.assertOnUiThread();
        BackgroundSession session = findSessionByMessageId(glicTriggerMessageId);
        if (session != null) {
            mBackgroundSessions.remove(session);
            session.destroy();
        }
    }

    /** Destroys all active background sessions. */
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        // Copy to avoid ConcurrentModificationException when onDestroyed triggers callback
        List<BackgroundSession> sessions = new ArrayList<>(mBackgroundSessions);
        mBackgroundSessions.clear();
        for (BackgroundSession session : sessions) {
            session.destroy();
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

    private void setupBackgroundTab(
            Profile profile, String glicTriggerMessageId, Callback<@Nullable Tab> callback) {
        ThreadUtils.assertOnUiThread();
        Log.d(TAG, "Provisioning offscreen tab for message: %s", glicTriggerMessageId);
        WindowAndroid window = OffscreenRenderingManager.getInstance().getOffscreenWindow();

        Tab tab =
                TabBuilder.createLiveTab(profile, true)
                        .setWindow(window)
                        .setLaunchType(TabLaunchType.FROM_CHROME_UI)
                        .setDelegateFactory(new ActorTabDelegateFactory())
                        .build();

        DisplayMetrics displayMetrics =
                ContextUtils.getApplicationContext().getResources().getDisplayMetrics();
        int width = displayMetrics.widthPixels;
        int height = displayMetrics.heightPixels;

        Log.d(TAG, "Starting offscreen rendering (%dx%d).", width, height);
        OffscreenRenderingManager.getInstance().startOffscreenRendering(tab, width, height);

        loadBlankThenCallback(tab, glicTriggerMessageId, callback);
    }

    private void loadBlankThenCallback(
            Tab tab, String glicTriggerMessageId, Callback<@Nullable Tab> callback) {
        ThreadUtils.assertOnUiThread();
        EmptyTabObserver observer =
                new EmptyTabObserver() {
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
                        Log.d(
                                TAG,
                                "Actor: Offscreen page load failed for message: %s, error: %d",
                                glicTriggerMessageId,
                                errorCode);
                        if (!mInitialLoadFinished) {
                            mInitialLoadFinished = true;
                            callback.onResult(null);
                            tab.removeObserver(this);
                        }
                    }

                    @Override
                    public void onCrash(Tab tab) {
                        Log.d(
                                TAG,
                                "Actor: Offscreen tab crashed for message: %s",
                                glicTriggerMessageId);
                        if (!mInitialLoadFinished) {
                            mInitialLoadFinished = true;
                            callback.onResult(null);
                            tab.removeObserver(this);
                        }
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        tab.removeObserver(this);
                    }
                };
        tab.addObserver(observer);

        BackgroundSession session = new BackgroundSession(tab, glicTriggerMessageId);
        mBackgroundSessions.add(session);

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
