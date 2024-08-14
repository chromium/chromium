// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
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
public class SafetyHubFetchService implements SigninManager.SignInStateObserver, Destroyable {
    interface Observer {
        void compromisedPasswordCountChanged();

        void updateStatusChanged();
    }

    private static final int SAFETY_HUB_JOB_INTERVAL_IN_DAYS = 1;
    private final Profile mProfile;

    private final Callback<UpdateStatusProvider.UpdateStatus> mUpdateCallback =
            status -> {
                mUpdateStatus = status;
                notifyUpdateStatusChanged();
            };

    /*
     * The current state of updates for Chrome. This can change during runtime and may be {@code
     * null} if the status hasn't been determined yet.
     */
    private @Nullable UpdateStatusProvider.UpdateStatus mUpdateStatus;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final SigninManager mSigninManager;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    SafetyHubFetchService(Profile profile) {
        assert profile != null;
        mProfile = profile;

        mSigninManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        if (mSigninManager != null) {
            mSigninManager.addSignInStateObserver(this);
        }

        // Fetch latest update status.
        UpdateStatusProvider.getInstance().addObserver(mUpdateCallback);
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
        boolean isSignedIn = SafetyHubUtils.isSignedIn(mProfile);
        String accountEmail = SafetyHubUtils.getAccountEmail(mProfile);

        return ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB)
                && isSignedIn
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

        passwordManagerHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                SafetyHubUtils.getAccountEmail(mProfile),
                count -> {
                    prefService.setInteger(Pref.BREACHED_CREDENTIALS_COUNT, count);
                    notifyCompromisedPasswordCountChanged();

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
            prefService.clearPref(Pref.BREACHED_CREDENTIALS_COUNT);

            cancelFetchJob();
        }
    }

    private void notifyCompromisedPasswordCountChanged() {
        for (Observer observer : mObservers) {
            observer.compromisedPasswordCountChanged();
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
    public UpdateStatusProvider.UpdateStatus getUpdateStatus() {
        return mUpdateStatus;
    }

    @Override
    public void onSignedIn() {
        scheduleOrCancelFetchJob(/* delayMs= */ 0);
    }

    @Override
    public void onSignedOut() {
        scheduleOrCancelFetchJob(/* delayMs= */ 0);
    }
}
