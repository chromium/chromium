// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefService;

/** Manages fetching and setting the password information for Safety Hub. */
public class SafetyHubPasswordsFetchService {
    @NonNull private final PrefService mPrefService;
    @NonNull private final PasswordManagerHelper mPasswordManagerHelper;

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

    /** Returns true if a password fetch can be performed, namely if GMSCore can be called. */
    public boolean canPerformFetch() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB)
                && PasswordManagerUtilBridge.areMinUpmRequirementsMet()
                && mPasswordManagerHelper.canUseUpm();
    }

    public void clearPrefs() {
        mPrefService.clearPref(getBreachedPreference());
        mPrefService.clearPref(getWeakPreference());
        mPrefService.clearPref(getReusedPreference());
    }

    private String getBreachedPreference() {
        // TODO(crbug.com/388789824): Add local preference if `mAccount` is null.
        return Pref.BREACHED_CREDENTIALS_COUNT;
    }

    private String getWeakPreference() {
        // TODO(crbug.com/388789824): Add local preference if `mAccount` is null.
        return Pref.WEAK_CREDENTIALS_COUNT;
    }

    private String getReusedPreference() {
        // TODO(crbug.com/388789824): Add local preference if `mAccount` is null.
        return Pref.REUSED_CREDENTIALS_COUNT;
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
