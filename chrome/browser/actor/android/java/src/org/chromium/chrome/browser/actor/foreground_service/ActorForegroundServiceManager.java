// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.foreground_service;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Notification;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.VisibleForTesting;
import androidx.core.app.ServiceCompat;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorMetrics;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.actor.ActorTaskTimeoutManager;
import org.chromium.chrome.browser.actor.ActorUtils;
import org.chromium.chrome.browser.actor.notifications.ActorNotificationFactory;
import org.chromium.chrome.browser.actor.notifications.ActorNotificationService;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Profile-scoped manager for the ActorForegroundService. Observes ActorKeyedService to start/stop
 * the foreground service based on active tasks.
 */
@NullMarked
public class ActorForegroundServiceManager implements ActorKeyedService.Observer {
    private static final String TAG = "ActorFgsMngr";
    public static final int INVALID_NOTIFICATION_ID = -1;
    // Delay to ensure start/stop foreground doesn't happen too quickly.
    private static long sWaitTimeMs = TimeUnit.SECONDS.toMillis(30);

    private static @Nullable ActorForegroundServiceManager sInstance;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Runnable mMaybeStopServiceRunnable =
            new Runnable() {
                @Override
                public void run() {
                    mStopServiceDelayed = false;
                    if (!hasActiveOrPendingTasks()) {
                        if (mNotificationService != null
                                && mNotificationService.hasPendingDemotions()) {
                            postMaybeStopServiceRunnable();
                            return;
                        }
                        if (hasOptInNotification()) {
                            processTaskUpdateQueue();
                            return;
                        }
                        stopAndUnbindService();
                    }
                }
            };

    private final Set<Integer> mActiveTaskIds = new HashSet<>();
    private int mPendingTaskCount;
    private boolean mStopServiceDelayed;
    // This is true when context.bindService has been called and before context.unbindService.
    private boolean mIsServiceBound;
    // Whether startForeground() is called. Must be called shortly after service start.
    private boolean mStartForegroundCalled;

    private @Nullable ActorKeyedService mKeyedService;
    private @Nullable ActorNotificationService mNotificationService;
    private @Nullable ActorTaskTimeoutManager mTimeoutManager;
    private int mPinnedNotificationId = INVALID_NOTIFICATION_ID;
    private @Nullable Notification mPinnedNotification;
    private boolean mHasOptInNotification;
    private @Nullable Runnable mOptInCancelCallback;

    private @Nullable Runnable mStopCallbackForTesting;

    private final ProfileManager.Observer mProfileObserver;

    /** Initializes the manager and starts observing profile changes. */
    public static void initialize() {
        if (sInstance != null) return;

        sInstance = new ActorForegroundServiceManager();
        ProfileManager.addObserver(sInstance.mProfileObserver);

        // Handle profiles that were already loaded before initialization.
        for (Profile profile : ProfileManager.getLoadedProfiles()) {
            sInstance.mProfileObserver.onProfileAdded(profile);
        }
    }

    /** Returns the singleton manager instance if initialized. */
    public static @Nullable ActorForegroundServiceManager getInstance() {
        return sInstance;
    }

    @VisibleForTesting
    ActorForegroundServiceManager() {
        mProfileObserver =
                new ProfileManager.Observer() {
                    @Override
                    public void onProfileAdded(Profile profile) {
                        if (profile.isOffTheRecord()) return;
                        ActorKeyedService service = ActorKeyedServiceFactory.getForProfile(profile);
                        if (service != null) {
                            setKeyedService(service);
                        }
                    }

                    @Override
                    public void onProfileDestroyed(Profile profile) {
                        if (profile.isOffTheRecord()) return;
                        if (mKeyedService != null) {
                            setKeyedService(null);
                        }
                    }
                };
    }

