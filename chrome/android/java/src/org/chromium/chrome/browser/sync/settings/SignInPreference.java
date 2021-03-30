// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.annotation.IntDef;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInAllowedObserver;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.ProfileSyncService.SyncStateChangedListener;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.ViewUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A preference that displays "Sign in to Chrome" when the user is not sign in, and displays
 * the user's name, email, profile image and sync error icon if necessary when the user is signed
 * in.
 */
public class SignInPreference
        extends Preference implements SignInAllowedObserver, ProfileDataCache.Observer,
                                      SyncStateChangedListener, AccountsChangeObserver {
    @IntDef({State.SIGNIN_DISABLED_BY_POLICY, State.SIGNIN_DISALLOWED, State.GENERIC_PROMO,
            State.SIGNED_IN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        int SIGNIN_DISABLED_BY_POLICY = 0;
        int SIGNIN_DISALLOWED = 1;
        int GENERIC_PROMO = 2;
        int SIGNED_IN = 3;
    }

    private final PrefService mPrefService;
    private boolean mWasGenericSigninPromoDisplayed;
    private boolean mViewEnabled;
    private final ProfileDataCache mProfileDataCache;
    private final AccountManagerFacade mAccountManagerFacade;
    private @State int mState;

    /**
     * Constructor for inflating from XML.
     */
    public SignInPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.account_management_account_row);

        mPrefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();

        // State will be updated in registerForUpdates.
        mState = State.SIGNED_IN;
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

    /** Returns the state of the preference. Not valid until registerForUpdates is called. */
    @State
    public int getState() {
        return mState;
    }

    /** Updates the title, summary, and image based on the current sign-in state. */
    private void update() {
        if (IdentityServicesProvider.get()
                        .getSigninManager(Profile.getLastUsedRegularProfile())
                        .isSigninDisabledByPolicy()) {
            // TODO(https://crbug.com/1133739): Clean up after revising isSigninDisabledByPolicy.
            if (mPrefService.isManagedPreference(Pref.SIGNIN_ALLOWED)) {
                setupSigninDisabledByPolicy();
            } else {
                setupSigninDisallowed();
            }
            return;
        }

        CoreAccountInfo accountInfo =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (accountInfo != null) {
            setupSignedIn(accountInfo.getEmail());
            return;
        }

        setupGenericPromo();
    }

    private void setupSigninDisabledByPolicy() {
        mState = State.SIGNIN_DISABLED_BY_POLICY;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            setTitle(R.string.sync_promo_turn_on_sync);
        } else {
            setTitle(R.string.sign_in_to_chrome);
        }
        setSummary(R.string.sign_in_to_chrome_disabled_summary);
        setFragment(null);
        setIcon(ManagedPreferencesUtils.getManagedByEnterpriseIconId());
        setWidgetLayoutResource(0);
        setViewEnabled(false);
        setOnPreferenceClickListener(pref -> {
            ManagedPreferencesUtils.showManagedByAdministratorToast(getContext());
            return true;
        });
        mWasGenericSigninPromoDisplayed = false;
    }

    private void setupSigninDisallowed() {
        // TODO(https://crbug.com/1133743): Revise the preference behavior.
        mState = State.SIGNIN_DISALLOWED;
        setTitle(R.string.signin_pref_disallowed_title);
        setSummary(null);
        setFragment(null);
        setIcon(AppCompatResources.getDrawable(getContext(), R.drawable.logo_avatar_anonymous));
        setWidgetLayoutResource(0);
        setViewEnabled(false);
        setOnPreferenceClickListener(null);
        mWasGenericSigninPromoDisplayed = false;
    }

    private void setupGenericPromo() {
        mState = State.GENERIC_PROMO;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            setTitle(R.string.sync_promo_turn_on_sync);
        } else {
            setTitle(R.string.sign_in_to_chrome);
        }
        setSummary(R.string.signin_pref_summary);

        setFragment(null);
        setIcon(AppCompatResources.getDrawable(getContext(), R.drawable.logo_avatar_anonymous));
        setWidgetLayoutResource(0);
        setViewEnabled(true);
        setOnPreferenceClickListener(pref
                -> SigninActivityLauncherImpl.get().launchActivityIfAllowed(
                        getContext(), SigninAccessPoint.SETTINGS));

        if (!mWasGenericSigninPromoDisplayed) {
            RecordUserAction.record("Signin_Impression_FromSettings");
        }

        mWasGenericSigninPromoDisplayed = true;
    }

    private void setupSignedIn(String accountName) {
        mState = State.SIGNED_IN;
        DisplayableProfileData profileData = mProfileDataCache.getProfileDataOrDefault(accountName);

        setTitle(profileData.getFullNameOrEmail());
        setSummary(accountName);
        setFragment(AccountManagementFragment.class.getName());
        setIcon(profileData.getImage());
        setWidgetLayoutResource(0);
        setViewEnabled(true);
        setOnPreferenceClickListener(null);

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
    public void onProfileDataUpdated(String accountEmail) {
        update();
    }

    // AccountsChangeObserver implementation.
    @Override
    public void onAccountsChanged() {
        update();
    }
}
