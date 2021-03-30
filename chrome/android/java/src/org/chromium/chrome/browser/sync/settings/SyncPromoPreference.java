// Copyright 2020 The Chromium Authors. All rights reserved.
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
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInAllowedObserver;
import org.chromium.chrome.browser.signin.ui.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ui.SigninPromoController;
import org.chromium.chrome.browser.signin.ui.SigninPromoUtil;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.ProfileSyncService.SyncStateChangedListener;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A preference that displays Personalized Sync Promo when the user is not syncing.
 */
// TODO(https://crbug.com/1110889): Move all promos from SigninPreference to this class.
public class SyncPromoPreference
        extends Preference implements SignInAllowedObserver, ProfileDataCache.Observer,
                                      SyncStateChangedListener, AccountsChangeObserver {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({State.PROMO_HIDDEN, State.PERSONALIZED_SIGNIN_PROMO, State.PERSONALIZED_SYNC_PROMO})
    public @interface State {
        int PROMO_HIDDEN = 0;
        int PERSONALIZED_SIGNIN_PROMO = 1;
        int PERSONALIZED_SYNC_PROMO = 2;
    }

    private final ProfileDataCache mProfileDataCache;
    private final AccountManagerFacade mAccountManagerFacade;
    private @SignInPreference.State int mState;
    private Runnable mStateChangedCallback;
    private @Nullable SigninPromoController mSigninPromoController;

    /**
     * Constructor for inflating from XML.
     */
    public SyncPromoPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.personalized_signin_promo_view_settings);

        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();

        // State will be updated in onAttached.
        mState = State.PROMO_HIDDEN;
        setVisible(false);
    }

    @Override
    public void onAttached() {
        super.onAttached();

        mAccountManagerFacade.addObserver(this);
        IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .addSignInAllowedObserver(this);
        mProfileDataCache.addObserver(this);
        FirstRunSignInProcessor.updateSigninManagerFirstRunCheckDone();
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.addSyncStateChangedListener(this);
        }

        update();
    }

    @Override
    public void onDetached() {
        super.onDetached();

        mAccountManagerFacade.removeObserver(this);
        IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .removeSignInAllowedObserver(this);
        mProfileDataCache.removeObserver(this);
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.removeSyncStateChangedListener(this);
        }
    }

    /**
     * Should be called when the {@link PreferenceFragmentCompat} which used {@link
     * SyncPromoPreference} gets destroyed. Used to record "ImpressionsTilDismiss" histogram.
     */
    public void onPreferenceFragmentDestroyed() {
        if (mSigninPromoController != null) {
            mSigninPromoController.onPromoDestroyed();
        }
    }

    /** Returns the state of the preference. Not valid until registerForUpdates is called. */
    @State
    public int getState() {
        return mState;
    }

    /** Sets callback to be notified of changes to the preference state. See {@link #getState}. */
    public void setOnStateChangedCallback(Runnable stateChangedCallback) {
        mStateChangedCallback = stateChangedCallback;
    }

    private void setState(@State int state) {
        if (mState == state) return;
        mState = state;
        assert mStateChangedCallback != null;
        mStateChangedCallback.run();
    }

    /** Updates the title, summary, and image based on the current sign-in state. */
    private void update() {
        if (IdentityServicesProvider.get()
                        .getSigninManager(Profile.getLastUsedRegularProfile())
                        .isSigninDisabledByPolicy()) {
            setupPromoHidden();
            return;
        }

        boolean personalizedPromoDismissed = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED, false);
        if (!personalizedPromoDismissed
                && SigninPromoController.hasNotReachedImpressionLimit(SigninAccessPoint.SETTINGS)) {
            IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                    Profile.getLastUsedRegularProfile());
            if (identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN) == null) {
                setupPersonalizedPromo(State.PERSONALIZED_SIGNIN_PROMO);
                return;
            } else if (identityManager.getPrimaryAccountInfo(ConsentLevel.SYNC) == null) {
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

        if (mSigninPromoController == null) {
            mSigninPromoController = new SigninPromoController(
                    SigninAccessPoint.SETTINGS, SigninActivityLauncherImpl.get());
        }

        notifyChanged();
    }

    private void setupPromoHidden() {
        setState(State.PROMO_HIDDEN);
        mSigninPromoController = null;
        setVisible(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        if (mSigninPromoController == null) {
            return;
        }

        PersonalizedSigninPromoView syncPromoView =
                (PersonalizedSigninPromoView) holder.findViewById(R.id.signin_promo_view_container);
        if (mState == State.PERSONALIZED_SIGNIN_PROMO) {
            SigninPromoUtil.setupSigninPromoViewFromCache(mSigninPromoController, mProfileDataCache,
                    syncPromoView, this::onPromoDismissClicked);
        } else {
            SigninPromoUtil.setupSyncPromoViewFromCache(mSigninPromoController, mProfileDataCache,
                    syncPromoView, this::onPromoDismissClicked);
        }
    }

    public void onPromoDismissClicked() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED, true);
        setupPromoHidden();
    }

    // SignInAllowedObserver implementation.
    @Override
    public void onSignInAllowedChanged() {
        update();
    }

    // ProfileSyncServiceListener implementation.
    @Override
    public void syncStateChanged() {
        update();
    }

    // ProfileDataCache.Observer implementation.
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        update();
    }

    // AccountsChangeObserver implementation.
    @Override
    public void onAccountsChanged() {
        update();
    }
}
