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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.chrome.browser.signin.SigninPromoUtil;
import org.chromium.chrome.browser.sync.AndroidSyncSettings;
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
public class SyncPromoPreference extends Preference
        implements ProfileDataCache.Observer, AndroidSyncSettings.AndroidSyncSettingsObserver,
                   SyncStateChangedListener, AccountsChangeObserver {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({State.PROMO_HIDDEN, State.PERSONALIZED_SYNC_PROMO})
    public @interface State {
        int PROMO_HIDDEN = 0;
        int PERSONALIZED_SYNC_PROMO = 1;
    }

    private final ProfileDataCache mProfileDataCache;
    private final AccountManagerFacade mAccountManagerFacade;
    private @SignInPreference.State int mState;
    private @Nullable SigninPromoController mSigninPromoController;

    /**
     * Constructor for inflating from XML.
     */
    public SyncPromoPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        mProfileDataCache = ProfileDataCache.createProfileDataCache(context);
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();

        // State will be updated in registerForUpdates.
        mState = State.PROMO_HIDDEN;
        setVisible(false);
    }

    @Override
    public void onAttached() {
        super.onAttached();
        mAccountManagerFacade.addObserver(this);
        mProfileDataCache.addObserver(this);
        FirstRunSignInProcessor.updateSigninManagerFirstRunCheckDone();
        AndroidSyncSettings.get().registerObserver(this);
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
        mProfileDataCache.removeObserver(this);
        AndroidSyncSettings.get().unregisterObserver(this);
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

    /** Updates the title, summary, and image based on the current sign-in state. */
    private void update() {
        // If feature is not enabled keep the preference at the default PROMO_NONE state.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            return;
        }
        boolean personalizedPromoDismissed = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED, false);
        if (isSignedInButNotSyncing() && !personalizedPromoDismissed
                && SigninPromoController.hasNotReachedImpressionLimit(SigninAccessPoint.SETTINGS)) {
            setupPersonalizedSyncPromo();
            return;
        }

        setupPromoHidden();
    }

    private void setupPersonalizedSyncPromo() {
        mState = State.PERSONALIZED_SYNC_PROMO;
        setLayoutResource(R.layout.personalized_signin_promo_view_settings);
        setVisible(true);

        if (mSigninPromoController == null) {
            mSigninPromoController = new SigninPromoController(SigninAccessPoint.SETTINGS);
        }

        notifyChanged();
    }

    private void setupPromoHidden() {
        mState = State.PROMO_HIDDEN;
        mSigninPromoController = null;
        setVisible(false);
    }

    private boolean isSignedInButNotSyncing() {
        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        return !identityManager.hasPrimaryAccount()
                && identityManager.getPrimaryAccountInfo(ConsentLevel.NOT_REQUIRED) != null;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        if (mSigninPromoController == null) {
            return;
        }

        PersonalizedSigninPromoView syncPromoView =
                (PersonalizedSigninPromoView) holder.findViewById(R.id.signin_promo_view_container);
        SigninPromoUtil.setupSyncPromoViewFromCache(
                mSigninPromoController, mProfileDataCache, syncPromoView, () -> {
                    SharedPreferencesManager.getInstance().writeBoolean(
                            ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED,
                            true);
                    setupPromoHidden();
                });
    }

    // ProfileSyncServiceListener implementation.
    @Override
    public void syncStateChanged() {
        update();
    }

    // ProfileDataCache.Observer implementation.
    @Override
    public void onProfileDataUpdated(String accountId) {
        update();
    }

    // AndroidSyncSettings.AndroidSyncSettingsObserver implementation.
    @Override
    public void androidSyncSettingsChanged() {
        update();
    }

    // AccountsChangeObserver implementation.
    @Override
    public void onAccountsChanged() {
        update();
    }
}
