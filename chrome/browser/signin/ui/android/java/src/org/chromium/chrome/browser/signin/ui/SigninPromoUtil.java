// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import android.accounts.Account;
import android.content.Context;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
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
     * Launches the {@link SigninActivity} if it needs to be displayed.
     * @param context The {@link Context} to launch the {@link SigninActivity}.
     * @param signinActivityLauncher launcher used to launch the {@link SigninActivity}.
     * @param currentMajorVersion The current major version of Chrome.
     * @return Whether the signin promo is shown.
     */
    public static boolean launchSigninPromoIfNeeded(Context context,
            SigninActivityLauncher signinActivityLauncher, final int currentMajorVersion) {
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

        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        if (!accountManagerFacade.isCachePopulated()) {
            // Suppress the promo if the account list isn't available yet.
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

        final List<String> currentAccountNames =
                AccountUtils.toAccountNames(accountManagerFacade.tryGetGoogleAccounts());
        if (currentAccountNames.isEmpty()) {
            // Don't show if the account list isn't available yet or there are no accounts in it.
            return false;
        }

        final Set<String> previousAccountNames = prefManager.getSigninPromoLastAccountNames();
        if (previousAccountNames != null && previousAccountNames.containsAll(currentAccountNames)) {
            // Don't show if no new accounts have been added after the last time promo was shown.
            return false;
        }

        signinActivityLauncher.launchActivityIfAllowed(context, SigninAccessPoint.SIGNIN_PROMO);
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
        signinPromoController.setupPromoView(
                view, getDefaultProfileData(profileDataCache), listener);
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
        String signedInAccount = CoreAccountInfo.getEmailFrom(
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        assert signedInAccount != null : "Sync promo should only be shown for a signed in account";
        signinPromoController.setupPromoView(
                view, profileDataCache.getProfileDataOrDefault(signedInAccount), listener);

        view.getPrimaryButton().setText(R.string.sync_promo_turn_on_sync);
        view.getSecondaryButton().setVisibility(View.GONE);
    }

    /**
     * @return The default profile data if the account list is available, otherwise returns null.
     */
    private static @Nullable DisplayableProfileData getDefaultProfileData(
            ProfileDataCache profileDataCache) {
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        if (accountManagerFacade.isCachePopulated()) {
            final List<Account> accounts = accountManagerFacade.tryGetGoogleAccounts();
            if (accounts.size() > 0) {
                return profileDataCache.getProfileDataOrDefault(accounts.get(0).name);
            }
        }
        return null;
    }
}
