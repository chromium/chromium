// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.SparseArray;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;

import javax.inject.Inject;
import javax.inject.Singleton;

import dagger.Lazy;

/**
 * Holds the currently active {@link SessionHandler} and redirects relevant intents
 * and calls into it. {@link SessionHandler} is an interface owned by the currently
 * focused activity that has a linkage to a third party client app through a session.
 */
@Singleton
public class SessionDataHolder {
    private final Lazy<CustomTabsConnection> mConnection;
    private final SparseArray<SessionData> mTaskIdToSessionData = new SparseArray<>();

    @Nullable
    private SessionHandler mActiveSessionHandler;

    @Nullable
    private Callback<CustomTabsSessionToken> mSessionDisconnectCallback;

    @Inject
    public SessionDataHolder(Lazy<CustomTabsConnection> connection) {
        mConnection = connection;
    }

    /**
     * Data associated with a {@link SessionHandler} necessary to pass new intents to it.
     */
    private static class SessionData {
        public final CustomTabsSessionToken session;

        // Session handlers can reside in Activities of different types, so we need to store the
        // Activity class to be able to route new intents into it.
        public final Class<? extends Activity> activityClass;

        private SessionData(CustomTabsSessionToken session,
                Class<? extends Activity> activityClass) {
            this.session = session;
            this.activityClass = activityClass;
        }
    }

    /**
     * Sets the currently active {@link SessionHandler} in focus.
     * @param sessionHandler {@link SessionHandler} to set.
     */
    public void setActiveHandler(@NonNull SessionHandler sessionHandler) {
        mActiveSessionHandler = sessionHandler;
        CustomTabsSessionToken session = sessionHandler.getSession();
        if (session == null) return;

        mTaskIdToSessionData.append(sessionHandler.getTaskId(),
                new SessionData(session, sessionHandler.getActivityClass()));
        ensureSessionCleanUpOnDisconnects();
    }

    /**
     * Notifies that given {@link SessionHandler} no longer has focus.
     */
    public void removeActiveHandler(SessionHandler sessionHandler) {
        if (mActiveSessionHandler == sessionHandler) {
            mActiveSessionHandler = null;
        } // else this sessionHandler has already been replaced.

        // Intentionally not removing from mTaskIdToSessionData to handle cases when the task is
        // brought to foreground by a new intent - the CCT might not be able to call
        // setActiveHandler in time.
    }

    /**
     * Returns the class of Activity with a matching session running in the same task as the given
     * intent is being launched from, or null if no such Activity present.
     */
    @Nullable
    public Class<? extends Activity> getActiveHandlerClassInCurrentTask(Intent intent,
            Context context) {
        if (!(context instanceof Activity)) return null;
        int taskId = ((Activity) context).getTaskId();
        SessionData handlerDataInCurrentTask = mTaskIdToSessionData.get(taskId);
        if (handlerDataInCurrentTask == null || !handlerDataInCurrentTask.session.equals(
                    CustomTabsSessionToken.getSessionTokenFromIntent(intent))) {
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

    /**
     * Returns whether the given session is the currently active session.
     */
    public boolean isActiveSession(@Nullable CustomTabsSessionToken session) {
        return getActiveHandler(session) != null;
    }

    /**
     * Returns the active session handler if it is associated with given session, null otherwise.
     */
    @Nullable
    public SessionHandler getActiveHandler(@Nullable CustomTabsSessionToken session) {
        if (mActiveSessionHandler == null) return null;
        CustomTabsSessionToken activeSession = mActiveSessionHandler.getSession();
        if (activeSession == null || !activeSession.equals(session)) return null;
        return mActiveSessionHandler;
    }

    @Nullable
    private SessionHandler getActiveHandlerForIntent(Intent intent) {
        return getActiveHandler(CustomTabsSessionToken.getSessionTokenFromIntent(intent));
    }

    /**
     * Checks whether the given referrer can be used as valid within the Activity launched by the
     * given intent. For this to be true, the intent should be for a {@link CustomTabsSessionToken}
     * that is the currently in focus custom tab and also the related client should have a verified
     * relationship with the referrer origin. This can only be true for https:// origins.
     *
     * @param intent The intent that was used to launch the Activity in question.
     * @param referrer The referrer url that is to be used.
     * @return Whether the given referrer is a valid first party url to the client that launched
     *         the activity.
     */
    public boolean canActiveHandlerUseReferrer(Intent intent, Uri referrer) {
        SessionHandler handler = getActiveHandlerForIntent(intent);
        return handler != null && handler.canUseReferrer(referrer);
    }

    /**
     * @return The url for the page displayed using the current {@link
     * SessionHandler}.
     *
     * @deprecated This will be removed once downstream usages change.
     */
    @Deprecated
    public String getCurrentUrlForActiveBrowserSession() {
        if (mActiveSessionHandler == null) return null;
        return mActiveSessionHandler.getCurrentUrl();
    }

    /**
     * @return The pending url for the page about to be displayed using the current {@link
     * SessionHandler}.
     *
     * @deprecated This will be removed once downstream usages change.
     */
    @Deprecated
    public String getPendingUrlForActiveBrowserSession() {
        if (mActiveSessionHandler == null) return null;
        return mActiveSessionHandler.getPendingUrl();
    }

    private void ensureSessionCleanUpOnDisconnects() {
        if (mSessionDisconnectCallback != null) return;
        mSessionDisconnectCallback = (session) -> {
            if (session == null) {
                return;
            }
            for (int i = 0; i < mTaskIdToSessionData.size(); i++) {
                if (session.equals(mTaskIdToSessionData.valueAt(i).session)) {
                    mTaskIdToSessionData.removeAt(i);
                }
            }
        };
        mConnection.get().setDisconnectCallback(mSessionDisconnectCallback);
    }
}
