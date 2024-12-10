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
        void passwordCountsChanged();

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

    /**
     * These booleans indicate if the specific type of credentials count has returned. They are used
     * so the callback of `fetchCredentialsCount` call is only ran once.
     */
    private boolean mBreachedCredentialsCountFetched;
    private boolean mWeakCredentialsCountFetched;
    private boolean mReusedCredentialsCountFetched;

    /**
     * Indicates if any of the credential counts has returned with an error. Used when running the
     * `fetchCredentialsCount` callback to indicate if a rescheduled is needed.
     */
    private boolean mCredentialCountError;

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
        int nextFetchDelayInDays =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.SAFETY_HUB,
                        "background-password-check-interval-in-days",
                        SAFETY_HUB_JOB_INTERVAL_IN_DAYS);
        long nextFetchDelayMs = TimeUnit.DAYS.toMillis(nextFetchDelayInDays);

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
     * Triggers several calls to GMSCore to fetch the latest leaked, weak and reused credentials
     * counts for the currently signed-in profile. `onFinishedCallback` is triggered when all calls
     * to GMSCore have returned.
     */
    void fetchCredentialsCount(Callback<Boolean> onFinishedCallback) {
        if (!checkConditions()) {
            onFinishedCallback.onResult(/* needsReschedule= */ false);
            cancelFetchJob();
            return;
        }

        mCredentialCountError = false;
        mBreachedCredentialsCountFetched = false;
        mWeakCredentialsCountFetched = false;
        mReusedCredentialsCountFetched = false;

        fetchBreachedCredentialsCount(onFinishedCallback);
        fetchWeakCredentialsCount(onFinishedCallback);
        fetchReusedCredentialsCount(onFinishedCallback);
    }

    /**
     * Makes a call to GMSCore to fetch the latest leaked credentials count for the currently
     * signed-in profile.
     */
    private void fetchBreachedCredentialsCount(Callback<Boolean> onFinishedCallback) {
        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(mProfile);
        PrefService prefService = UserPrefs.get(mProfile);

        passwordManagerHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                SafetyHubUtils.getAccountEmail(mProfile),
                count -> {
                    mBreachedCredentialsCountFetched = true;
                    prefService.setInteger(Pref.BREACHED_CREDENTIALS_COUNT, count);
                    onFetchCredentialsFinished(onFinishedCallback);
                },
                error -> {
                    mBreachedCredentialsCountFetched = true;
                    mCredentialCountError = true;
                    onFetchCredentialsFinished(onFinishedCallback);
                });
    }

    /**
     * Makes a call to GMSCore to fetch the latest weak credentials count for the currently
     * signed-in profile.
     */
    private void fetchWeakCredentialsCount(Callback<Boolean> onFinishedCallback) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)) {
            mWeakCredentialsCountFetched = true;
            onFetchCredentialsFinished(onFinishedCallback);
            return;
        }

        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(mProfile);
        PrefService prefService = UserPrefs.get(mProfile);

        passwordManagerHelper.getWeakCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                SafetyHubUtils.getAccountEmail(mProfile),
                count -> {
                    mWeakCredentialsCountFetched = true;
                    prefService.setInteger(Pref.WEAK_CREDENTIALS_COUNT, count);
                    onFetchCredentialsFinished(onFinishedCallback);
                },
                error -> {
                    mWeakCredentialsCountFetched = true;
                    mCredentialCountError = true;
                    onFetchCredentialsFinished(onFinishedCallback);
                });
    }

    /**
     * Makes a call to GMSCore to fetch the latest reused credentials count for the currently
     * signed-in profile.
     */
    private void fetchReusedCredentialsCount(Callback<Boolean> onFinishedCallback) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)) {
            mReusedCredentialsCountFetched = true;
            onFetchCredentialsFinished(onFinishedCallback);
            return;
        }

        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(mProfile);
        PrefService prefService = UserPrefs.get(mProfile);

        passwordManagerHelper.getReusedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                SafetyHubUtils.getAccountEmail(mProfile),
                count -> {
                    mReusedCredentialsCountFetched = true;
                    prefService.setInteger(Pref.REUSED_CREDENTIALS_COUNT, count);
                    onFetchCredentialsFinished(onFinishedCallback);
                },
                error -> {
                    mReusedCredentialsCountFetched = true;
                    mCredentialCountError = true;
                    onFetchCredentialsFinished(onFinishedCallback);
                });
    }

    private void onFetchCredentialsFinished(Callback<Boolean> onFinishedCallback) {
        if (!mBreachedCredentialsCountFetched
                || !mWeakCredentialsCountFetched
                || !mReusedCredentialsCountFetched) {
            return;
        }

        notifyPasswordCountsChanged();
        onFinishedCallback.onResult(/* needsReschedule= */ mCredentialCountError);
        if (!mCredentialCountError) {
            scheduleNextFetchJob();
        }
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
            prefService.clearPref(Pref.WEAK_CREDENTIALS_COUNT);
            prefService.clearPref(Pref.REUSED_CREDENTIALS_COUNT);

            cancelFetchJob();
        }
    }

    private void notifyPasswordCountsChanged() {
        for (Observer observer : mObservers) {
            observer.passwordCountsChanged();
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
