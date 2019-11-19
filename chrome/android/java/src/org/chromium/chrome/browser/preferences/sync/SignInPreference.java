// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.sync;

import android.content.Context;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ManagedPreferencesUtils;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninActivity;
import org.chromium.chrome.browser.signin.SigninManager.SignInAllowedObserver;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.chrome.browser.signin.SigninPromoUtil;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.ProfileSyncService.SyncStateChangedListener;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.AndroidSyncSettings;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;

/**
 * A preference that displays "Sign in to Chrome" when the user is not sign in, and displays
 * the user's name, email, profile image and sync error icon if necessary when the user is signed
 * in.
 */
public class SignInPreference
        extends Preference implements SignInAllowedObserver, ProfileDataCache.Observer,
                                      AndroidSyncSettings.AndroidSyncSettingsObserver,
                                      SyncStateChangedListener, AccountsChangeObserver {
    @IntDef({State.SIGNIN_DISABLED, State.GENERIC_PROMO, State.PERSONALIZED_PROMO, State.SIGNED_IN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        int SIGNIN_DISABLED = 0;
        int GENERIC_PROMO = 1;
        int PERSONALIZED_PROMO = 2;
        int SIGNED_IN = 3;
    }

    private boolean mPersonalizedPromoEnabled = true;
    private boolean mWasGenericSigninPromoDisplayed;
    private boolean mViewEnabled;
    private @Nullable SigninPromoController mSigninPromoController;
    private final ProfileDataCache mProfileDataCache;
    private @State int mState;
    private @Nullable Runnable mStateChangedCallback;
    private boolean mObserversAdded;

    /**
     * Constructor for inflating from XML.
     */
    public SignInPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        int imageSize = context.getResources().getDimensionPixelSize(R.dimen.user_picture_size);
        mProfileDataCache = new ProfileDataCache(context, imageSize);

        setOnPreferenceClickListener(preference
                -> SigninActivity.startIfAllowed(getContext(), SigninAccessPoint.SETTINGS));

        // State will be updated in registerForUpdates.
        mState = State.SIGNED_IN;
    }

    /**
     * Starts listening for updates to the sign-in and sync state.
     */
    public void registerForUpdates() {
        AccountManagerFacade.get().addObserver(this);
        IdentityServicesProvider.getSigninManager().addSignInAllowedObserver(this);
        mProfileDataCache.addObserver(this);
        FirstRunSignInProcessor.updateSigninManagerFirstRunCheckDone();
        AndroidSyncSettings.get().registerObserver(this);
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.addSyncStateChangedListener(this);
        }
        mObserversAdded = true;

        update();
    }

    /**
     * Stops listening for updates to the sign-in and sync state. Every call to registerForUpdates()
     * must be matched with a call to this method.
     */
    public void unregisterForUpdates() {
        AccountManagerFacade.get().removeObserver(this);
        IdentityServicesProvider.getSigninManager().removeSignInAllowedObserver(this);
        mProfileDataCache.removeObserver(this);
        AndroidSyncSettings.get().unregisterObserver(this);
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.removeSyncStateChangedListener(this);
        }
        mObserversAdded = false;
    }

    /**
     * Should be called when the {@link PreferenceFragment} which used {@link SignInPreference} gets
     * destroyed. Used to record "ImpressionsTilDismiss" histogram.
     */
    public void onPreferenceFragmentDestroyed() {
        if (mSigninPromoController != null) {
            mSigninPromoController.onPromoDestroyed();
        }
    }

    private void setState(@State int state) {
        if (mState == state) return;
        mState = state;
        if (mStateChangedCallback != null) {
            mStateChangedCallback.run();
        }
    }

    /** Enables/disables personalized promo mode. */
    public void setPersonalizedPromoEnabled(boolean personalizedPromoEnabled) {
        if (mPersonalizedPromoEnabled == personalizedPromoEnabled) return;
        mPersonalizedPromoEnabled = personalizedPromoEnabled;
        // Can't update until observers are added.
        if (mObserversAdded) update();
    }

    /** Returns the state of the preference. Not valid until registerForUpdates is called. */
    @State
    public int getState() {
        return mState;
    }

    /** Sets callback to be notified of changes to the preference state. See {@link #getState}. */
    public void setOnStateChangedCallback(@Nullable Runnable stateChangedCallback) {
        mStateChangedCallback = stateChangedCallback;
    }

    /** Updates the title, summary, and image based on the current sign-in state. */
    private void update() {
        if (IdentityServicesProvider.getSigninManager().isSigninDisabledByPolicy()) {
            setupSigninDisabled();
            return;
        }

        String accountName = ChromeSigninController.get().getSignedInAccountName();
        if (accountName != null) {
            setupSignedIn(accountName);
            return;
        }

        boolean personalizedPromoDismissed = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.SETTINGS_PERSONALIZED_SIGNIN_PROMO_DISMISSED, false);
        if (!mPersonalizedPromoEnabled || personalizedPromoDismissed) {
            setupGenericPromo();
            return;
        }

        if (mSigninPromoController != null) {
            // Don't change the promo type if the new promo is already being shown.
            setupPersonalizedPromo();
            return;
        }

        if (SigninPromoController.hasNotReachedImpressionLimit(SigninAccessPoint.SETTINGS)) {
            setupPersonalizedPromo();
            return;
        }

        setupGenericPromo();
    }

    private void setupSigninDisabled() {
        setState(State.SIGNIN_DISABLED);
        setLayoutResource(R.layout.account_management_account_row);
        setTitle(R.string.sign_in_to_chrome);
        setSummary(R.string.sign_in_to_chrome_disabled_summary);
        setFragment(null);
        setIcon(ManagedPreferencesUtils.getManagedByEnterpriseIconId());
        setWidgetLayoutResource(0);
        setViewEnabled(false);
        mSigninPromoController = null;
        mWasGenericSigninPromoDisplayed = false;
    }

    private void setupPersonalizedPromo() {
        setState(State.PERSONALIZED_PROMO);
        setLayoutResource(R.layout.personalized_signin_promo_view_settings);
        setTitle("");
        setSummary("");
        setFragment(null);
        setIcon(null);
        setWidgetLayoutResource(0);
        setViewEnabled(true);

        if (mSigninPromoController == null) {
            mSigninPromoController = new SigninPromoController(SigninAccessPoint.SETTINGS);
        }

        mWasGenericSigninPromoDisplayed = false;
        notifyChanged();
    }

    private void setupGenericPromo() {
        setState(State.GENERIC_PROMO);
        setLayoutResource(R.layout.account_management_account_row);
        setTitle(R.string.sign_in_to_chrome);

        setSummary(R.string.signin_pref_summary);

        setFragment(null);
        setIcon(AppCompatResources.getDrawable(getContext(), R.drawable.logo_avatar_anonymous));
        setWidgetLayoutResource(0);
        setViewEnabled(true);
        mSigninPromoController = null;

        if (!mWasGenericSigninPromoDisplayed) {
            RecordUserAction.record("Signin_Impression_FromSettings");
        }

        mWasGenericSigninPromoDisplayed = true;
    }

    private void setupSignedIn(String accountName) {
        setState(State.SIGNED_IN);
        mProfileDataCache.update(Collections.singletonList(accountName));
        DisplayableProfileData profileData = mProfileDataCache.getProfileDataOrDefault(accountName);

        setLayoutResource(R.layout.account_management_account_row);
        setTitle(profileData.getFullNameOrEmail());
        setSummary(accountName);
        setFragment(AccountManagementFragment.class.getName());
        setIcon(profileData.getImage());
        setWidgetLayoutResource(0);
        setViewEnabled(true);

        mSigninPromoController = null;
        mWasGenericSigninPromoDisplayed = false;
    }

    // This just changes visual representation. Actual enabled flag in preference stays
    // always true to receive clicks (necessary to show "Managed by administrator" toast).
    private void setViewEnabled(boolean enabled) {
        if (mViewEnabled == enabled) {
            return;
        }
        mViewEnabled = enabled;
        notifyChanged();
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        ViewUtils.setEnabledRecursive(holder.itemView, mViewEnabled);

        if (mSigninPromoController == null) {
            return;
        }

        PersonalizedSigninPromoView signinPromoView =
                (PersonalizedSigninPromoView) holder.findViewById(R.id.signin_promo_view_container);
        SigninPromoUtil.setupPromoViewFromCache(
                mSigninPromoController, mProfileDataCache, signinPromoView, () -> {
                    SharedPreferencesManager.getInstance().writeBoolean(
                            ChromePreferenceKeys.SETTINGS_PERSONALIZED_SIGNIN_PROMO_DISMISSED,
                            true);
                    update();
                });
    }

    // ProfileSyncServiceListener implementation.
    @Override
    public void syncStateChanged() {
        update();
    }

    // SignInAllowedObserver implementation.
    @Override
    public void onSignInAllowedChanged() {
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
