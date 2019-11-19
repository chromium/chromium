// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.text.format.DateUtils;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInAllowedObserver;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * Shows a card prompting the user to sign in. This item is also an {@link OptionalLeaf}, and sign
 * in state changes control its visibility.
 */
public class SignInPromo extends OptionalLeaf {
    /**
     * Period for which promos are suppressed if signin is refused in FRE.
     */
    @VisibleForTesting
    static final long SUPPRESSION_PERIOD_MS = DateUtils.DAY_IN_MILLIS;

    private static boolean sDisablePromoForTests;

    /**
     * Whether the promo has been dismissed by the user.
     */
    private boolean mDismissed;

    /**
     * Whether the signin status means that the user has the possibility to sign in.
     */
    private boolean mCanSignIn;

    /**
     * Whether the list of accounts is ready to be displayed. An attempt to display SignInPromo
     * while accounts are not ready may cause ANR since the UI thread would be synchronously waiting
     * for the accounts list.
     */
    private boolean mAccountsReady;

    /**
     * Whether personalized suggestions can be shown. If it's not the case, we have no reason to
     * offer the user to sign in.
     */
    private boolean mCanShowPersonalizedSuggestions;

    private final SigninObserver mSigninObserver;
    protected final SigninPromoController mSigninPromoController;
    protected final ProfileDataCache mProfileDataCache;

    protected SignInPromo(SigninManager signinManager) {
        Context context = ContextUtils.getApplicationContext();

        // TODO(bsazonov): Signin manager should check for native status in isSignInAllowed
        mCanSignIn = signinManager.isSignInAllowed()
                && !signinManager.getIdentityManager().hasPrimaryAccount();
        mAccountsReady = AccountManagerFacade.get().isCachePopulated();
        updateVisibility();

        int imageSize = context.getResources().getDimensionPixelSize(R.dimen.user_picture_size);
        mProfileDataCache = new ProfileDataCache(context, imageSize);
        mSigninPromoController =
                new SigninPromoController(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS);

        mSigninObserver = new SigninObserver(signinManager);
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
     * Suppress signin promos in New Tab Page for {@link SignInPromo#SUPPRESSION_PERIOD_MS}. This
     * will not affect promos that were created before this call.
     */
    public static void temporarilySuppressPromos() {
        ChromePreferenceManager.getInstance().setNewTabPageSigninPromoSuppressionPeriodStart(
                System.currentTimeMillis());
    }

    /** @return Whether the {@link SignInPromo} should be created. */
    public static boolean shouldCreatePromo() {
        return !sDisablePromoForTests
                && !SharedPreferencesManager.getInstance().readBoolean(
                        ChromePreferenceKeys.NTP_SIGNIN_PROMO_DISMISSED, false)
                && !getSuppressionStatus();
    }

    private static boolean getSuppressionStatus() {
        long suppressedFrom = ChromePreferenceManager.getInstance()
                                      .getNewTabPageSigninPromoSuppressionPeriodStart();
        if (suppressedFrom == 0) return false;
        long currentTime = System.currentTimeMillis();
        long suppressedTo = suppressedFrom + SUPPRESSION_PERIOD_MS;
        if (suppressedFrom <= currentTime && currentTime < suppressedTo) {
            return true;
        }
        ChromePreferenceManager.getInstance().clearNewTabPageSigninPromoSuppressionPeriodStart();
        return false;
    }

    @Override
    @ItemViewType
    protected int getItemViewType() {
        return ItemViewType.PROMO;
    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {
        ((PersonalizedPromoViewHolder) holder)
                .onBindViewHolder(mSigninPromoController, mProfileDataCache);
    }

    @Override
    public String describeForTesting() {
        return "SIGN_IN_PROMO";
    }

    /** Notify that the content for this {@link SignInPromo} has changed. */
    protected void notifyDataChanged() {
        if (isVisible()) notifyItemChanged(0, PersonalizedPromoViewHolder::update);
    }

    private void updateVisibility() {
        setVisibilityInternal(
                !mDismissed && mCanSignIn && mCanShowPersonalizedSuggestions && mAccountsReady);
    }

    @Override
    protected boolean canBeDismissed() {
        return true;
    }

    /** Hides the sign in promo and sets a preference to make sure it is not shown again. */
    @Override
    public void dismiss(Callback<String> itemRemovedCallback) {
        mDismissed = true;
        updateVisibility();

        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.NTP_SIGNIN_PROMO_DISMISSED, true);

        final @StringRes int promoHeader = mSigninPromoController.getDescriptionStringId();

        mSigninObserver.unregister();
        itemRemovedCallback.onResult(ContextUtils.getApplicationContext().getString(promoHeader));
    }

    @VisibleForTesting
    public static void setDisablePromoForTests(boolean disable) {
        sDisablePromoForTests = disable;
    }

    @VisibleForTesting
    public SigninObserver getSigninObserverForTesting() {
        return mSigninObserver;
    }

    /**
     * Observer to get notifications about various sign-in events.
     */
    @VisibleForTesting
    public class SigninObserver implements SignInStateObserver, SignInAllowedObserver,
                                           ProfileDataCache.Observer, AccountsChangeObserver {
        private final SigninManager mSigninManager;

        /** Guards {@link #unregister()}, which can be called multiple times. */
        private boolean mUnregistered;

        private SigninObserver(SigninManager signinManager) {
            mSigninManager = signinManager;
            mSigninManager.addSignInAllowedObserver(this);
            mSigninManager.addSignInStateObserver(this);

            mProfileDataCache.addObserver(this);
            AccountManagerFacade.get().addObserver(this);
        }

        private void unregister() {
            if (mUnregistered) return;
            mUnregistered = true;

            mSigninManager.removeSignInAllowedObserver(this);
            mSigninManager.removeSignInStateObserver(this);
            mProfileDataCache.removeObserver(this);
            AccountManagerFacade.get().removeObserver(this);
        }

        // SignInAllowedObserver implementation.
        @Override
        public void onSignInAllowedChanged() {
            // Listening to onSignInAllowedChanged is important for the FRE. Sign in is not allowed
            // until it is completed, but the NTP is initialised before the FRE is even shown. By
            // implementing this we can show the promo if the user did not sign in during the FRE.
            mCanSignIn = mSigninManager.isSignInAllowed();
            updateVisibility();
        }

        // SignInStateObserver implementation.
        @Override
        public void onSignedIn() {
            mCanSignIn = false;
            updateVisibility();
        }

        @Override
        public void onSignedOut() {
            mCanSignIn = mSigninManager.isSignInAllowed();
            updateVisibility();
        }

        // AccountsChangeObserver implementation.
        @Override
        public void onAccountsChanged() {
            mAccountsReady = AccountManagerFacade.get().isCachePopulated();
            // We don't change the visibility here to avoid the promo popping up in the feed
            // unexpectedly. If accounts are ready, the promo will be shown up on the next reload.
            notifyDataChanged();
        }

        // ProfileDataCache.Observer implementation.
        @Override
        public void onProfileDataUpdated(String accountId) {
            notifyDataChanged();
        }
    }
}
