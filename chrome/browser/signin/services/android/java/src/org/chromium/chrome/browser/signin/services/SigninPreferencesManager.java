// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.util.Set;

/**
 * SigninPreferencesManager stores the state of SharedPreferences related to account sign-in.
 */
public class SigninPreferencesManager {
    private static final SigninPreferencesManager INSTANCE = new SigninPreferencesManager();

    private final SharedPreferencesManager mManager;

    private SigninPreferencesManager() {
        mManager = SharedPreferencesManager.getInstance();
    }

    /**
     * @return the SignInPromoStore singleton
     */
    public static SigninPreferencesManager getInstance() {
        return INSTANCE;
    }

    /**
     * Sets the {@link ChromePreferenceKeys#SIGNIN_ACCOUNTS_CHANGED} to true.
     */
    public void markAccountsChangedPref() {
        // The process may go away as soon as we return from onReceive but Android makes sure
        // that in-flight disk writes from apply() complete before changing component states.
        mManager.writeBoolean(ChromePreferenceKeys.SIGNIN_ACCOUNTS_CHANGED, true);
    }

    /**
     * @return The new account name of the current user. Null if it wasn't renamed.
     */
    public String getNewSignedInAccountName() {
        return mManager.readString(ChromePreferenceKeys.SIGNIN_ACCOUNT_RENAMED, null);
    }

    /**
     * Sets the new account name of the current user.
     *
     * @param newName the new name to write
     */
    public void setNewSignedInAccountName(@Nullable String newName) {
        mManager.writeString(ChromePreferenceKeys.SIGNIN_ACCOUNT_RENAMED, newName);
    }

    /**
     * Clears the new account name of the current user.
     */
    public void clearNewSignedInAccountName() {
        setNewSignedInAccountName(null);
    }

    /**
     * Sets the last read index of all the account changed events of the current signed in account.
     *
     * @param newIndex the new index to write
     */
    public void setLastAccountChangedEventIndex(int newIndex) {
        mManager.writeInt(ChromePreferenceKeys.SIGNIN_ACCOUNT_RENAME_EVENT_INDEX, newIndex);
    }

    /**
     * @return the last read index of all the account changed events of the current signed in
     *         account.
     */
    public int getLastAccountChangedEventIndex() {
        return mManager.readInt(ChromePreferenceKeys.SIGNIN_ACCOUNT_RENAME_EVENT_INDEX);
    }

    /**
     * Gets the state of {@link ChromePreferenceKeys#SIGNIN_ACCOUNTS_CHANGED} and clears it.
     *
     * @return the state of {@link ChromePreferenceKeys#SIGNIN_ACCOUNTS_CHANGED} before the call.
     */
    public boolean checkAndClearAccountsChangedPref() {
        if (mManager.readBoolean(ChromePreferenceKeys.SIGNIN_ACCOUNTS_CHANGED, false)) {
            // Clear the value in prefs.
            mManager.writeBoolean(ChromePreferenceKeys.SIGNIN_ACCOUNTS_CHANGED, false);
            return true;
        } else {
            return false;
        }
    }

    /**
     * Clears the accounts state-related shared prefs.
     */
    @VisibleForTesting
    public void clearAccountsStateSharedPrefsForTesting() {
        mManager.removeKey(ChromePreferenceKeys.SIGNIN_ACCOUNT_RENAME_EVENT_INDEX);
        mManager.removeKey(ChromePreferenceKeys.SIGNIN_ACCOUNT_RENAMED);
        mManager.removeKey(ChromePreferenceKeys.SIGNIN_ACCOUNTS_CHANGED);
    }

    /**
     * Returns Chrome major version number when signin promo was last shown, or 0 if version number
     * isn't known.
     */
    public int getSigninPromoLastShownVersion() {
        return mManager.readInt(ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION);
    }

    /**
     * Sets Chrome major version number when signin promo was last shown.
     */
    public void setSigninPromoLastShownVersion(int majorVersion) {
        mManager.writeInt(ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION, majorVersion);
    }

    /**
     * Returns a set of account names on the device when signin promo was last shown,
     * or null if promo hasn't been shown yet.
     */
    @Nullable
    public Set<String> getSigninPromoLastAccountNames() {
        return mManager.readStringSet(
                ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, null);
    }

    /**
     * Stores a set of account names on the device when signin promo is shown.
     */
    public void setSigninPromoLastAccountNames(Set<String> accountNames) {
        mManager.writeStringSet(
                ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, accountNames);
    }

    /**
     * Returns timestamp of the suppression period start if signin promos in the New Tab Page are
     * temporarily suppressed; zero otherwise.
     * @return the epoch time in milliseconds (see {@link System#currentTimeMillis()}).
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public long getNewTabPageSigninPromoSuppressionPeriodStart() {
        return mManager.readLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_SUPPRESSION_PERIOD_START);
    }

    /**
     * Sets timestamp of the suppression period start if signin promos in the New Tab Page are
     * temporarily suppressed.
     * @param timeMillis the epoch time in milliseconds (see {@link System#currentTimeMillis()}).
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void setNewTabPageSigninPromoSuppressionPeriodStart(long timeMillis) {
        mManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_SUPPRESSION_PERIOD_START, timeMillis);
    }

    /**
     * Removes the stored timestamp of the suppression period start when signin promos in the New
     * Tab Page are no longer suppressed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void clearNewTabPageSigninPromoSuppressionPeriodStart() {
        mManager.removeKey(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_SUPPRESSION_PERIOD_START);
    }

    /**
     * Sets the email of the account for which sync was enabled.
     *
     * @param accountEmail The email of the sync account or null if sync isn't enabled.
     */
    // TODO(https://crbug.com/1091858): Remove this after migrating the legacy code that uses
    //                                  the sync account before the native is loaded.
    public void setLegacySyncAccountEmail(@Nullable String accountEmail) {
        mManager.writeString(ChromePreferenceKeys.SIGNIN_LEGACY_SYNC_ACCOUNT_EMAIL, accountEmail);
    }

    /**
     * The email of the account for which sync was enabled.
     */
    // TODO(https://crbug.com/1091858): Remove this after migrating the legacy code that uses
    //                                  the sync account before the native is loaded.
    public String getLegacySyncAccountEmail() {
        return mManager.readString(ChromePreferenceKeys.SIGNIN_LEGACY_SYNC_ACCOUNT_EMAIL, null);
    }

    /**
     * Increments the shown count for the account picker bottom sheet.
     */
    public void incrementAccountPickerBottomSheetShownCount() {
        mManager.incrementInt(ChromePreferenceKeys.ACCOUNT_PICKER_BOTTOM_SHEET_SHOWN_COUNT);
    }

    /**
     * Returns the number of times account picker bottom sheet has already been shown.
     */
    public int getAccountPickerBottomSheetShownCount() {
        return mManager.readInt(ChromePreferenceKeys.ACCOUNT_PICKER_BOTTOM_SHEET_SHOWN_COUNT);
    }
}
