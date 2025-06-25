// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.concurrent.TimeUnit;

/** Manages the scheduling of Safety Hub fetch jobs. */
@NullMarked
public class SafetyHubFetchService implements SigninManager.SignInStateObserver, Destroyable {
    interface Observer {
        void accountPasswordCountsChanged();

        void localPasswordCountsChanged();

        void updateStatusChanged();
    }

    public static final int SAFETY_HUB_JOB_INTERVAL_IN_DAYS = 1;

    private final Profile mProfile;
    private final Callback<UpdateStatusProvider.UpdateStatus> mUpdateCallback =
            status -> {
                mUpdateStatus = status;
                notifyUpdateStatusChanged();
            };

    /**
     * The current state of updates for Chrome. This can change during runtime and may be {@code
     * null} if the status hasn't been determined yet.
     */
    private UpdateStatusProvider.@Nullable UpdateStatus mUpdateStatus;

    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final @Nullable SigninManager mSigninManager;

    /**
     * Passwords fetch service for account passwords. Should only be used if the user is signed-in.
     */
    private final SafetyHubPasswordsFetchService mAccountPasswordsFetchService;

    private final SafetyHubPasswordsFetchService mLocalPasswordsFetchService;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    SafetyHubFetchService(Profile profile) {
        assert profile != null;
        mProfile = profile;

        mSigninManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        if (mSigninManager != null) {
            mSigninManager.addSignInStateObserver(this);
        }

        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(mProfile);
        PrefService prefService = UserPrefs.get(mProfile);
        mAccountPasswordsFetchService =
                new SafetyHubPasswordsFetchService(
                        passwordManagerHelper,
                        prefService,
                        SafetyHubUtils.getAccountEmail(profile));

        mLocalPasswordsFetchService =
                new SafetyHubPasswordsFetchService(passwordManagerHelper, prefService, null);

        // Fetch latest update status.
        UpdateStatusProvider.getInstance().addObserver(mUpdateCallback);

        recordMetricForUnusedSitePermissionsSettingState();
    }

    /**
     * Records the metric related to the setting state of autorevoke unused site permissions. This
     * should only be recorded on start up.
     */
    private void recordMetricForUnusedSitePermissionsSettingState() {
        boolean unusedSitePermissionsRevocationEnabled =
                UserPrefs.get(mProfile).getBoolean(Pref.UNUSED_SITE_PERMISSIONS_REVOCATION_ENABLED);
        RecordHistogram.recordBooleanHistogram(
                "Settings.SafetyHub.AutorevokeUnusedSitePermissions.StateOnStartup",
                unusedSitePermissionsRevocationEnabled);
    }

    void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void destroy() {
        if (mSigninManager != null) {
            mSigninManager.removeSignInStateObserver(this);
        }

        UpdateStatusProvider.getInstance().removeObserver(mUpdateCallback);
    }

    /** See {@link ChromeActivitySessionTracker#onForegroundSessionStart()}. */
    public void onForegroundSessionStart() {
        scheduleOrCancelAccountPasswordsFetchJob(/* delayMs= */ 0);
    }

    /**
     * Schedules the account passwords fetch job to run after the given delay. If there is already a
     * pending scheduled task, then the newly requested task is dropped by the
     * BackgroundTaskScheduler. This behaviour is defined by setting updateCurrent to false.
     */
    private void scheduleAccountPasswordsFetchJobAfterDelay(long delayMs) {
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

    /** Cancels the account passwords fetch job if there is any pending. */
    private void cancelAccountPasswordsFetchJob() {
        BackgroundTaskSchedulerFactory.getScheduler()
                .cancel(ContextUtils.getApplicationContext(), TaskIds.SAFETY_HUB_JOB_ID);
    }

    /** Schedules the next account passwords fetch job to run after a delay. */
    private void scheduleNextAccountPasswordsFetchJob() {
        int nextFetchDelayInDays =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.SAFETY_HUB,
                        "background-password-check-interval-in-days",
                        SAFETY_HUB_JOB_INTERVAL_IN_DAYS);
        long nextFetchDelayMs = TimeUnit.DAYS.toMillis(nextFetchDelayInDays);

        // Cancel existing account password fetch job if it wasn't already stopped.
        cancelAccountPasswordsFetchJob();

        scheduleOrCancelAccountPasswordsFetchJob(nextFetchDelayMs);
    }

    private boolean checkConditionsForAccountPasswords() {
        boolean isSignedIn = SafetyHubUtils.isSignedIn(mProfile);
        String accountEmail = SafetyHubUtils.getAccountEmail(mProfile);

        return isSignedIn
                && accountEmail != null
                && mAccountPasswordsFetchService.canPerformFetch();
    }

