// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.ViewUtils;

/**
 * A preference that displays "Sign in to Chrome" when the user is not sign in, and displays
 * the user's name, email, profile image and sync error icon if necessary when the user is signed
 * in.
 */
public class SignInPreference
        extends Preference implements SignInStateObserver, ProfileDataCache.Observer,
                                      SyncService.SyncStateChangedListener, AccountsChangeObserver {
    private boolean mWasGenericSigninPromoDisplayed;
    private boolean mViewEnabled;
    private boolean mIsShowingSigninPromo;

    private PrefService mPrefService;
    private ProfileDataCache mProfileDataCache;
    private AccountManagerFacade mAccountManagerFacade;
    private SyncService mSyncService;
    private SigninManager mSigninManager;
    private IdentityManager mIdentityManager;

    public ProfileDataCache getProfileDataCache() {
        return mProfileDataCache;
    }

    /**
     * Constructor for inflating from XML.
     */
    public SignInPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.account_management_account_row);
        mIsShowingSigninPromo = false;
    }

    /**
     * Initialize the dependencies for the SignInPreference.
     *
     * Must be called before the preference is attached, which is called from the containing
     * settings screen's onViewCreated method.
     */
    public void initialize(ProfileDataCache profileDataCache,
            AccountManagerFacade accountManagerFacade, PrefService prefService,
            SyncService syncService, SigninManager signinManager, IdentityManager identityManager) {
        mProfileDataCache = profileDataCache;
        mAccountManagerFacade = accountManagerFacade;
        mPrefService = prefService;
        mSyncService = syncService;
        mSigninManager = signinManager;
        mIdentityManager = identityManager;
    }

    @Override
    public void onAttached() {
        super.onAttached();

        mAccountManagerFacade.addObserver(this);
        mSigninManager.addSignInStateObserver(this);
        mProfileDataCache.addObserver(this);
        if (mSyncService != null) {
            mSyncService.addSyncStateChangedListener(this);
        }

        update();
    }

    @Override
    public void onDetached() {
        super.onDetached();

        mAccountManagerFacade.removeObserver(this);
        mSigninManager.removeSignInStateObserver(this);
        mProfileDataCache.removeObserver(this);
        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
        }
    }

    /**
     * Sets whether Personalized Signin Promo is being shown in {@link
     * org.chromium.chrome.browser.settings.MainSettings} page
     */
    public void setIsShowingPersonalizedSigninPromo(boolean isShowingSigninPromo) {
        mIsShowingSigninPromo = isShowingSigninPromo;
        update();
    }

    /** Updates the title, summary, and image based on the current sign-in state. */
    private void update() {
        setVisible(!mIsShowingSigninPromo);
        if (mSigninManager.isSigninDisabledByPolicy()) {
            // TODO(https://crbug.com/1133739): Clean up after revising isSigninDisabledByPolicy.
            if (mPrefService.isManagedPreference(Pref.SIGNIN_ALLOWED)) {
                setupSigninDisabledByPolicy();
            } else {
                setupSigninDisallowed();
                assert !mIsShowingSigninPromo
                    : "Signin Promo should not be shown when signin is not allowed";
                setVisible(false);
            }
            return;
        }

        CoreAccountInfo accountInfo = mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (accountInfo != null) {
            setupSignedIn(accountInfo.getEmail());
            return;
        }

        setupGenericPromo();
    }

    private void setupSigninDisabledByPolicy() {
        setTitle(R.string.sync_promo_turn_on_sync);
        setSummary(R.string.sign_in_to_chrome_disabled_summary);
        setFragment(null);
        setIcon(ManagedPreferencesUtils.getManagedByEnterpriseIconId());
        setViewEnabled(false);
        setOnPreferenceClickListener(pref -> {
            ManagedPreferencesUtils.showManagedByAdministratorToast(getContext());
            return true;
        });
        mWasGenericSigninPromoDisplayed = false;
    }

    private void setupSigninDisallowed() {
        mWasGenericSigninPromoDisplayed = false;
    }

    private void setupGenericPromo() {
        setTitle(R.string.sync_promo_turn_on_sync);
        setSummary(R.string.signin_pref_summary);

        setFragment(null);
        setIcon(AppCompatResources.getDrawable(getContext(), R.drawable.logo_avatar_anonymous));
        setViewEnabled(true);
        setOnPreferenceClickListener(pref
                -> SyncConsentActivityLauncherImpl.get().launchActivityIfAllowed(
                        getContext(), SigninAccessPoint.SETTINGS_SYNC_OFF_ROW));

        if (!mWasGenericSigninPromoDisplayed) {
            RecordUserAction.record("Signin_Impression_FromSettings");
        }

        mWasGenericSigninPromoDisplayed = true;
    }

    private void setupSignedIn(String accountName) {
        DisplayableProfileData profileData = mProfileDataCache.getProfileDataOrDefault(accountName);
        final boolean canShowEmailAddress = profileData.hasDisplayableEmailAddress();
        setSummary(canShowEmailAddress ? accountName : "");
        setTitle(SyncSettingsUtils.getDisplayableFullNameOrEmailWithPreference(
                profileData, getContext(), SyncSettingsUtils.TitlePreference.FULL_NAME));
        setFragment(AccountManagementFragment.class.getName());
        setIcon(profileData.getImage());
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

    // SyncService.SyncStateChangedListener implementation.
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
    public void onCoreAccountInfosChanged() {
        update();
    }
}
