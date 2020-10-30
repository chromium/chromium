// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninManager.SignInAllowedObserver;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.chrome.browser.signin.SigninPromoUtil;
import org.chromium.chrome.browser.signin.SigninUtils;
import org.chromium.chrome.browser.sync.AndroidSyncSettings;
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
    @IntDef({State.SIGNIN_DISABLED_BY_POLICY, State.SIGNIN_DISALLOWED, State.GENERIC_PROMO,
            State.PERSONALIZED_PROMO, State.SIGNED_IN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        int SIGNIN_DISABLED_BY_POLICY = 0;
        int SIGNIN_DISALLOWED = 1;
        int GENERIC_PROMO = 2;
        int PERSONALIZED_PROMO = 3;
        int SIGNED_IN = 4;
    }

    private final PrefService mPrefService;
    private boolean mPersonalizedPromoEnabled = true;
    private boolean mWasGenericSigninPromoDisplayed;
    private boolean mViewEnabled;
    private @Nullable SigninPromoController mSigninPromoController;
    private final ProfileDataCache mProfileDataCache;
    private final AccountManagerFacade mAccountManagerFacade;
    private @State int mState;
    private @Nullable Runnable mStateChangedCallback;
    private boolean mObserversAdded;

    /**
     * Constructor for inflating from XML.
     */
    public SignInPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        mPrefService = UserPrefs.get(Profile.getLastUsedRegularProfile());

        int imageSize = context.getResources().getDimensionPixelSize(R.dimen.user_picture_size);
        mProfileDataCache = new ProfileDataCache(context, imageSize);
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
        AndroidSyncSettings.get().registerObserver(this);
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.addSyncStateChangedListener(this);
        }
        mObserversAdded = true;

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
        AndroidSyncSettings.get().unregisterObserver(this);
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.removeSyncStateChangedListener(this);
        }
        mObserversAdded = false;
    }

    /**
     * Should be called when the {@link PreferenceFragmentCompat} which used {@link
     * SignInPreference} gets destroyed. Used to record "ImpressionsTilDismiss" histogram.
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

        @ConsentLevel
        int consentLevel =
                ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                ? ConsentLevel.NOT_REQUIRED
                : ConsentLevel.SYNC;
        CoreAccountInfo accountInfo =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(consentLevel);
        if (accountInfo != null) {
            setupSignedIn(accountInfo.getEmail());
            return;
        }

        boolean personalizedPromoDismissed = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED, false);
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

    private void setupSigninDisabledByPolicy() {
        setState(State.SIGNIN_DISABLED_BY_POLICY);
        setLayoutResource(R.layout.account_management_account_row);
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
        mSigninPromoController = null;
        mWasGenericSigninPromoDisplayed = false;
    }

    private void setupSigninDisallowed() {
        // TODO(https://crbug.com/1133743): Revise the preference behavior.
        setState(State.SIGNIN_DISALLOWED);
        setLayoutResource(R.layout.account_management_account_row);
        setTitle(R.string.signin_pref_disallowed_title);
        setSummary(null);
        setFragment(null);
        setIcon(AppCompatResources.getDrawable(getContext(), R.drawable.logo_avatar_anonymous));
        setWidgetLayoutResource(0);
        setViewEnabled(false);
        setOnPreferenceClickListener(null);
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
        setOnPreferenceClickListener(null);

        if (mSigninPromoController == null) {
            mSigninPromoController = new SigninPromoController(SigninAccessPoint.SETTINGS);
        }

        mWasGenericSigninPromoDisplayed = false;
        notifyChanged();
    }

    private void setupGenericPromo() {
        setState(State.GENERIC_PROMO);
        setLayoutResource(R.layout.account_management_account_row);
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
                -> SigninUtils.startSigninActivityIfAllowed(
                        getContext(), SigninAccessPoint.SETTINGS));
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
        setOnPreferenceClickListener(null);

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
        SigninPromoUtil.setupSigninPromoViewFromCache(
                mSigninPromoController, mProfileDataCache, signinPromoView, () -> {
                    SharedPreferencesManager.getInstance().writeBoolean(
                            ChromePreferenceKeys.SIGNIN_PROMO_SETTINGS_PERSONALIZED_DISMISSED,
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
