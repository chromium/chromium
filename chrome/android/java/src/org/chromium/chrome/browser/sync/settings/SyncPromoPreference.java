// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.SyncPromoController;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A preference that displays Personalized Sync Promo when the user is not syncing. */
public class SyncPromoPreference extends Preference
        implements SignInStateObserver, ProfileDataCache.Observer, AccountsChangeObserver {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({State.PROMO_HIDDEN, State.PERSONALIZED_SIGNIN_PROMO, State.PERSONALIZED_SYNC_PROMO})
    public @interface State {
        int PROMO_HIDDEN = 0;
        int PERSONALIZED_SIGNIN_PROMO = 1;
        int PERSONALIZED_SYNC_PROMO = 2;
    }

    private ProfileDataCache mProfileDataCache;
    private AccountManagerFacade mAccountManagerFacade;
    private SigninManager mSigninManager;
    private IdentityManager mIdentityManager;

    private @State int mState;
    private @Nullable SyncPromoController mSyncPromoController;

    /** Constructor for inflating from XML. */
    public SyncPromoPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.sync_promo_view_settings);

        // State will be updated in onAttached.
        mState = State.PROMO_HIDDEN;
        setVisible(false);
    }

    /**
     * Initialize the dependencies for the SyncPromoPreference.
     *
     * <p>Must be called before the preference is attached, which is called from the containing
     * settings screen's onViewCreated method.
     */
    public void initialize(
            ProfileDataCache profileDataCache,
            AccountManagerFacade accountManagerFacade,
            SigninManager signinManager,
            IdentityManager identityManager,
            SyncPromoController syncPromoController) {
        mProfileDataCache = profileDataCache;
        mAccountManagerFacade = accountManagerFacade;
        mSigninManager = signinManager;
        mIdentityManager = identityManager;
        mSyncPromoController = syncPromoController;
    }

    @Override
    public void onAttached() {
        super.onAttached();

        mAccountManagerFacade.addObserver(this);
        mSigninManager.addSignInStateObserver(this);
        mProfileDataCache.addObserver(this);

        update();
    }

    @Override
    public void onDetached() {
        super.onDetached();

        mAccountManagerFacade.removeObserver(this);
        mSigninManager.removeSignInStateObserver(this);
        mProfileDataCache.removeObserver(this);
    }

    /** Returns the state of the preference. Not valid until registerForUpdates is called. */
    public @State int getState() {
        return mState;
    }

    private void setState(@State int state) {
        if (mState == state) return;

        final boolean hasStateChangedFromHiddenToShown =
                mState == State.PROMO_HIDDEN
                        && (state == State.PERSONALIZED_SIGNIN_PROMO
                                || state == State.PERSONALIZED_SYNC_PROMO);
        if (hasStateChangedFromHiddenToShown) {
            mSyncPromoController.increasePromoShowCount();
        }

        mState = state;
    }

    /** Updates the title, summary, and image based on the current sign-in state. */
    private void update() {
        if (SigninFeatureMap.isEnabled(SigninFeatures.HIDE_SETTINGS_SIGN_IN_PROMO)
                || mSigninManager.isSigninDisabledByPolicy()) {
            setupPromoHidden();
            return;
        }

        if (mSyncPromoController.canShowSyncPromo()) {
            if (!mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
                setupPersonalizedPromo(State.PERSONALIZED_SIGNIN_PROMO);
                return;
            }

            if (!mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC)) {
                setupPersonalizedPromo(State.PERSONALIZED_SYNC_PROMO);
                return;
            }
        }

        setupPromoHidden();
    }

    private void setupPersonalizedPromo(@State int state) {
        setState(state);
        setSelectable(false);
        setVisible(true);
        notifyChanged();
    }

    private void setupPromoHidden() {
        setState(State.PROMO_HIDDEN);
        setVisible(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        if (mState == State.PROMO_HIDDEN) return;

        PersonalizedSigninPromoView syncPromoView =
                (PersonalizedSigninPromoView) holder.findViewById(R.id.signin_promo_view_container);
        mSyncPromoController.setUpSyncPromoView(
                mProfileDataCache, syncPromoView, this::onPromoDismissClicked);
    }

    public void onPromoDismissClicked() {
        setupPromoHidden();
    }

    // SignInAllowedObserver implementation.
    @Override
    public void onSignInAllowedChanged() {
        update();
    }

    // ProfileDataCache.Observer implementation.
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        update();
    }

    // AccountsChangeObserver implementation.
    @Override
    public void onCoreAccountInfosChanged() {
        update();
    }
}
