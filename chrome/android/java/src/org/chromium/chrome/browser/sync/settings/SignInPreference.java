// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.ViewUtils;

/**
 * A preference that displays "Sign in to Chrome" when the user is not sign in, and displays the
 * user's name, email, profile image and sync error icon if necessary when the user is signed in.
 */
public class SignInPreference extends Preference
        implements SignInStateObserver,
                ProfileDataCache.Observer,
                SyncService.SyncStateChangedListener,
                AccountsChangeObserver {
    private boolean mWasGenericSigninPromoDisplayed;
    private boolean mViewEnabled;
    private boolean mIsShowingSigninPromo;
    private boolean mShowAlertIcon;

    private Profile mProfile;
    private PrefService mPrefService;
    private ProfileDataCache mProfileDataCache;
    private AccountManagerFacade mAccountManagerFacade;
    private SyncService mSyncService;
    private SigninManager mSigninManager;
    private IdentityManager mIdentityManager;

    public ProfileDataCache getProfileDataCache() {
        return mProfileDataCache;
    }

    /** Constructor for inflating from XML. */
    public SignInPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.account_management_account_row);
        setViewId(R.id.account_management_account_row);
        mIsShowingSigninPromo = false;
    }

    /**
     * Initialize the dependencies for the SignInPreference.
     *
     * <p>Must be called before the preference is attached, which is called from the containing
     * settings screen's onViewCreated method.
     */
    public void initialize(
            Profile profile,
            ProfileDataCache profileDataCache,
            AccountManagerFacade accountManagerFacade) {
        mProfile = profile;
        mProfileDataCache = profileDataCache;
        mAccountManagerFacade = accountManagerFacade;
        mPrefService = UserPrefs.get(mProfile);
        mSyncService = SyncServiceFactory.getForProfile(mProfile);
        mSigninManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        mIdentityManager = IdentityServicesProvider.get().getIdentityManager(mProfile);
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
            // TODO(crbug.com/40722691): Clean up after revising isSigninDisabledByPolicy.
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
        setFragment(null);
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            setTitle(R.string.signin_settings_title);
            setSummary(R.string.settings_signin_disabled_by_administrator);
            setIcon(R.drawable.ic_business_small_with_bg);
        } else {
            setTitle(R.string.sync_promo_turn_on_sync);
            setSummary(R.string.sign_in_to_chrome_disabled_summary);
            setIcon(ManagedPreferencesUtils.getManagedByEnterpriseIconId());
        }
        setViewEnabledAndShowAlertIcon(/* enabled= */ false, /* alertIconVisible= */ false);
        setOnPreferenceClickListener(
                pref -> {
                    ManagedPreferencesUtils.showManagedByAdministratorToast(getContext());
                    return true;
                });
        mWasGenericSigninPromoDisplayed = false;
    }

    private void setupSigninDisallowed() {
        mWasGenericSigninPromoDisplayed = false;
    }

    private void setupGenericPromo() {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            setTitle(R.string.signin_settings_title);
            setSummary(R.string.signin_settings_subtitle);
        } else {
            setTitle(R.string.sync_promo_turn_on_sync);
            setSummary(R.string.signin_pref_summary);
        }

        setFragment(null);
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            setIcon(
                    AppCompatResources.getDrawable(
                            getContext(), R.drawable.account_circle_with_bg));
        } else {
            setIcon(AppCompatResources.getDrawable(getContext(), R.drawable.logo_avatar_anonymous));
        }
        setViewEnabledAndShowAlertIcon(/* enabled= */ true, /* alertIconVisible= */ false);
        OnPreferenceClickListener clickListener =
                pref -> {
                    if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
                        AccountPickerBottomSheetStrings bottomSheetStrings =
                                new AccountPickerBottomSheetStrings.Builder(
                                                R.string.signin_account_picker_bottom_sheet_title)
                                        .build();
                        SigninAndHistorySyncActivityLauncherImpl.get()
                                .launchActivityIfAllowed(
                                        getContext(),
                                        mProfile,
                                        bottomSheetStrings,
                                        SigninAndHistorySyncCoordinator.NoAccountSigninMode
                                                .BOTTOM_SHEET,
                                        SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                                .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                        SigninAndHistorySyncCoordinator.HistoryOptInMode.OPTIONAL,
                                        SigninAccessPoint.SETTINGS);
                    } else {
                        SyncConsentActivityLauncherImpl.get()
                                .launchActivityIfAllowed(
                                        getContext(), SigninAccessPoint.SETTINGS_SYNC_OFF_ROW);
                    }
                    return true;
                };
        setOnPreferenceClickListener(clickListener);

        if (!mWasGenericSigninPromoDisplayed) {
            RecordUserAction.record("Signin_Impression_FromSettings");
        }

        mWasGenericSigninPromoDisplayed = true;
    }

    private void setupSignedIn(String accountName) {
        DisplayableProfileData profileData = mProfileDataCache.getProfileDataOrDefault(accountName);
        final boolean canShowEmailAddress = profileData.hasDisplayableEmailAddress();
        setSummary(canShowEmailAddress ? accountName : "");
        setTitle(
                SyncSettingsUtils.getDisplayableFullNameOrEmailWithPreference(
                        profileData, getContext(), SyncSettingsUtils.TitlePreference.FULL_NAME));
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                && !mSyncService.hasSyncConsent()) {
            setFragment(ManageSyncSettings.class.getName());
        } else {
            setFragment(AccountManagementFragment.class.getName());
        }
        setIcon(profileData.getImage());
        setViewEnabledAndShowAlertIcon(
                /* enabled= */ true,
                /* alertIconVisible= */ SyncSettingsUtils.getIdentityError(mProfile)
                        != SyncError.NO_ERROR);
        setOnPreferenceClickListener(null);

        mWasGenericSigninPromoDisplayed = false;
    }

    // This just changes visual representation. Actual enabled flag in preference stays
    // always true to receive clicks (necessary to show "Managed by administrator" toast). This also
    // sets the visibility of the alert icon.
    private void setViewEnabledAndShowAlertIcon(boolean enabled, boolean alertIconVisible) {
        assert enabled || !alertIconVisible
                : "Alert icon should not be made visible if the view is disabled.";
        if (mViewEnabled == enabled && mShowAlertIcon == alertIconVisible) {
            return;
        }
        mViewEnabled = enabled;
        mShowAlertIcon = alertIconVisible;
        notifyChanged();
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        ViewUtils.setEnabledRecursive(holder.itemView, mViewEnabled);

        ImageView alertIcon = (ImageView) holder.findViewById(R.id.alert_icon);
        alertIcon.setVisibility(mShowAlertIcon ? View.VISIBLE : View.GONE);
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
