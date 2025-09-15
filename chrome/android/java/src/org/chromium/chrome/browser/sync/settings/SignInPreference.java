// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.Intent;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserActionableError;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.ViewUtils;

/**
 * A preference that displays "Sign in to Chrome" when the user is not sign in, and displays the
 * user's name, email, profile image and sync error icon if necessary when the user is signed in.
 */
@NullMarked
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
    private @Nullable SyncService mSyncService;
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
    @Initializer
    public void initialize(
            Profile profile,
            ProfileDataCache profileDataCache,
            AccountManagerFacade accountManagerFacade) {
        mProfile = profile;
        mProfileDataCache = profileDataCache;
        mAccountManagerFacade = accountManagerFacade;
        mPrefService = UserPrefs.get(mProfile);
        mSyncService = SyncServiceFactory.getForProfile(mProfile);
        mSigninManager = assumeNonNull(IdentityServicesProvider.get().getSigninManager(mProfile));
        mIdentityManager =
                assumeNonNull(IdentityServicesProvider.get().getIdentityManager(mProfile));
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
        if (!mPrefService.getBoolean(Pref.SIGNIN_ALLOWED)) {
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
        setTitle(R.string.signin_settings_title);
        setSummary(R.string.settings_signin_disabled_by_administrator);
        setIcon(R.drawable.ic_business_small_with_bg);
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
        setTitle(R.string.signin_settings_title);
        setSummary(R.string.signin_settings_subtitle);

        setFragment(null);
        setIcon(AppCompatResources.getDrawable(getContext(), R.drawable.account_circle_with_bg));
        setViewEnabledAndShowAlertIcon(/* enabled= */ true, /* alertIconVisible= */ false);
        OnPreferenceClickListener clickListener =
                pref -> {
                    AccountPickerBottomSheetStrings bottomSheetStrings =
                            new AccountPickerBottomSheetStrings.Builder(
                                            getContext()
                                                    .getString(
                                                            R.string
                                                                    .signin_account_picker_bottom_sheet_title))
                                    .build();
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            bottomSheetStrings,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.OPTIONAL,
                                            getContext().getString(R.string.history_sync_title),
                                            getContext().getString(R.string.history_sync_subtitle))
                                    .build();

                    @Nullable Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            getContext(),
                                            mProfile,
                                            config,
                                            SigninAccessPoint.SETTINGS);
                    if (intent != null) {
                        getContext().startActivity(intent);
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
        if (!assumeNonNull(mSyncService).hasSyncConsent()) {
            setFragment(ManageSyncSettings.class.getName());
        } else {
            setFragment(AccountManagementFragment.class.getName());
        }
        setIcon(profileData.getImage());
        setViewEnabledAndShowAlertIcon(
                /* enabled= */ true,
                /* alertIconVisible= */ SyncSettingsUtils.getSyncError(mProfile)
                        != UserActionableError.NONE);
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
