// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

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
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

import java.util.HashSet;
import java.util.Set;

/**
 * Profile-scoped manager for the ActorForegroundService. Observes ActorKeyedService to start/stop
 * the foreground service based on active tasks.
 */
@NullMarked
public class ActorForegroundServiceManager implements ActorKeyedService.Observer {
    private static final String TAG = "ActorFgsMngr";
    public static final int INVALID_NOTIFICATION_ID = -1;
    // Delay to ensure start/stop foreground doesn't happen too quickly.
    private static long sWaitTimeMs = 200;

    @Nullable private static ActorForegroundServiceManager sInstance;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Runnable mMaybeStopServiceRunnable =
            new Runnable() {
                @Override
                public void run() {
                    mStopServiceDelayed = false;
                    if (mKeyedService == null
                            || mKeyedService.getActiveTasksCount() == 0
                            || mActiveTaskIds.isEmpty()) {
                        stopAndUnbindService();
                    }
                }
            };

    private boolean mStopServiceDelayed;
    // This is true when context.bindService has been called and before context.unbindService.
    private boolean mIsServiceBound;
    // Whether startForeground() is called. Must be called shortly after service start.
    private boolean mStartForegroundCalled;

    @Nullable private ActorKeyedService mKeyedService;
    @Nullable private ActorNotificationService mNotificationService;
    private final ActorForegroundServiceController mServiceController;
    private int mPinnedNotificationId = INVALID_NOTIFICATION_ID;
    @Nullable private Notification mPinnedNotification;
    private final Set<Integer> mActiveTaskIds = new HashSet<>();

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

    @VisibleForTesting
    ActorForegroundServiceManager() {
        mServiceController = ActorForegroundServiceController.get();
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
            // If we are switching or clearing, clear state.
            mActiveTaskIds.clear();
            if (mNotificationService != null) {
                mNotificationService.clearAll();
            }
        }
        mKeyedService = service;
        if (mKeyedService != null) {
            mKeyedService.addObserver(this);
            if (mNotificationService == null) {
                mNotificationService = new ActorNotificationService(mKeyedService);
            }
        } else {
            mNotificationService = null;
        }
        processTaskUpdateQueue();
    }

    /** Called forcefully when the Profile is destroyed. */
    public void shutDown() {
        ProfileManager.removeObserver(mProfileObserver);
        setKeyedService(null);
        mHandler.removeCallbacks(mMaybeStopServiceRunnable);
        if (mIsServiceBound) {
            stopAndUnbindService();
        }
    }

    // ActorKeyedService.Observer implementation
    @Override
    public void onTaskStateChanged(int taskId, @ActorTaskState int newState) {
        if (mNotificationService == null || mKeyedService == null) return;
        mNotificationService.updateNotificationForTask(
                taskId, newState, isActivityVisibleForTask(taskId));

        // Any task that is not completed is considered active for the foreground service.
        if (!ActorUtils.isCompletedState(newState)) {
            mActiveTaskIds.add(taskId);
        } else {
            mActiveTaskIds.remove(taskId);
        }
        processTaskUpdateQueue();
    }

    /**
     * Returns true if there is a visible Chrome activity that has one of the tabs, the given task
     * is acting on.
     */
    public boolean isActivityVisibleForTask(int taskId) {
        if (mNotificationService == null) return false;
        ActorTask task = mNotificationService.getTask(taskId);
        return task != null && mServiceController.isActivityVisibleForTabs(task.getTabs());
    }

    /** Process the current task state and initiate any needed service actions. */
    @VisibleForTesting
    void processTaskUpdateQueue() {
        if (mKeyedService == null || mNotificationService == null) return;
        int activeTaskCount = mKeyedService.getActiveTasksCount();
        boolean hasActiveTasks = activeTaskCount > 0 && !mActiveTaskIds.isEmpty();

        if (!mIsServiceBound) {
            if (!hasActiveTasks) return;
            startAndBindService();
            return;
        }

        if (!mServiceController.isConnected()) {
            return;
        }

        if (hasActiveTasks) {
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
                                currentTask, isActivityVisibleForTask(notificationId));

                startOrUpdateForegroundService(notificationId, notification);
            }
        } else {
            // No active tasks. Update the foreground service with the latest notification
            // (e.g. Success/Failed status) before we wait to stop it.
            if (mPinnedNotificationId != INVALID_NOTIFICATION_ID) {
                Notification notification =
                        mNotificationService.getCachedNotification(
                                mPinnedNotificationId,
                                isActivityVisibleForTask(mPinnedNotificationId));
                if (notification != null) {
                    startOrUpdateForegroundService(mPinnedNotificationId, notification);
                }
            }

            if (!mStopServiceDelayed) {
                postMaybeStopServiceRunnable();
            }
        }
    }

    @VisibleForTesting
    void startAndBindService() {
        mIsServiceBound = true;
        mStartForegroundCalled = false;
        mServiceController.startAndBindService(() -> mHandler.post(this::processTaskUpdateQueue));
    }

    @VisibleForTesting
    void startOrUpdateForegroundService(int notificationId, @Nullable Notification notification) {
        if (notification == null
                || !mServiceController.isConnected()
                || notificationId == INVALID_NOTIFICATION_ID) {
            return;
        }

        if (mPinnedNotificationId == notificationId && mPinnedNotification == notification) {
            return;
        }

        boolean killOldNotification =
                mPinnedNotificationId != INVALID_NOTIFICATION_ID
                        && mPinnedNotificationId != notificationId;

        mServiceController.startOrUpdateForegroundService(
                notificationId, notification, mPinnedNotificationId, killOldNotification);

        mStartForegroundCalled = true;
        mPinnedNotificationId = notificationId;
        mPinnedNotification = notification;
    }

    @VisibleForTesting
    void stopAndUnbindService() {
        if (!mIsServiceBound) return;
        mIsServiceBound = false;

        int lastNotificationId = mPinnedNotificationId;

        mServiceController.stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
        mServiceController.unbindService();

        mStartForegroundCalled = false;
        mPinnedNotificationId = INVALID_NOTIFICATION_ID;
        mPinnedNotification = null;

        if (lastNotificationId != INVALID_NOTIFICATION_ID && mNotificationService != null) {
            mNotificationService.repostNotification(lastNotificationId);
        }

        if (mStopCallbackForTesting != null) {
            mStopCallbackForTesting.run();
        }
    }

    @VisibleForTesting
    void postMaybeStopServiceRunnable() {
        mHandler.removeCallbacks(mMaybeStopServiceRunnable);
        mHandler.postDelayed(mMaybeStopServiceRunnable, sWaitTimeMs);
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

    static void setInstanceForTesting(ActorForegroundServiceManager instance) {
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
                    ActorForegroundServiceManager currentInstance = sInstance;
                    if (currentInstance != null) {
                        ProfileManager.removeObserver(currentInstance.mProfileObserver);
                    }
                    sInstance = oldInstance;
                    if (oldInstance != null) {
                        ProfileManager.addObserver(oldInstance.mProfileObserver);
                    }
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
        mStopServiceDelayed = false;
        mIsServiceBound = false;
        mStartForegroundCalled = false;
        mPinnedNotificationId = INVALID_NOTIFICATION_ID;
        mPinnedNotification = null;
        mHandler.removeCallbacks(mMaybeStopServiceRunnable);
    }

    void setNotificationServiceForTesting(ActorNotificationService service) {
        mNotificationService = service;
    }

    void setKeyedServiceForTesting(ActorKeyedService service) {
        setKeyedService(service);
    }
}
