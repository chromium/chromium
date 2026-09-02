// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Activity;
import android.app.Notification;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;
import java.util.Set;

/** Android implementation of {@link ActorForegroundServiceController}. */
@NullMarked
@ServiceImpl(ActorForegroundServiceController.class)
public class ActorForegroundServiceControllerImpl implements ActorForegroundServiceController {
    private static final String TAG = "ActorFgsController";
    private @Nullable ActorBackgroundActuationManager mBackgroundActuationManager;
    private @Nullable Runnable mOnConnectedRunnable;
    private @Nullable ActorForegroundServiceImpl mBoundService;

    private final ServiceConnection mConnection =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName className, IBinder service) {
                    if (service instanceof ActorForegroundServiceImpl.LocalBinder binder) {
                        mBoundService = binder.getService();
                    } else {
                        Log.w(TAG, "Unexpected binder type.");
                    }

                    if (mOnConnectedRunnable != null) {
                        mOnConnectedRunnable.run();
                    }
                }

                @Override
                public void onServiceDisconnected(ComponentName componentName) {
                    if (BuildConfig.ENABLE_ASSERTS) {
                        Log.i(TAG, "Service disconnected: " + componentName);
                    }
                    mBoundService = null;
                    destroyBackgroundActuationManager();
                }
            };

    @Override
    public void startService(String glicTriggerMessageId) {
        ensureBackgroundActuationManagerCreated();
        Context context = ContextUtils.getApplicationContext();
        ActorForegroundServiceImpl.startActorForegroundServiceWithGlicTriggerMessageId(
                context, glicTriggerMessageId);
    }

    @Override
    public void startAndBindService(Runnable onConnected) {
        ensureBackgroundActuationManagerCreated();
        mOnConnectedRunnable = onConnected;
        Context context = ContextUtils.getApplicationContext();
        ActorForegroundServiceImpl.startActorForegroundService(context);
        Intent intent = new Intent(context, ActorForegroundService.class);
        context.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
    }

    @Override
    public void unbindService() {
        ContextUtils.getApplicationContext().unbindService(mConnection);
        mBoundService = null;
        mOnConnectedRunnable = null;
        destroyBackgroundActuationManager();
    }

    @Override
    public boolean isConnected() {
        return mBoundService != null;
    }

    @Override
    public void startOrUpdateForegroundService(
            int newNotificationId,
            Notification newNotification,
            int oldNotificationId,
            boolean killOldNotification) {
        if (mBoundService == null) {
            if (BuildConfig.ENABLE_ASSERTS) {
                Log.i(TAG, "Cannot update foreground service: not connected.");
            }
            return;
        }
        ensureBackgroundActuationManagerCreated();
        mBoundService.startOrUpdateForegroundService(
                newNotificationId, newNotification, oldNotificationId, killOldNotification);
    }

    @Override
    public void stopActorForegroundService(int flags) {
        if (mBoundService == null) {
            if (BuildConfig.ENABLE_ASSERTS) {
                Log.w(TAG, "Cannot stop foreground service: not connected.");
            }
            return;
        }
        mBoundService.stopActorForegroundService(flags);
    }

    @Override
    public void transitionActiveTasksToBackground(TabModelSelector selector) {
        ThreadUtils.assertOnUiThread();
        if (mBoundService == null) return;
        assert mBackgroundActuationManager != null;
        mBackgroundActuationManager.transitionActiveTasksToBackground(selector);
    }

    @Override
    public void restoreActiveWindowBackgroundTabs(
            TabModelSelector selector,
            WindowAndroid window,
            TabDelegateFactory tabDelegateFactory) {
        ThreadUtils.assertOnUiThread();
        if (mBackgroundActuationManager == null) return;
        int activeWindowId =
                TabWindowManagerSingleton.getInstance().getWindowIdForSelector(selector);
        if (activeWindowId == TabWindowManager.INVALID_WINDOW_ID) return;

        List<BackgroundSession> sessionsToRemove =
                ActorTabStateHelper.restoreActiveWindowBackgroundTabs(
                        selector,
                        activeWindowId,
                        window,
                        mBackgroundActuationManager.getBackgroundSessions(),
                        tabDelegateFactory);
        mBackgroundActuationManager.removeBackgroundSessions(sessionsToRemove);
    }

    @Override
    public void destroyBackgroundActuationManager() {
        if (mBackgroundActuationManager != null) {
            mBackgroundActuationManager.destroy();
            mBackgroundActuationManager = null;
        }
    }

    @Override
    public void provisionBackgroundTabForTask(
            Profile profile, int taskId, Callback<@Nullable Tab> callback) {
        ThreadUtils.assertOnUiThread();
        assert mBackgroundActuationManager != null;
        mBackgroundActuationManager.provisionBackgroundTabForTask(profile, taskId, callback);
    }

    @Override
    public @Nullable Intent createTrustedBringTabToFrontIntent(ActorTask task) {
        int tabId = task.getLastActuatedTabId();

        Intent intent =
                IntentHandler.createTrustedBringTabToFrontIntent(
                        tabId, IntentHandler.BringToFrontSource.NOTIFICATION);
        intent.putExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, true);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, task.getId());
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, task.getState());
        return intent;
    }

    @Override
    public boolean isActivityVisibleForTabs(Set<Integer> tabIds) {
        ThreadUtils.assertOnUiThread();
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();

        // An activity is considered visible for a task if it is a ChromeTabbedActivity that is not
        // in PiP. Since we cannot directly reference ChromeTabbedActivity, we use the
        // TabModelSelector to determine relevance: the activity must host at least one of the
        // task's tabs. If the task is generic (has no tabs), it is considered not visible.
        if (!(activity instanceof AsyncInitializationActivity asyncInitActivity)) return false;
        if (asyncInitActivity.isInPictureInPictureMode()) return false;
        if (asyncInitActivity.isFinishing() || asyncInitActivity.isDestroyed()) return false;

        TabModelSelector selector =
                TabModelSelectorSupplier.getValueOrNullFrom(asyncInitActivity.getWindowAndroid());
        if (selector == null || selector.isIncognitoBrandedModelSelected()) return false;

        // If the user is on the NTP, the user has no visible indicators that there is an active
        // task. So, this is treated as not visible.
        Tab currentTab = selector.getCurrentTab();
        if (currentTab != null && UrlUtilities.isNtpUrl(currentTab.getUrl())) return false;

        // TODO(crbug.com/494093802): When the task first starts and ends, getTabs is empty. This
        // check is neccesarry to ensure that we have silent notifications when the user is on the
        // CTA that the task is being performed on.
        if (tabIds.isEmpty()) return true;

        for (int tabId : tabIds) {
            if (selector.getTabById(tabId) != null) return true;
        }

        return false;
    }

    @Override
    public boolean isTabbedActivityVisible() {
        ThreadUtils.assertOnUiThread();
        int windowId = MultiWindowUtils.getInstanceIdForViewIntent();
        if (windowId == TabWindowManager.INVALID_WINDOW_ID) return false;

        Activity activity = MultiWindowUtils.getActivityById(windowId);
        if (activity == null) return false;

        // Safe to cast as getInstanceIdForViewIntent() only returns ChromeTabbedActivity IDs.
        AsyncInitializationActivity asyncActivity = (AsyncInitializationActivity) activity;
        if (asyncActivity.isInPictureInPictureMode()) return false;
        if (asyncActivity.isFinishing() || asyncActivity.isDestroyed()) return false;

        int state = ApplicationStatus.getStateForActivity(asyncActivity);
        return state == ActivityState.STARTED || state == ActivityState.RESUMED;
    }

    @Override
    public void onMessageTriggerTaskStopped(String contextId) {
        // If a background session is active for the task and no associated actor task yet,
        // it needs to be cleaned up here.
        if (mBackgroundActuationManager != null) {
            mBackgroundActuationManager.cleanupContext(contextId);
        }
    }

    @Override
    public void onTaskCompleted(int taskId) {
        ThreadUtils.assertOnUiThread();
        if (mBackgroundActuationManager != null) {
            ActorTabStateHelper.persistTabsForCompletedTask(
                    mBackgroundActuationManager.getBackgroundSessions(), taskId);
            mBackgroundActuationManager.onTaskCompleted(taskId);
        }
    }

    public @Nullable ActorBackgroundActuationManager getBackgroundActuationManager() {
        return mBackgroundActuationManager;
    }

    public void setBackgroundManagerForTesting(
            @Nullable ActorBackgroundActuationManager backgroundManager) {
        mBackgroundActuationManager = backgroundManager;
    }

    public ServiceConnection getServiceConnectionForTesting() {
        return mConnection;
    }

    private void ensureBackgroundActuationManagerCreated() {
        if (mBackgroundActuationManager == null) {
            mBackgroundActuationManager = new ActorBackgroundActuationManager();
        }
    }
}
