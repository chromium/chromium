// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import android.accounts.Account;
import android.content.Context;
import android.text.TextUtils;

import com.google.common.base.Optional;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Helper functions for promoting sign in.
 */
public final class SigninPromoUtil {
    private SigninPromoUtil() {}

    /**
     * Launches the {@link SyncConsentActivity} if it needs to be displayed.
     * @param context The {@link Context} to launch the {@link SyncConsentActivity}.
     * @param syncConsentActivityLauncher launcher used to launch the {@link SyncConsentActivity}.
     * @param currentMajorVersion The current major version of Chrome.
     * @return Whether the signin promo is shown.
     */
    public static boolean launchSigninPromoIfNeeded(Context context,
            SyncConsentActivityLauncher syncConsentActivityLauncher,
            final int currentMajorVersion) {
        final SigninPreferencesManager prefManager = SigninPreferencesManager.getInstance();
        final int lastPromoMajorVersion = prefManager.getSigninPromoLastShownVersion();
        if (lastPromoMajorVersion == 0) {
            prefManager.setSigninPromoLastShownVersion(currentMajorVersion);
            return false;
        }

        if (currentMajorVersion < lastPromoMajorVersion + 2) {
            // Promo can be shown at most once every 2 Chrome major versions.
            return false;
        }

        final Profile profile = Profile.getLastUsedRegularProfile();
        if (IdentityServicesProvider.get().getIdentityManager(profile).getPrimaryAccountInfo(
                    ConsentLevel.SYNC)
                != null) {
            // Don't show if user is signed in.
            return false;
        }

        if (TextUtils.isEmpty(
                    UserPrefs.get(profile).getString(Pref.GOOGLE_SERVICES_LAST_USERNAME))) {
            // Don't show if user has manually signed out.
            return false;
        }

        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        Optional<List<Account>> accounts = accountManagerFacade.getGoogleAccounts();
        if (!accounts.isPresent() || accounts.get().isEmpty()) {
            // Don't show if the account list isn't available yet or there are no accounts in it.
            return false;
        }

        Optional<Boolean> canDefaultAccountOfferExtendedSyncPromos =
                accountManagerFacade.canOfferExtendedSyncPromos(accounts.get().get(0));
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MINOR_MODE_SUPPORT)
                && canDefaultAccountOfferExtendedSyncPromos.or(/* defaultValue= */ false)) {
            return false;
        }

        final List<String> currentAccountNames = AccountUtils.toAccountNames(accounts.get());
        final Set<String> previousAccountNames = prefManager.getSigninPromoLastAccountNames();
        if (previousAccountNames != null && previousAccountNames.containsAll(currentAccountNames)) {
            // Don't show if no new accounts have been added after the last time promo was shown.
            return false;
        }

        syncConsentActivityLauncher.launchActivityIfAllowed(
                context, SigninAccessPoint.SIGNIN_PROMO);
        prefManager.setSigninPromoLastShownVersion(currentMajorVersion);
        prefManager.setSigninPromoLastAccountNames(new HashSet<>(currentAccountNames));
        return true;
    }

    /**
     * @param signinPromoController The {@link SigninPromoController} that maintains the view.
     * @param profileDataCache The {@link ProfileDataCache} that stores profile data.
     * @param view The {@link PersonalizedSigninPromoView} that should be set up.
     * @param listener The {@link SigninPromoController.OnDismissListener} to be set to the view.
     */
    public static void setupSigninPromoViewFromCache(SigninPromoController signinPromoController,
            ProfileDataCache profileDataCache, PersonalizedSigninPromoView view,
            SigninPromoController.OnDismissListener listener) {
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        if (!accountManagerFacade.isCachePopulated()) {
            signinPromoController.setupPromoView(view, /* profileData= */ null, listener);
            return;
        }
        final List<Account> accounts = accountManagerFacade.tryGetGoogleAccounts();
        if (accounts.isEmpty()) {
            signinPromoController.setupPromoView(view, /* profileData= */ null, listener);
            return;
        }
        final Account defaultAccount = accounts.get(0);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MINOR_MODE_SUPPORT)
                && accountManagerFacade.canOfferExtendedSyncPromos(defaultAccount).or(false)
                && signinPromoController.getAccessPoint()
                        == SigninAccessPoint.NTP_CONTENT_SUGGESTIONS) {
            return;
        }
        signinPromoController.setupPromoView(
                view, profileDataCache.getProfileDataOrDefault(defaultAccount.name), listener);
    }

    /**
     * @param signinPromoController The {@link SigninPromoController} that maintains the view.
     * @param profileDataCache The {@link ProfileDataCache} that stores profile data.
     * @param view The {@link PersonalizedSigninPromoView} that should be set up.
     * @param listener The {@link SigninPromoController.OnDismissListener} to be set to the view.
     */
    public static void setupSyncPromoViewFromCache(SigninPromoController signinPromoController,
            ProfileDataCache profileDataCache, PersonalizedSigninPromoView view,
            SigninPromoController.OnDismissListener listener) {
        final Account primaryAccount = CoreAccountInfo.getAndroidAccountFrom(
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        assert primaryAccount != null : "Sync promo should only be shown for a signed in account";

        final boolean canPrimaryAccountOfferExtendedSyncPromos =
                AccountManagerFacadeProvider.getInstance()
                        .canOfferExtendedSyncPromos(primaryAccount)
                        .or(false);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MINOR_MODE_SUPPORT)
                && canPrimaryAccountOfferExtendedSyncPromos
                && signinPromoController.getAccessPoint()
                        == SigninAccessPoint.NTP_CONTENT_SUGGESTIONS) {
            return;
        }
        signinPromoController.setupPromoView(
                view, profileDataCache.getProfileDataOrDefault(primaryAccount.name), listener);
    }
}
