// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;

/** Manages fetching and setting the password information for Safety Hub. */
@NullMarked
public class SafetyHubPasswordsFetchService {
    private static final long CHECKUP_COOL_DOWN_PERIOD_IN_MS =
            60 * TimeUtils.MILLISECONDS_PER_MINUTE;

    private final PrefService mPrefService;
    private final PasswordManagerHelper mPasswordManagerHelper;
    private final AccountManagerFacade mAccountManagerFacade;

    /**
     * These booleans indicate if the specific type of passwords count has returned. They are used
     * so the callback of `fetchPasswordsCount` call is only ran once.
     */
    private boolean mBreachedPasswordsCountFetched;

    private boolean mWeakPasswordsCountFetched;
    private boolean mReusedPasswordsCountFetched;

    /**
     * Indicates if any of the credential counts has returned with an error. Used when running the
     * `fetchPasswordsCount` callback to indicate if a rescheduled is needed.
     */
    private boolean mCredentialCountError;

    /**
     * Account for the password checkups or to fetch password counts from GMSCore. Is null if this
     * service is fetching local passwords information.
     */
    private @Nullable String mAccount;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    SafetyHubPasswordsFetchService(
            PasswordManagerHelper passwordManagerHelper,
            PrefService prefService,
            @Nullable String account) {
        mPasswordManagerHelper = passwordManagerHelper;
        mPrefService = prefService;
        mAccount = account;
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
    }

    public void setAccount(@Nullable String account) {
        mAccount = account;
    }

    /**
     * Triggers several calls to GMSCore to fetch the latest leaked, weak and reused passwords
     * counts for `mAccount`. `onFinishedCallback` is triggered when all calls to GMSCore have
     * returned.
     */
    public void fetchPasswordsCount(Callback<Boolean> onFinishedCallback) {
        if (!canPerformFetch()) {
            clearPrefs();
            onFinishedCallback.onResult(/* errorOccurred */ true);
            return;
        }

        mCredentialCountError = false;
        mBreachedPasswordsCountFetched = false;
        mWeakPasswordsCountFetched = false;
        mReusedPasswordsCountFetched = false;

        fetchBreachedPasswordsCount(onFinishedCallback);
        fetchWeakPasswordsCount(onFinishedCallback);
        fetchReusedPasswordsCount(onFinishedCallback);
    }

    /**
     * Makes a call to GMSCore to perform a password checkup in the background for `mAccount`. It
     * also triggers several calls to GMSCore to fetch the compromised, weak and reuse password
     * counts.
     *
     * <p>The password checkup has a cool down period of one hour. In other words, if the client has
     * made a password checkup call to GMSCore for this account in the last hour, then it assumes
     * the results are still fresh and they can be reused.
     *
     * <p>{@code onFinishedCallback} runs either: (1) on success, when all password counts have
     * successfully been returned and the appropriate preferences have been updated with the
     * results; or (2) if any error has occurred either when running the checkup or fetching the
     * counts.
     *
     * @return {@code true} if the checkup will be performed by GMSCore. Otherwise, returns {@code
     *     false}, e.g. when the last checkup results are within the cool down period.
     */
    public boolean runPasswordCheckup(Callback<Boolean> onFinishedCallback) {
        if (!canPerformFetch()) {
            clearPrefs();
            onFinishedCallback.onResult(/* errorOccurred */ true);
            return false;
        }

        boolean inCoolDownPeriod = getTimeSinceLastCheckupInMs() <= CHECKUP_COOL_DOWN_PERIOD_IN_MS;

        if (inCoolDownPeriod) {
            onFinishedCallback.onResult(/* errorOccurred */ false);
            return false;
        }

        if (noAccountsOnDevice()) {
            clearPrefsIfCheckUpLongAgo();
            onFinishedCallback.onResult(/* errorOccurred */ true);
            return false;
        }

        mPasswordManagerHelper.runPasswordCheckupInBackground(
                PasswordCheckReferrer.SAFETY_CHECK,
                mAccount,
                success -> {
                    mPrefService.setLong(
                            getLastTimeInMsCheckCompletedPreference(),
                            TimeUtils.currentTimeMillis());
                    fetchPasswordsCount(onFinishedCallback);
                },
                error -> {
                    clearPrefsIfCheckUpLongAgo();
                    onFinishedCallback.onResult(/* errorOccurred */ true);
                });

        return true;
    }

    /** Returns true if a password fetch can be performed, namely if GMSCore can be called. */
    public boolean canPerformFetch() {
        return PasswordManagerUtilBridge.isPasswordManagerAvailable();
    }

    public void clearPrefs() {
        mPrefService.clearPref(getBreachedPreference());
        mPrefService.clearPref(getWeakPreference());
        mPrefService.clearPref(getReusedPreference());
        if (mAccount == null) {
            mPrefService.clearPref(getLastTimeInMsCheckCompletedPreference());
        }
    }

