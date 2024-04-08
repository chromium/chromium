// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.concurrent.TimeUnit;

/** Manages the scheduling of Safety Hub fetch jobs. */
public class SafetyHubFetchService implements BackgroundTask {
    private static final int SAFETY_HUB_JOB_INTERVAL_IN_DAYS = 1;

    /** See {@link ChromeActivitySessionTracker#onForegroundSessionStart()}. */
    public static void onForegroundSessionStart() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB)) {
            schedulePeriodicFetchJob();
        } else {
            cancelPeriodicFetchJob();
        }
    }

    /** Schedules the fetch job to run periodically at the given interval. */
    private static void schedulePeriodicFetchJob() {
        TaskInfo.TimingInfo periodicTimingInfo =
                TaskInfo.PeriodicInfo.create()
                        .setIntervalMs(TimeUnit.DAYS.toMillis(SAFETY_HUB_JOB_INTERVAL_IN_DAYS))
                        .build();

        TaskInfo taskInfo =
                TaskInfo.createTask(TaskIds.SAFETY_HUB_JOB_ID, periodicTimingInfo)
                        .setUpdateCurrent(false)
                        .setIsPersisted(true)
                        .build();

        BackgroundTaskSchedulerFactory.getScheduler()
                .schedule(ContextUtils.getApplicationContext(), taskInfo);
    }

    private static void cancelPeriodicFetchJob() {
        BackgroundTaskSchedulerFactory.getScheduler()
                .cancel(ContextUtils.getApplicationContext(), TaskIds.SAFETY_HUB_JOB_ID);
    }

    @Override
    public boolean onStartTask(
            Context context, TaskParameters parameters, final TaskFinishedCallback callback) {
        fetchBreachedCredentialsCount(callback);
        return false;
    }

    @Override
    public boolean onStopTask(Context context, TaskParameters taskParameters) {
        // GMSCore has no mechanism to abort dispatched tasks.
        return false;
    }

    private void fetchBreachedCredentialsCount(final TaskFinishedCallback callback) {
        Profile profile = ProfileManager.getLastUsedRegularProfile();
        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(profile);
        PrefService prefService = UserPrefs.get(profile);
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        String accountEmail =
                (syncService != null)
                        ? CoreAccountInfo.getEmailFrom(syncService.getAccountInfo())
                        : null;

        if (!PasswordManagerHelper.hasChosenToSyncPasswords(syncService)
                || !passwordManagerHelper.canUseUpm()
                || accountEmail == null) {
            callback.taskFinished(/* needsReschedule= */ true);
            return;
        }

        passwordManagerHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                accountEmail,
                count -> {
                    prefService.setInteger(Pref.BREACHED_CREDENTIALS_COUNT, count);
                    callback.taskFinished(/* needsReschedule= */ false);
                },
                error -> {
                    callback.taskFinished(/* needsReschedule= */ true);
                });
    }
}
