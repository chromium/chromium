// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.SparseArray;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;

import java.lang.ref.WeakReference;

/**
 * Holds the currently active {@link SessionHandler} and redirects relevant intents and calls into
 * it. {@link SessionHandler} is an interface owned by the currently focused activity that has a
 * linkage to a third party client app through a session.
 */
@NullMarked
public class SessionDataHolder {
    private final SparseArray<SessionData> mTaskIdToSessionData = new SparseArray<>();

    private @Nullable SessionHandler mActiveSessionHandler;

    private @Nullable Callback<SessionHolder> mSessionDisconnectCallback;

    private static SessionDataHolder sInstance = new SessionDataHolder();

    @VisibleForTesting
    /* package */ SessionDataHolder() {}

    public static SessionDataHolder getInstance() {
        return sInstance;
    }

    public static void setInstanceForTesting(SessionDataHolder instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    /** Data associated with a {@link SessionHandler} necessary to pass new intents to it. */
    private static class SessionData {
        public final SessionHolder session;

        // Session handlers can reside in Activities of different types, so we need to store the
        // Activity class to be able to route new intents into it.
        public final Class<? extends Activity> activityClass;

        public final WeakReference<SessionHandler> sessionHandler;

        private SessionData(
                SessionHolder session,
                Class<? extends Activity> activityClass,
                WeakReference<SessionHandler> sessionHandler) {
            this.session = session;
            this.activityClass = activityClass;
            this.sessionHandler = sessionHandler;
        }
    }

    /**
     * Sets the currently active {@link SessionHandler} in focus.
     *
     * @param sessionHandler {@link SessionHandler} to set.
     */
    public void setActiveHandler(SessionHandler sessionHandler) {
        SessionHolder session = sessionHandler.getSession();
        if (session == null) return;

        mActiveSessionHandler = sessionHandler;

        mTaskIdToSessionData.put(
                sessionHandler.getTaskId(),
                new SessionData(
                        session,
                        sessionHandler.getActivityClass(),
                        new WeakReference<>(sessionHandler)));
        ensureSessionCleanUpOnDisconnects();
    }

    /** Notifies that given {@link SessionHandler} no longer has focus. */
    public void removeActiveHandler(SessionHandler sessionHandler) {
        if (mActiveSessionHandler == sessionHandler) {
            mActiveSessionHandler = null;
        } // else this sessionHandler has already been replaced.

        // Intentionally not removing from mTaskIdToSessionData to handle cases when the task is
        // brought to foreground by a new intent - the CCT might not be able to call
        // setActiveHandler in time.
    }

    /**
     * Notifies that the given {@link SessionHandler} is being destroyed. Clears the active handler
     * and the weak reference to the handler for its task, so cross-task intent routing does not
     * dispatch to a finishing or destroyed activity before garbage collection occurs.
     *
     * <p>The task's entry (session and activity class) is intentionally kept, as in {@link
     * #removeActiveHandler}: the activity may be destroyed while its task and activity record
     * survive (e.g. "Don't keep activities"), and {@link #getActiveHandlerClassInCurrentTask} must
     * still return the activity class so a new intent is routed into that record with CLEAR_TOP |
     * SINGLE_TOP rather than stacking a new activity. Entries are removed on session disconnect.
     *
     * <p>The identity check ensures a late onDestroy() of an old instance does not affect a newer
     * handler already registered for the same task (e.g. relaunch with CLEAR_TOP, where the new
     * instance starts before the old one is destroyed).
     */
    public void removeHandlerFromTask(SessionHandler sessionHandler) {
        removeActiveHandler(sessionHandler);
        SessionData data = mTaskIdToSessionData.get(sessionHandler.getTaskId());
        if (data != null && data.sessionHandler.get() == sessionHandler) {
            data.sessionHandler.clear();
        }
    }

    /**
     * Returns the class of Activity with a matching session running in the same task as the given
     * intent is being launched from, or null if no such Activity present.
     */
    public @Nullable Class<? extends Activity> getActiveHandlerClassInCurrentTask(
            Intent intent, Context context) {
        if (!(context instanceof Activity activity)) return null;
        int taskId = activity.getTaskId();
        SessionData handlerDataInCurrentTask = mTaskIdToSessionData.get(taskId);
        if (handlerDataInCurrentTask == null
                || !handlerDataInCurrentTask.session.equals(
                        SessionHolder.getSessionHolderFromIntent(intent))) {
            return null;
        }
        return handlerDataInCurrentTask.activityClass;
    }

    /**
     * Attempts to handle an Intent.
     *
     * Checks whether an incoming intent can be handled by the current {@link SessionHandler}, and
     * if so, delegates the handling to it.
     *
     * @return Whether the active {@link SessionHandler} has handled the intent.
     */
    public boolean handleIntent(Intent intent) {
        SessionHandler handler = getActiveHandlerForIntent(intent);
        return handler != null && handler.handleIntent(intent);
    }

    /** Returns whether the given session is the currently active session. */
    public boolean isActiveSession(@Nullable SessionHolder session) {
        return getActiveHandler(session) != null;
    }

    /**
     * Returns the active session handler if it is associated with given session, null otherwise.
     */
    public @Nullable SessionHandler getActiveHandler(@Nullable SessionHolder session) {
        if (mActiveSessionHandler == null) return null;
        SessionHolder activeSession = mActiveSessionHandler.getSession();
        if (activeSession == null || !activeSession.equals(session)) return null;
        return mActiveSessionHandler;
    }

    /**
     * Returns the active session handler if it is associated with the session in the given intent,
     * null otherwise.
     */
    public @Nullable SessionHandler getActiveHandlerForIntent(Intent intent) {
        return getActiveHandler(SessionHolder.getSessionHolderFromIntent(intent));
    }

    /**
     * Returns the session handler associated with the given session, checking the active handler
     * first and then recorded handlers across tasks, or null if not found.
     *
     * <p>TWA sessions are expected to be unique per task, so at most one recorded task should match
     * a given session. Destroyed handlers are cleared via {@link #removeHandlerFromTask}; a handler
     * whose activity is finishing but not yet destroyed may still be returned. In that case {@link
     * SessionHandler#handleIntent} returns false and the caller falls back to launching a new task,
     * which is the desired behavior since the matching task is going away.
     */
    public @Nullable SessionHandler getHandlerForSession(@Nullable SessionHolder session) {
        if (session == null) return null;
        SessionHandler activeHandler = getActiveHandler(session);
        if (activeHandler != null) return activeHandler;
        for (int i = mTaskIdToSessionData.size() - 1; i >= 0; i--) {
            SessionData data = mTaskIdToSessionData.valueAt(i);
            if (session.equals(data.session)) {
                SessionHandler handler = data.sessionHandler.get();
                if (handler != null) return handler;
            }
        }
        return null;
    }

    /**
     * Returns the session handler associated with the session in the given intent, checking the
     * active handler first and then recorded handlers across tasks, or null if not found.
     */
    public @Nullable SessionHandler getHandlerForIntent(Intent intent) {
        return getHandlerForSession(SessionHolder.getSessionHolderFromIntent(intent));
    }

    /**
     * Checks whether the given referrer can be used as valid within the Activity launched with the
     * given {@link CustomTabsSessionToken}. For this to be true the token should correspond to the
     * currently in focus custom tab and also the related client should have a verified relationship
     * with the referrer origin. This can only be true for https:// origins.
     *
     * @param session The {@link SessionHolder} holding the session token specified in the activity
     *     launch intent.
     * @param referrer The referrer url that is to be used.
     * @return Whether the given referrer is a valid first party url to the client that launched the
     *     activity.
     */
    public boolean canActiveHandlerUseReferrer(@Nullable SessionHolder session, Uri referrer) {
        SessionHandler handler = getActiveHandler(session);
        return handler != null && handler.canUseReferrer(referrer);
    }

    private void ensureSessionCleanUpOnDisconnects() {
        if (mSessionDisconnectCallback != null) return;
        mSessionDisconnectCallback =
                (session) -> {
                    if (session == null) {
                        return;
                    }
                    for (int i = mTaskIdToSessionData.size() - 1; i >= 0; i--) {
                        if (session.equals(mTaskIdToSessionData.valueAt(i).session)) {
                            mTaskIdToSessionData.removeAt(i);
                        }
                    }
                };
        CustomTabsConnection.getInstance().setDisconnectCallback(mSessionDisconnectCallback);
    }
}