    /**
     * Clears the count preferences if the last check up was performed a long time ago, so they
     * don't get reused.
     */
    private void clearPrefsIfCheckUpLongAgo() {
        if (getTimeSinceLastCheckupInMs()
                > (SafetyHubFetchService.SAFETY_HUB_JOB_INTERVAL_IN_DAYS
                        * TimeUtils.MILLISECONDS_PER_DAY)) {
            clearPrefs();
        }
    }

    /**
     * Returns true if there are no accounts on device. If there are accounts on the device or if
     * there isn't yet information about accounts on the device, returns false.
     */
    private boolean noAccountsOnDevice() {
        if (!mAccountManagerFacade.getAccounts().isFulfilled()
                || !mAccountManagerFacade.didAccountFetchSucceed()) {
            return false;
        }

        return mAccountManagerFacade.getAccounts().getResult().isEmpty();
    }

    private long getTimeSinceLastCheckupInMs() {
        // TODO(crbug.com/420659257): Migrate to GMSCore API for the timestamp of the last check
        // when it's available.
        return TimeUtils.currentTimeMillis()
                - mPrefService.getLong(getLastTimeInMsCheckCompletedPreference());
    }

    private String getBreachedPreference() {
        return mAccount == null
                ? Pref.LOCAL_BREACHED_CREDENTIALS_COUNT
                : Pref.BREACHED_CREDENTIALS_COUNT;
    }

    private String getWeakPreference() {
        return mAccount == null ? Pref.LOCAL_WEAK_CREDENTIALS_COUNT : Pref.WEAK_CREDENTIALS_COUNT;
    }

    private String getReusedPreference() {
        return mAccount == null
                ? Pref.LOCAL_REUSED_CREDENTIALS_COUNT
                : Pref.REUSED_CREDENTIALS_COUNT;
    }

    private String getLastTimeInMsCheckCompletedPreference() {
        return mAccount == null
                ? Pref.LAST_TIME_IN_MS_LOCAL_PASSWORD_CHECK_COMPLETED
                : Pref.LAST_TIME_IN_MS_ACCOUNT_PASSWORD_CHECK_COMPLETED;
    }

    /** Makes a call to GMSCore to fetch the latest leaked passwords count for `mAccount`. */
    private void fetchBreachedPasswordsCount(Callback<Boolean> onFinishedCallback) {
        mPasswordManagerHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                mAccount,
                count -> {
                    mBreachedPasswordsCountFetched = true;
                    mPrefService.setInteger(getBreachedPreference(), count);
                    onFetchPasswordsFinished(onFinishedCallback);
                },
                error -> {
                    mBreachedPasswordsCountFetched = true;
                    mCredentialCountError = true;
                    onFetchPasswordsFinished(onFinishedCallback);
                });
    }

    /** Makes a call to GMSCore to fetch the latest weak passwords count for `mAccount`. */
    private void fetchWeakPasswordsCount(Callback<Boolean> onFinishedCallback) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)) {
            mWeakPasswordsCountFetched = true;
            onFetchPasswordsFinished(onFinishedCallback);
            return;
        }

        mPasswordManagerHelper.getWeakCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                mAccount,
                count -> {
                    mWeakPasswordsCountFetched = true;
                    mPrefService.setInteger(getWeakPreference(), count);
                    onFetchPasswordsFinished(onFinishedCallback);
                },
                error -> {
                    mWeakPasswordsCountFetched = true;
                    mCredentialCountError = true;
                    onFetchPasswordsFinished(onFinishedCallback);
                });
    }

    /** Makes a call to GMSCore to fetch the latest reused passwords count for `mAccount`. */
    private void fetchReusedPasswordsCount(Callback<Boolean> onFinishedCallback) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)) {
            mReusedPasswordsCountFetched = true;
            onFetchPasswordsFinished(onFinishedCallback);
            return;
        }

        mPasswordManagerHelper.getReusedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                mAccount,
                count -> {
                    mReusedPasswordsCountFetched = true;
                    mPrefService.setInteger(getReusedPreference(), count);
                    onFetchPasswordsFinished(onFinishedCallback);
                },
                error -> {
                    mReusedPasswordsCountFetched = true;
                    mCredentialCountError = true;
                    onFetchPasswordsFinished(onFinishedCallback);
                });
    }

    /**
     * Notifies the caller by running the `onFinishedCallback` if all passwords counts have
     * returned.
     */
    private void onFetchPasswordsFinished(Callback<Boolean> onFinishedCallback) {
        if (!mBreachedPasswordsCountFetched
                || !mWeakPasswordsCountFetched
                || !mReusedPasswordsCountFetched) {
            return;
        }

        onFinishedCallback.onResult(/* errorOccurred */ mCredentialCountError);
    }
}
