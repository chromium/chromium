// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.Nullable;
import androidx.annotation.StringDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Set;

/** SigninPreferencesManager stores the state of SharedPreferences related to account sign-in. */
public class SigninPreferencesManager {
    private static final SigninPreferencesManager INSTANCE = new SigninPreferencesManager();

    private final SharedPreferencesManager mManager;

    /** Suffix strings for promo shown count preference and histograms. */
    @StringDef({
        SyncPromoAccessPointId.BOOKMARKS,
        SyncPromoAccessPointId.NTP,
        SyncPromoAccessPointId.RECENT_TABS,
        SyncPromoAccessPointId.SETTINGS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SyncPromoAccessPointId {
        String BOOKMARKS = "Bookmarks";
        String NTP = "Ntp";
        String RECENT_TABS = "RecentTabs"; // Only used for histograms
        String SETTINGS = "Settings";
    }

    private SigninPreferencesManager() {
        mManager = ChromeSharedPreferences.getInstance();
    }

    /**
     * @return the SignInPromoStore singleton
     */
    public static SigninPreferencesManager getInstance() {
        return INSTANCE;
    }

    /**
     * Suppress signin promos in New Tab Page for {@link SignInPromo#SUPPRESSION_PERIOD_MS}. This
     * will not affect promos that were created before this call.
     */
    public void temporarilySuppressNewTabPagePromos() {
        SigninPreferencesManager.getInstance()
                .setNewTabPageSigninPromoSuppressionPeriodStart(System.currentTimeMillis());
    }

    /** Clears the accounts state-related shared prefs. */
    public void clearSigninPromoLastShownPrefsForTesting() {
        mManager.removeKey(ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION);
        mManager.removeKey(ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES);
    }

    /**
     * Returns Chrome major version number when signin promo was last shown, or 0 if version number
     * isn't known.
     */
    public int getSigninPromoLastShownVersion() {
        return mManager.readInt(ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION);
    }

    /** Sets Chrome major version number when signin promo was last shown. */
    public void setSigninPromoLastShownVersion(int majorVersion) {
        mManager.writeInt(ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION, majorVersion);
    }

    /**
     * Returns a set of account emails on the device when signin promo was last shown,
     * or null if promo hasn't been shown yet.
     */
    @Nullable
    public Set<String> getSigninPromoLastAccountEmails() {
        return mManager.readStringSet(
                ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, null);
    }

    /** Stores a set of account emails on the device when signin promo is shown. */
    public void setSigninPromoLastAccountEmails(Set<String> accountEmails) {
        mManager.writeStringSet(
                ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, accountEmails);
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
    // TODO(crbug.com/40697988): Remove this after migrating the legacy code that uses
    //                                  the sync account before the native is loaded.
    public void setLegacyPrimaryAccountEmail(@Nullable String accountEmail) {
        mManager.writeString(
                ChromePreferenceKeys.SIGNIN_LEGACY_PRIMARY_ACCOUNT_EMAIL, accountEmail);
    }

    // TODO(crbug.com/337003667): Remove after fixing internal usages.
    @Deprecated
    public void setLegacySyncAccountEmail(@Nullable String accountEmail) {
        setLegacyPrimaryAccountEmail(accountEmail);
    }

    /** The email of the account for which sync was enabled. */
    // TODO(crbug.com/40697988): Remove this after migrating the legacy code that uses
    //                                  the sync account before the native is loaded.
    public String getLegacyPrimaryAccountEmail() {
        return mManager.readString(ChromePreferenceKeys.SIGNIN_LEGACY_PRIMARY_ACCOUNT_EMAIL, null);
    }

    // TODO(crbug.com/337003667): Remove after fixing internal usages.
    @Deprecated
    public String getLegacySyncAccountEmail() {
        return getLegacyPrimaryAccountEmail();
    }

    /** Increments the active dismissal count for the account picker bottom sheet. */
    public void incrementWebSigninAccountPickerActiveDismissalCount() {
        mManager.incrementInt(
                ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT);
    }

    /** Returns the number of times account picker bottom sheet has been actively dismissed. */
    public int getWebSigninAccountPickerActiveDismissalCount() {
        return mManager.readInt(
                ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT);
    }

    /** Clears the active dismissal count for the account picker bottom sheet. */
    public void clearWebSigninAccountPickerActiveDismissalCount() {
        mManager.removeKey(ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT);
    }
}
