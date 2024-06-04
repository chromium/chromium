// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.SyncPromoController;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Superclass tracking whether a signin card could be shown.
 *
 * Subclasses are notified when relevant signin status changes.
 */
public abstract class SignInPromo {
    /** Period for which promos are suppressed if signin is refused in FRE. */
    @VisibleForTesting static final long SUPPRESSION_PERIOD_MS = DateUtils.DAY_IN_MILLIS;

    private static boolean sDisablePromoForTests;

    /**
     * Whether personalized suggestions can be shown. If it's not the case, we have no reason to
     * offer the user to sign in.
     */
    private boolean mCanShowPersonalizedSuggestions;

    private boolean mIsVisible;

    private final SigninObserver mSigninObserver;
    private final SigninManager mSigninManager;
    protected final SyncPromoController mSyncPromoController;
    protected final ProfileDataCache mProfileDataCache;

    protected SignInPromo(SigninManager signinManager, SyncPromoController syncPromoController) {
        Context context = ContextUtils.getApplicationContext();
        mSigninManager = signinManager;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);
        mSyncPromoController = syncPromoController;
        mSigninObserver = new SigninObserver();

        updateVisibility();
    }

    /** Clear any dependencies. */
    public void destroy() {
        mSigninObserver.unregister();
    }

    /**
     * Update whether personalized suggestions can be shown and update visibility for this
     * {@link SignInPromo} accordingly.
     * @param canShow Whether personalized suggestions can be shown.
     */
    public void setCanShowPersonalizedSuggestions(boolean canShow) {
        mCanShowPersonalizedSuggestions = canShow;
        updateVisibility();
    }

    /**
     * @return Whether the {@link SignInPromo} should be created.
     */
    public static boolean shouldCreatePromo() {
        return !sDisablePromoForTests
                && !ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false)
                && !getSuppressionStatus();
    }

    private static boolean getSuppressionStatus() {
        long suppressedFrom =
                SigninPreferencesManager.getInstance()
                        .getNewTabPageSigninPromoSuppressionPeriodStart();
        if (suppressedFrom == 0) return false;
        long currentTime = System.currentTimeMillis();
        long suppressedTo = suppressedFrom + SUPPRESSION_PERIOD_MS;
        if (suppressedFrom <= currentTime && currentTime < suppressedTo) {
            return true;
        }
        SigninPreferencesManager.getInstance().clearNewTabPageSigninPromoSuppressionPeriodStart();
        return false;
    }

    public boolean isUserSignedInButNotSyncing() {
        IdentityManager identityManager = mSigninManager.getIdentityManager();
        return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                && !identityManager.hasPrimaryAccount(ConsentLevel.SYNC);
    }

    /** Notify that the content for this {@link SignInPromo} has changed. */
    protected abstract void notifyDataChanged();

    private void updateVisibility() {
        final boolean isAccountsCachePopulated =
                AccountManagerFacadeProvider.getInstance().getCoreAccountInfos().isFulfilled();
        boolean canShowPersonalizedSigninPromo =
                mSigninManager.isSigninAllowed()
                        && mSyncPromoController.canShowSyncPromo()
                        && mCanShowPersonalizedSuggestions
                        && isAccountsCachePopulated
                        && mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ true);
        boolean canShowPersonalizedSyncPromo =
                mSigninManager.isSyncOptInAllowed()
                        && mSyncPromoController.canShowSyncPromo()
                        && isUserSignedInButNotSyncing()
                        && mCanShowPersonalizedSuggestions
                        && isAccountsCachePopulated;
        setVisibilityInternal(canShowPersonalizedSigninPromo || canShowPersonalizedSyncPromo);
    }

    /**
     * Updates visibility status. Overridden by subclasses that want to track visibility changes.
     */
    protected void setVisibilityInternal(boolean visibility) {
        if (!mIsVisible && visibility) mSyncPromoController.increasePromoShowCount();
        mIsVisible = visibility;
    }

    /** Returns current visibility status of the underlying promo view. */
    public boolean isVisible() {
        return mIsVisible;
    }

    public void onDismissPromo() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, true);
        mSyncPromoController.detach();
        setVisibilityInternal(false);
    }

    public static void setDisablePromoForTesting(boolean disable) {
        sDisablePromoForTests = disable;
        ResettersForTesting.register(() -> sDisablePromoForTests = false);
    }

    public SigninObserver getSigninObserverForTesting() {
        return mSigninObserver;
    }

    /** Observer to get notifications about various sign-in events. */
    @VisibleForTesting
    public class SigninObserver
            implements SignInStateObserver, ProfileDataCache.Observer, AccountsChangeObserver {
        private final AccountManagerFacade mAccountManagerFacade;

        /** Guards {@link #unregister()}, which can be called multiple times. */
        private boolean mUnregistered;

        private SigninObserver() {
            mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();

            mSigninManager.addSignInStateObserver(this);
            mProfileDataCache.addObserver(this);
            mAccountManagerFacade.addObserver(this);
        }

        private void unregister() {
            if (mUnregistered) return;
            mUnregistered = true;

            mSigninManager.removeSignInStateObserver(this);
            mProfileDataCache.removeObserver(this);
            mAccountManagerFacade.removeObserver(this);
        }

        // SignInAllowedObserver implementation.
        @Override
        public void onSignInAllowedChanged() {
            // Listening to onSignInAllowedChanged is important for the FRE. Sign in is not allowed
            // until it is completed, but the NTP is initialised before the FRE is even shown. By
            // implementing this we can show the promo if the user did not sign in during the FRE.
            updateVisibility();
            // Update the promo state between sign-in promo and sync promo if required.
            notifyDataChanged();
        }

        // SignInStateObserver implementation.
        @Override
        public void onSignedIn() {
            updateVisibility();
            // Update the promo state between sign-in promo and sync promo if required.
            notifyDataChanged();
        }

        @Override
        public void onSignedOut() {
            updateVisibility();
            // Update the promo state between sign-in promo and sync promo if required.
            notifyDataChanged();
        }

        // AccountsChangeObserver implementation.
        @Override
        public void onCoreAccountInfosChanged() {
            // We don't change the visibility here to avoid the promo popping up in the feed
            // unexpectedly. If accounts are ready, the promo will be shown up on the next reload.
            notifyDataChanged();
        }

        // ProfileDataCache.Observer implementation.
        @Override
        public void onProfileDataUpdated(String accountEmail) {
            notifyDataChanged();
        }
    }
}