    /**
     * Schedules the background account passwords fetch job to run after the given delay if the
     * conditions are met, cancels and cleans up prefs otherwise.
     */
    private void scheduleOrCancelAccountPasswordsFetchJob(long delayMs) {
        if (checkConditionsForAccountPasswords()) {
            scheduleAccountPasswordsFetchJobAfterDelay(delayMs);
        } else {
            mAccountPasswordsFetchService.clearPrefs();
            cancelAccountPasswordsFetchJob();
        }
    }

    /**
     * Triggers several calls to GMSCore to fetch the latest leaked, weak and reused credentials
     * counts for the currently signed-in profile. `onFinishedCallback` is triggered when all calls
     * to GMSCore have returned.
     */
    void fetchAccountCredentialsCount(Callback<Boolean> onFinishedCallback) {
        // TODO(crbug.com/388789824): Consider letting the fetch fail in the `PasswordFetchService`
        // instead.
        if (!checkConditionsForAccountPasswords()) {
            onFinishedCallback.onResult(/* needsReschedule */ false);
            cancelAccountPasswordsFetchJob();
            return;
        }

        Callback<Boolean> onFinishedFetchCallback =
                (errorOccurred) -> {
                    notifyAccountPasswordCountsChanged();
                    onFinishedCallback.onResult(/* needsReschedule */ errorOccurred);
                    if (!errorOccurred) {
                        scheduleNextAccountPasswordsFetchJob();
                    }
                };

        mAccountPasswordsFetchService.fetchPasswordsCount(onFinishedFetchCallback);
    }

    /**
     * Triggers several calls to GMSCore to fetch the latest leaked, weak and reused local
     * credentials counts. {@link notifyLocalPasswordCountsChanged} is triggered when all calls to
     * GMSCore have returned.
     */
    void fetchLocalCredentialsCount() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE)) {
            mLocalPasswordsFetchService.clearPrefs();
            return;
        }

        Callback<Boolean> onFinishedFetchCallback =
                (errorOccurred) -> notifyLocalPasswordCountsChanged();

        mLocalPasswordsFetchService.fetchPasswordsCount(onFinishedFetchCallback);
    }

    /**
     * Triggers a call to GMSCore to perform the account-level password checks in the background.
     * {@link notifyAccountPasswordCountsChanged} is triggered when all calls to GMSCore have
     * returned.
     *
     * @return {@code true} if the checkup will be performed by GMSCore. Otherwise, returns {@code
     *     false}, e.g. when the last checkup results are within the cool down period.
     */
    public boolean runAccountPasswordCheckup() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE)) {
            return false;
        }

        Callback<Boolean> onCheckupFinishedCallback =
                (errorOccurred) -> notifyAccountPasswordCountsChanged();

        return mAccountPasswordsFetchService.runPasswordCheckup(onCheckupFinishedCallback);
    }

    /**
     * Triggers a call to GMSCore to perform the local-level password checks in the background.
     * {@link notifyLocalPasswordCountsChanged} is triggered when all calls to GMSCore have
     * returned.
     *
     * @return {@code true} if the checkup will be performed by GMSCore. Otherwise, returns {@code
     *     false}, e.g. when the last checkup results are within the cool down period.
     */
    public boolean runLocalPasswordCheckup() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE)) {
            mLocalPasswordsFetchService.clearPrefs();
            return false;
        }

        Callback<Boolean> onCheckupFinishedCallback =
                (errorOccurred) -> notifyLocalPasswordCountsChanged();

        return mLocalPasswordsFetchService.runPasswordCheckup(onCheckupFinishedCallback);
    }

    private void notifyAccountPasswordCountsChanged() {
        for (Observer observer : mObservers) {
            observer.accountPasswordCountsChanged();
        }
    }

    private void notifyLocalPasswordCountsChanged() {
        for (Observer observer : mObservers) {
            observer.localPasswordCountsChanged();
        }
    }

    private void notifyUpdateStatusChanged() {
        for (Observer observer : mObservers) {
            observer.updateStatusChanged();
        }
    }

    /**
     * @return The last fetched update status from Omaha if available.
     */
    public UpdateStatusProvider.@Nullable UpdateStatus getUpdateStatus() {
        return mUpdateStatus;
    }

    @Override
    public void onSignedIn() {
        mAccountPasswordsFetchService.setAccount(SafetyHubUtils.getAccountEmail(mProfile));
        scheduleOrCancelAccountPasswordsFetchJob(/* delayMs= */ 0);
    }

    @Override
    public void onSignedOut() {
        scheduleOrCancelAccountPasswordsFetchJob(/* delayMs= */ 0);
    }
}