    private void setKeyedService(@Nullable ActorKeyedService service) {
        if (mKeyedService != null) {
            mKeyedService.removeObserver(this);
            mKeyedService.removeObserver(ActorMetrics.getInstance());
            // If we are switching or clearing, clear state.
            mActiveTaskIds.clear();
            mPendingTaskCount = 0;
            if (hasOptInNotification()) {
                cancelOptInNotification();
            }
            if (mNotificationService != null) {
                mNotificationService.clearAll();
            }
        }
        mKeyedService = service;
        if (mKeyedService != null) {
            // ActorMetrics must be registered before `this` because it needs to observe
            // onTaskStopped and onTaskStateChanged before ActorForegroundServiceManager
            // cleans up the background session via getServiceController().onTaskCompleted().
            // Otherwise, mode detection would misclassify Foreground tasks due to early cleanup.
            mKeyedService.addObserver(ActorMetrics.getInstance());
            mKeyedService.addObserver(this);
            mPendingTaskCount = mKeyedService.getPendingTasksCount();
            if (mNotificationService == null) {
                mNotificationService = new ActorNotificationService(mKeyedService);
            }
            if (mTimeoutManager == null
                    && ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT)) {
                mTimeoutManager = new ActorTaskTimeoutManager(mKeyedService, this);
            }
        } else {
            mNotificationService = null;
            mTimeoutManager = null;
        }
        processTaskUpdateQueue();
    }

    private boolean isTimeoutManagerEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT)
                && mTimeoutManager != null;
    }

    /** Called forcefully when the Profile is destroyed. */
    public void shutDown() {
        ProfileManager.removeObserver(mProfileObserver);
        if (hasOptInNotification()) {
            cancelOptInNotification();
        }
        setKeyedService(null);
        mHandler.removeCallbacks(mMaybeStopServiceRunnable);
        if (mIsServiceBound) {
            stopAndUnbindService();
        }
    }

    /**
     * Resends loud notifications for all active working tasks (e.g. when moving to background or
     * PiP).
     */
    public void resendWorkingNotifications() {
        if (mNotificationService == null) return;
        for (int taskId : mActiveTaskIds) {
            mNotificationService.resendWorkingNotificationLoudly(taskId);
        }
    }

    /**
     * Updates the notification for the task and also process the task update queue.
     *
     * @param taskId The ID of the task.
     * @param state The current state of the task.
     */
    public void refreshTaskUI(int taskId, @ActorTaskState int state) {
        if (mNotificationService == null) return;
        mNotificationService.updateNotificationForTask(
                taskId, state, isActivityVisibleForTask(taskId), isWarningMode(taskId));
        processTaskUpdateQueue();
    }

    private boolean isWarningMode(int taskId) {
        return isTimeoutManagerEnabled() && assumeNonNull(mTimeoutManager).isWarningMode(taskId);
    }

    // ActorKeyedService.Observer implementation
    @Override
    public void onTaskStateChanged(int taskId, @ActorTaskState int newState) {
        // ActorTaskTimeoutManager processes the state change first.
        if (isTimeoutManagerEnabled()) {
            assumeNonNull(mTimeoutManager).onTaskStateChanged(taskId, newState);
        }

        // Any task that is not completed is considered active for the foreground service.
        if (!ActorUtils.isCompletedState(newState)) {
            mActiveTaskIds.add(taskId);
        } else {
            mActiveTaskIds.remove(taskId);
            getServiceController().onTaskCompleted(taskId);
            if (mPinnedNotificationId == taskId
                    && hasActiveOrPendingTasks()
                    && mNotificationService != null) {
                mNotificationService.onNotificationUnpinned(taskId);
            }
        }

        refreshTaskUI(taskId, newState);
    }

    @Override
    public void onTaskStepProgressUpdated(int taskId, String stepProgress) {
        if (!ChromeFeatureList.sActorStepProgressNotification.isEnabled()) {
            return;
        }
        if (mNotificationService == null) return;
        mNotificationService.updateNotificationForStepProgress(taskId);
        processTaskUpdateQueue();
    }

    @Override
    public void onPendingTaskCountChanged(int pendingCount) {
        mPendingTaskCount = pendingCount;
        processTaskUpdateQueue();
    }

    /**
     * Returns true if there is a visible Chrome activity that has one of the tabs, the given task
     * is acting on.
     */
    public boolean isActivityVisibleForTask(int taskId) {
        if (mNotificationService == null) return false;
        ActorTask task = mNotificationService.getTask(taskId);
        return task != null && getServiceController().isActivityVisibleForTabs(task.getTabs());
    }

    /** Returns true if there are any active tasks currently being tracked. */
    public boolean hasActiveTasks() {
        return !mActiveTaskIds.isEmpty();
    }

    /** Returns true if there are any pending tasks. */
    public boolean hasPendingTasks() {
        return mPendingTaskCount > 0;
    }

    /** Returns true if there are any active or pending tasks. */
    public boolean hasActiveOrPendingTasks() {
        return hasActiveTasks() || hasPendingTasks();
    }

    /** Returns true if an opt-in notification is currently registered to keep the service alive. */
    public boolean hasOptInNotification() {
        return mHasOptInNotification;
    }

    /**
     * Pins the opt-in notification to the foreground service so the service stays alive while
     * Chrome waits for the user to tap the notification and complete opt-in.
     *
     * @param cancelCallback Callback invoked if the opt-in notification is cancelled (e.g. via the
     *     "No thanks" action or foreground service destruction).
     */
    public void pinOptInNotification(@Nullable Runnable cancelCallback) {
        mHasOptInNotification = true;
        mOptInCancelCallback = cancelCallback;
        processTaskUpdateQueue();
    }

    /**
     * Unpins the opt-in notification, dismisses it, and stops the foreground service if no other
     * tasks or pending demotions remain.
     */
    public void unpinOptInNotification() {
        if (!mHasOptInNotification) return;
        clearOptInState();
        BaseNotificationManagerProxyFactory.create()
                .cancel(ActorNotificationFactory.OPT_IN_NOTIFICATION_ID);
        maybeStopServiceNow();
    }

    /**
     * Cancels the opt-in notification, dismissing it from the status bar, stopping the foreground
     * service if idle, and invoking the registered cancel callback.
     */
    public void cancelOptInNotification() {
        boolean hadOptIn = mHasOptInNotification;
        Runnable cancelCallback = clearOptInState();
        BaseNotificationManagerProxyFactory.create()
                .cancel(ActorNotificationFactory.OPT_IN_NOTIFICATION_ID);
        if (!hadOptIn) return;
        maybeStopServiceNow();
        if (cancelCallback != null) {
            cancelCallback.run();
        }
    }

    /**
     * Cleans up any active opt-in notification when the foreground service is being destroyed,
     * without re-entrantly stopping the service.
     */
    public void onServiceDestroyed() {
        if (!mHasOptInNotification) return;
        Runnable cancelCallback = clearOptInState();
        BaseNotificationManagerProxyFactory.create()
                .cancel(ActorNotificationFactory.OPT_IN_NOTIFICATION_ID);
        if (cancelCallback != null) {
            cancelCallback.run();
        }
    }

    private @Nullable Runnable clearOptInState() {
        mHasOptInNotification = false;
        Runnable callback = mOptInCancelCallback;
        mOptInCancelCallback = null;
        return callback;
    }

    private void maybeClearPreemptedOptInNotification(
            int notificationId, int unpinnedNotificationId) {
        if (unpinnedNotificationId == ActorNotificationFactory.OPT_IN_NOTIFICATION_ID
                || (notificationId != ActorNotificationFactory.OPT_IN_NOTIFICATION_ID
                        && hasOptInNotification()
                        && hasActiveOrPendingTasks())) {
            // TODO(crbug.com/583955325): Post the opt-in notification as a regular (unpinned)
            // notification when an active or pending task already holds the foreground service
            // instead of failing fast.
            Runnable cancelCallback = clearOptInState();
            BaseNotificationManagerProxyFactory.create()
                    .cancel(ActorNotificationFactory.OPT_IN_NOTIFICATION_ID);
            if (cancelCallback != null) {
                cancelCallback.run();
            }
        }
    }

    private boolean isTaskNotificationId(int notificationId) {
        return notificationId != INVALID_NOTIFICATION_ID
                && notificationId != ActorNotificationFactory.TASK_STARTS_SOON_NOTIFICATION_ID
                && notificationId != ActorNotificationFactory.OPT_IN_NOTIFICATION_ID;
    }

    /** Process the current task state and initiate any needed service actions. */
    @VisibleForTesting
    void processTaskUpdateQueue() {
        if (mKeyedService == null || mNotificationService == null) return;

        if (!mIsServiceBound) {
            if (!hasActiveOrPendingTasks() && !hasOptInNotification()) return;
            startAndBindService();
            return;
        }

        if (!getServiceController().isConnected()) {
            return;
        }

        if (hasActiveTasks()) {
            // Check if we are allowed to start the foreground state. Updates are always allowed
            // if the service is already in foreground.
            if (!mStartForegroundCalled && !canStartForeground()) {
                return;
            }
            mHandler.removeCallbacks(mMaybeStopServiceRunnable);
            mStopServiceDelayed = false;

            // In the future, this can be extended to handle grouping of multiple tasks.
            // For now, we only pin the first active task's notification.
            if (mActiveTaskIds.size() > 1) {
                Log.w(TAG, "Multiple active tasks detected. Only the first active task is used.");
            }
            // TODO(b/487671227): Revisit lifecycle for paused tasks. We should stop the
            // foreground service when tasks are paused.
            ActorTask currentTask = mKeyedService.getCurrentActiveTask();
            if (currentTask != null) {
                int notificationId = currentTask.getId();
                Notification notification =
                        mNotificationService.getForegroundNotification(
                                currentTask,
                                isActivityVisibleForTask(notificationId),
                                isWarningMode(notificationId));

                startOrUpdateForegroundService(notificationId, notification);
            }
        } else if (hasPendingTasks()) {
            if (!mStartForegroundCalled && !canStartForeground()) {
                return;
            }
            mHandler.removeCallbacks(mMaybeStopServiceRunnable);
            mStopServiceDelayed = false;

            if (mPinnedNotificationId
                    != ActorNotificationFactory.TASK_STARTS_SOON_NOTIFICATION_ID) {
                // Pending tasks only occur for external/experimental triggers before an ActorTask
                // is created.
                Notification placeholder =
                        ActorNotificationFactory.buildTaskStartsSoonNotification(
                                        /* isSilent= */ ChromeFeatureList
                                                .sActorSilenceNotificationsAfterFirstWorking
                                                .isEnabled())
                                .getNotification();
                startOrUpdateForegroundService(
                        ActorNotificationFactory.TASK_STARTS_SOON_NOTIFICATION_ID, placeholder);
            } else {
                maybeClearPreemptedOptInNotification(
                        mPinnedNotificationId, INVALID_NOTIFICATION_ID);
            }
        } else if (hasOptInNotification() && !mNotificationService.hasPendingDemotions()) {
            if (!mStartForegroundCalled && !canStartForeground()) {
                return;
            }
            mHandler.removeCallbacks(mMaybeStopServiceRunnable);
            mStopServiceDelayed = false;

            if (mPinnedNotificationId != ActorNotificationFactory.OPT_IN_NOTIFICATION_ID) {
                Notification notification =
                        ActorNotificationFactory.buildOptInNotification().getNotification();
                startOrUpdateForegroundService(
                        ActorNotificationFactory.OPT_IN_NOTIFICATION_ID, notification);
                if (mPinnedNotificationId == ActorNotificationFactory.OPT_IN_NOTIFICATION_ID) {
                    NotificationUmaTracker.getInstance()
                            .onNotificationShown(
                                    NotificationUmaTracker.SystemNotificationType.ACTOR,
                                    notification);
                }
            }
        } else {
            // No active or pending tasks. Update the foreground service with the latest
            // notification (e.g. Success/Failed status), or transfer the pin to the next task
            // awaiting demotion, before we wait to stop it.
            int notificationIdToPin = mPinnedNotificationId;
            if (!isTaskNotificationId(notificationIdToPin)) {
                notificationIdToPin = INVALID_NOTIFICATION_ID;
            }
            Set<Integer> pendingDemotionIds = mNotificationService.getPendingDemotionTaskIds();
            if (!pendingDemotionIds.isEmpty()
                    && !pendingDemotionIds.contains(notificationIdToPin)) {
                notificationIdToPin = pendingDemotionIds.iterator().next();
            }
            if (notificationIdToPin != INVALID_NOTIFICATION_ID) {
                Notification notification =
                        mNotificationService.getCachedNotification(
                                notificationIdToPin,
                                isActivityVisibleForTask(notificationIdToPin),
                                isWarningMode(notificationIdToPin));
                if (notification != null) {
                    startOrUpdateForegroundService(notificationIdToPin, notification);
                }
            } else if (!mStartForegroundCalled && !hasOptInNotification()) {
                mHandler.removeCallbacks(mMaybeStopServiceRunnable);
                stopAndUnbindService();
                return;
            }

            if (!mStopServiceDelayed || mNotificationService.hasPendingDemotions()) {
                postMaybeStopServiceRunnable();
            }
        }
    }

    @VisibleForTesting
    void startAndBindService() {
        mIsServiceBound = true;
        mStartForegroundCalled = false;
        try {
            getServiceController()
                    .startAndBindService(() -> mHandler.post(this::processTaskUpdateQueue));
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to start and bind foreground service.", e);
            mIsServiceBound = false;
            cancelOptInNotification();
        }
    }

    @VisibleForTesting
    void startOrUpdateForegroundService(int notificationId, @Nullable Notification notification) {
        if (notification == null
                || !getServiceController().isConnected()
                || notificationId == INVALID_NOTIFICATION_ID) {
            return;
        }

        if (mPinnedNotificationId == notificationId && mPinnedNotification == notification) {
            maybeClearPreemptedOptInNotification(notificationId, INVALID_NOTIFICATION_ID);
            return;
        }

        int unpinnedNotificationId =
                mPinnedNotificationId != notificationId
                        ? mPinnedNotificationId
                        : INVALID_NOTIFICATION_ID;

        try {
            getServiceController().startOrUpdateForegroundService(notificationId, notification);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to start or update foreground service.", e);
            cancelOptInNotification();
            return;
        }

        mStartForegroundCalled = true;
        mPinnedNotificationId = notificationId;
        mPinnedNotification = notification;

        maybeClearPreemptedOptInNotification(notificationId, unpinnedNotificationId);

        if (mNotificationService == null) return;
        if (isTaskNotificationId(notificationId)) {
            mNotificationService.onNotificationPinned(notificationId);
        }
        if (isTaskNotificationId(unpinnedNotificationId)) {
            // The swap removes the old notification, so it is reposted under the unpinned tag.
            mNotificationService.onNotificationUnpinned(unpinnedNotificationId);
            mNotificationService.repostNotification(
                    unpinnedNotificationId, isWarningMode(unpinnedNotificationId));
        }
    }

    /**
     * Stops and unbinds the service, removing the pinned notification so that it does not retain
     * foreground service flags, and reposting it as a regular notification.
     */
    @VisibleForTesting
    void stopAndUnbindService() {
        if (!mIsServiceBound) return;
        mIsServiceBound = false;

        int pinnedNotificationId = mPinnedNotificationId;
        getServiceController().stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
        getServiceController().unbindService();

        mStartForegroundCalled = false;
        mPinnedNotificationId = INVALID_NOTIFICATION_ID;
        mPinnedNotification = null;

        if (isTaskNotificationId(pinnedNotificationId) && mNotificationService != null) {
            mNotificationService.onNotificationUnpinned(pinnedNotificationId);
            mNotificationService.repostNotification(
                    pinnedNotificationId, isWarningMode(pinnedNotificationId));
        }

        if (mStopCallbackForTesting != null) {
            mStopCallbackForTesting.run();
        }
    }

    /**
     * Handles notification dismissal when the notification click intent is handled.
     *
     * @param taskId The ID of the task whose notification was dismissed.
     */
    public void onNotificationDismissed(int taskId) {
        boolean wasPinned = mPinnedNotificationId == taskId;
        if (mNotificationService != null) {
            mNotificationService.clearTaskData(taskId);
        }
        if (wasPinned) {
            mPinnedNotificationId = INVALID_NOTIFICATION_ID;
            mPinnedNotification = null;
        }
        maybeStopServiceNow();
    }

    /**
     * Stops and unbinds the foreground service if all tasks are finished and no demotions are
     * pending, removing the pinned notification and reposting it as a regular notification.
     * Otherwise, updates the pinned notification to the next task that still needs the service.
     */
    public void maybeStopServiceNow() {
        if (!hasActiveOrPendingTasks()
                && !hasOptInNotification()
                && (mNotificationService == null || !mNotificationService.hasPendingDemotions())) {
            mHandler.removeCallbacks(mMaybeStopServiceRunnable);
            stopAndUnbindService();
            return;
        }
        processTaskUpdateQueue();
    }

    private ActorForegroundServiceController getServiceController() {
        return ActorForegroundServiceController.get();
    }

    @VisibleForTesting
    void postMaybeStopServiceRunnable() {
        mHandler.removeCallbacks(mMaybeStopServiceRunnable);
        long delay =
                (mNotificationService != null && mNotificationService.hasPendingDemotions())
                        ? ActorNotificationService.getDemotionDelayMs()
                        : sWaitTimeMs;
        mHandler.postDelayed(mMaybeStopServiceRunnable, delay);
        mStopServiceDelayed = true;
    }

    /**
     * @return Whether startForeground() is allowed to be called. Required to prevent
     *     ForegroundServiceStartNotAllowedException on Android 12+.
     */
    @VisibleForTesting
    protected boolean canStartForeground() {
        if (VERSION.SDK_INT < VERSION_CODES.S) return true;
        return ApplicationStatus.hasVisibleActivities()
                || (mIsServiceBound && !mStartForegroundCalled);
    }

    boolean isServiceBoundForTesting() {
        return mIsServiceBound;
    }

    public static void resetInstanceForTesting() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (sInstance != null) {
                        ProfileManager.removeObserver(sInstance.mProfileObserver);
                        sInstance.setKeyedService(null);
                        sInstance = null;
                    }
                });
    }

    public static void setInstanceForTesting(ActorForegroundServiceManager instance) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ActorForegroundServiceManager oldInstance = sInstance;
                    if (oldInstance != null) {
                        ProfileManager.removeObserver(oldInstance.mProfileObserver);
                    }

                    sInstance = instance;
                    if (instance != null) {
                        ProfileManager.addObserver(instance.mProfileObserver);
                    }

                    ResettersForTesting.register(
                            () -> {
                                ThreadUtils.runOnUiThreadBlocking(
                                        () -> {
                                            ActorForegroundServiceManager currentInstance =
                                                    sInstance;
                                            if (currentInstance != null) {
                                                ProfileManager.removeObserver(
                                                        currentInstance.mProfileObserver);
                                            }
                                            sInstance = oldInstance;
                                            if (oldInstance != null) {
                                                ProfileManager.addObserver(
                                                        oldInstance.mProfileObserver);
                                            }
                                        });
                            });
                });
    }

    static void setWaitTimeForTesting(long ms) {
        long oldValue = sWaitTimeMs;
        sWaitTimeMs = ms;
        ResettersForTesting.register(() -> sWaitTimeMs = oldValue);
    }

    void setStopCallbackForTesting(Runnable runnable) {
        mStopCallbackForTesting = runnable;
    }

    void resetForTesting() {
        mActiveTaskIds.clear();
        mPendingTaskCount = 0;
        mStopServiceDelayed = false;
        mIsServiceBound = false;
        mStartForegroundCalled = false;
        mPinnedNotificationId = INVALID_NOTIFICATION_ID;
        mPinnedNotification = null;
        mHasOptInNotification = false;
        mOptInCancelCallback = null;
        mHandler.removeCallbacks(mMaybeStopServiceRunnable);
    }

    int getPendingTaskCountForTesting() {
        return mPendingTaskCount;
    }

    /** Returns the {@link ActorNotificationService} managed by this instance. */
    public @Nullable ActorNotificationService getNotificationService() {
        return mNotificationService;
    }

    void setNotificationServiceForTesting(ActorNotificationService service) {
        mNotificationService = service;
    }

    void setKeyedServiceForTesting(ActorKeyedService service) {
        setKeyedService(service);
    }

    void setTimeoutManagerForTesting(ActorTaskTimeoutManager timeoutManager) {
        mTimeoutManager = timeoutManager;
    }
}
