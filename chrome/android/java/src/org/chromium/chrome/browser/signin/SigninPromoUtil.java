// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.collection.ArraySet;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.signin.ui.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ui.SigninPromoController;
import org.chromium.chrome.browser.version.ChromeVersionInfo;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
* Helper functions for promoting sign in.
*/
public class SigninPromoUtil {
    private SigninPromoUtil() {}

    /**
     * Launches the signin promo if it needs to be displayed.
     * @param activity The parent activity.
     * @return Whether the signin promo is shown.
     */
    public static boolean launchSigninPromoIfNeeded(final Activity activity) {
        if (!AccountManagerFacadeProvider.getInstance().isCachePopulated()) {
            // Suppress the promo if the account list isn't available yet.
            return false;
        }

        SigninPreferencesManager preferencesManager = SigninPreferencesManager.getInstance();
        int currentMajorVersion = ChromeVersionInfo.getProductMajorVersion();
        boolean isSignedIn = IdentityServicesProvider.get()
                                     .getIdentityManager(Profile.getLastUsedRegularProfile())
                                     .hasPrimaryAccount();
        boolean wasSignedIn =
                TextUtils.isEmpty(UserPrefs.get(Profile.getLastUsedRegularProfile())
                                          .getString(Pref.GOOGLE_SERVICES_LAST_USERNAME));
        Set<String> accountNames = new ArraySet<>(AccountUtils.toAccountNames(
                AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts()));
        if (!shouldLaunchSigninPromo(preferencesManager, currentMajorVersion, isSignedIn,
                    wasSignedIn, accountNames)) {
            return false;
        }

        SigninUtils.startSigninActivityIfAllowed(activity, SigninAccessPoint.SIGNIN_PROMO);
        preferencesManager.setSigninPromoLastShownVersion(currentMajorVersion);
        preferencesManager.setSigninPromoLastAccountNames(accountNames);
        return true;
    }

    /**
     * Launches the signin promo if it needs to be displayed.
     * @param preferencesManager the preferences manager to persist data
     * @param currentMajorVersion the current major version of Chrome
     * @param isSignedIn is user currently signed in
     * @param wasSignedIn has used manually signed out
     * @param accountNames the list of accounts on the device
     * @return Whether the signin promo should be shown.
     */
    @VisibleForTesting
    static boolean shouldLaunchSigninPromo(SigninPreferencesManager preferencesManager,
            int currentMajorVersion, boolean isSignedIn, boolean wasSignedIn,
            Set<String> accountNames) {
        int lastPromoMajorVersion = preferencesManager.getSigninPromoLastShownVersion();
        if (lastPromoMajorVersion == 0) {
            preferencesManager.setSigninPromoLastShownVersion(currentMajorVersion);
            return false;
        }

        // Don't show if user is signed in.
        if (isSignedIn) return false;

        // Don't show if user has manually signed out.
        if (wasSignedIn) return false;

        // Promo can be shown at most once every 2 Chrome major versions.
        if (currentMajorVersion < lastPromoMajorVersion + 2) return false;

        // Don't show if the account list isn't available yet or there are no accounts in it.
        if (accountNames.isEmpty()) return false;

        // Don't show if no new accounts have been added after the last time promo was shown.
        Set<String> previousAccountNames = preferencesManager.getSigninPromoLastAccountNames();
        return previousAccountNames == null || !previousAccountNames.containsAll(accountNames);
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
        DisplayableProfileData profileData = null;
        List<Account> accounts = AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts();
        if (accounts.size() > 0) {
            String defaultAccountName = accounts.get(0).name;
            profileDataCache.update(Collections.singletonList(defaultAccountName));
            profileData = profileDataCache.getProfileDataOrDefault(defaultAccountName);
        }
        signinPromoController.detach();
        signinPromoController.setupPromoView(view.getContext(), view, profileData, listener);
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
                        .getPrimaryAccountInfo(ConsentLevel.NOT_REQUIRED));
        assert signedInAccount != null : "Sync promo should only be shown for a signed in account";
        profileDataCache.update(Collections.singletonList(signedInAccount));
        DisplayableProfileData profileData =
                profileDataCache.getProfileDataOrDefault(signedInAccount);
        signinPromoController.detach();
        signinPromoController.setupPromoView(view.getContext(), view, profileData, listener);

        view.getPrimaryButton().setText(R.string.sync_promo_turn_on_sync);
        view.getSecondaryButton().setVisibility(View.GONE);
    }

    /**
     * A convenience method to create an SigninActivity, passing the access point as an
     * intent extra.
     * @param window WindowAndroid from which to get the Activity/Context.
     * @param accessPoint for metrics purposes.
     */
    @CalledByNative
    private static void openSigninActivityForPromo(WindowAndroid window, int accessPoint) {
        Activity activity = window.getActivity().get();
        if (activity != null) {
            SigninUtils.startSigninActivityIfAllowed(activity, accessPoint);
        }
    }
}
