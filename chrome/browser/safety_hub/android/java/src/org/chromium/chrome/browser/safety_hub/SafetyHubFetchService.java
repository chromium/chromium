// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.concurrent.TimeUnit;

/** Manages the scheduling of Safety Hub fetch jobs. */
public class SafetyHubFetchService implements SyncService.SyncStateChangedListener, Destroyable {
    private static final int SAFETY_HUB_JOB_INTERVAL_IN_DAYS = 1;
    private final Profile mProfile;

    private final Callback<UpdateStatusProvider.UpdateStatus> mUpdateCallback =
            status -> {
                mUpdateStatus = status;
            };

    /*
     * The current state of updates for Chrome. This can change during runtime and may be {@code
     * null} if the status hasn't been determined yet.
     */
    private @Nullable UpdateStatusProvider.UpdateStatus mUpdateStatus;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    SafetyHubFetchService(Profile profile) {
        assert profile != null;
        mProfile = profile;

        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (syncService != null) {
            syncService.addSyncStateChangedListener(this);
        }

        // Fetch latest update status.
        UpdateStatusProvider.getInstance().addObserver(mUpdateCallback);
    }

    @Override
    public void destroy() {
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (syncService != null) {
            syncService.removeSyncStateChangedListener(this);
        }

        UpdateStatusProvider.getInstance().removeObserver(mUpdateCallback);
    }

    /** See {@link ChromeActivitySessionTracker#onForegroundSessionStart()}. */
    public void onForegroundSessionStart() {
        scheduleOrCancelFetchJob(/* delayMs= */ 0);
    }

    /**
     * Schedules the fetch job to run after the given delay. If there is already a pending scheduled
     * task, then the newly requested task is dropped by the BackgroundTaskScheduler. This behaviour
     * is defined by setting updateCurrent to false.
     */
    private void scheduleFetchJobAfterDelay(long delayMs) {
        TaskInfo.TimingInfo oneOffTimingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowStartTimeMs(delayMs)
                        .setWindowEndTimeMs(delayMs)
                        .build();

        TaskInfo taskInfo =
                TaskInfo.createTask(TaskIds.SAFETY_HUB_JOB_ID, oneOffTimingInfo)
                        .setUpdateCurrent(false)
                        .setIsPersisted(true)
                        .build();

        BackgroundTaskSchedulerFactory.getScheduler()
                .schedule(ContextUtils.getApplicationContext(), taskInfo);
    }

    /** Cancels the fetch job if there is any pending. */
    private void cancelFetchJob() {
        BackgroundTaskSchedulerFactory.getScheduler()
                .cancel(ContextUtils.getApplicationContext(), TaskIds.SAFETY_HUB_JOB_ID);
    }

    /** Schedules the next fetch job to run after a delay. */
    private void scheduleNextFetchJob() {
        long nextFetchDelayMs = TimeUnit.DAYS.toMillis(SAFETY_HUB_JOB_INTERVAL_IN_DAYS);

        // Cancel existing job if it wasn't already stopped.
        cancelFetchJob();

        scheduleOrCancelFetchJob(nextFetchDelayMs);
    }

    private boolean checkConditions() {
        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(mProfile);
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        String accountEmail =
                (syncService != null)
                        ? CoreAccountInfo.getEmailFrom(syncService.getAccountInfo())
                        : null;

        return ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB)
                && PasswordManagerHelper.hasChosenToSyncPasswords(syncService)
                && PasswordManagerUtilBridge.areMinUpmRequirementsMet()
                && passwordManagerHelper.canUseUpm()
                && accountEmail != null;
    }

    /**
     * Makes a call to GMSCore to fetch the latest leaked credentials count for the currently
     * syncing profile.
     */
    void fetchBreachedCredentialsCount(Callback<Boolean> onFinishedCallback) {
        if (!checkConditions()) {
            onFinishedCallback.onResult(/* needsReschedule= */ false);
            cancelFetchJob();
            return;
        }

        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(mProfile);
        PrefService prefService = UserPrefs.get(mProfile);
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);

        assert syncService != null;
        String accountEmail = CoreAccountInfo.getEmailFrom(syncService.getAccountInfo());

        passwordManagerHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                accountEmail,
                count -> {
                    prefService.setInteger(Pref.BREACHED_CREDENTIALS_COUNT, count);
                    onFinishedCallback.onResult(/* needsReschedule= */ false);
                    scheduleNextFetchJob();
                },
                error -> {
                    onFinishedCallback.onResult(/* needsReschedule= */ true);
                });
    }

    /**
     * Schedules the background fetch job to run after the given delay if the conditions are met,
     * cancels and cleans up prefs otherwise.
     */
    private void scheduleOrCancelFetchJob(long delayMs) {
        if (checkConditions()) {
            scheduleFetchJobAfterDelay(delayMs);
        } else {
            // Clean up account specific prefs.
            PrefService prefService = UserPrefs.get(mProfile);
            prefService.setInteger(Pref.BREACHED_CREDENTIALS_COUNT, 0);

            cancelFetchJob();
        }
    }

    /**
     * @return The last fetched update status from Omaha if available.
     */
    public UpdateStatusProvider.UpdateStatus getUpdateStatus() {
        return mUpdateStatus;
    }

    @Override
    public void syncStateChanged() {
        scheduleOrCancelFetchJob(/* delayMs= */ 0);
    }
}
