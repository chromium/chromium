// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.UserManager;

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.ui.SignOutDialogFragment;
import org.chromium.chrome.browser.signin.ui.SignOutDialogFragment.SignOutDialogListener;
import org.chromium.chrome.browser.signin.ui.SigninUtils;
import org.chromium.chrome.browser.superviseduser.FilteringBehavior;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.List;

/**
 * The settings screen with information and settings related to the user's accounts.
 *
 * This shows which accounts the user is signed in with, allows the user to sign out of Chrome,
 * links to sync settings, has links to add accounts and go incognito, and shows parental settings
 * if a child account is in use.
 *
 * Note: This can be triggered from a web page, e.g. a GAIA sign-in page.
 */
public class AccountManagementFragment extends PreferenceFragmentCompat
        implements SignOutDialogListener, SignInStateObserver, ProfileDataCache.Observer {
    private static final String TAG = "AcctManagementPref";

    private static final String SIGN_OUT_DIALOG_TAG = "sign_out_dialog_tag";
    private static final String CLEAR_DATA_PROGRESS_DIALOG_TAG = "clear_data_progress";

    /**
     * The key for an integer value in arguments bundle to
     * specify the correct GAIA service that has triggered the dialog.
     * If the argument is not set, GAIA_SERVICE_TYPE_NONE is used as the origin of the dialog.
     */
    private static final String SHOW_GAIA_SERVICE_TYPE_EXTRA = "ShowGAIAServiceType";

    private static final String PREF_ACCOUNTS_CATEGORY = "accounts_category";
    private static final String PREF_PARENTAL_SETTINGS = "parental_settings";
    private static final String PREF_PARENT_ACCOUNTS = "parent_accounts";
    private static final String PREF_CHILD_CONTENT = "child_content";
    private static final String PREF_SIGN_OUT = "sign_out";
    private static final String PREF_SIGN_OUT_DIVIDER = "sign_out_divider";

    private @GAIAServiceType int mGaiaServiceType = GAIAServiceType.GAIA_SERVICE_TYPE_NONE;

    private Profile mProfile;
    private String mSignedInAccountName;
    private ProfileDataCache mProfileDataCache;
    private @Nullable ProfileSyncService.SyncSetupInProgressHandle mSyncSetupInProgressHandle;

    @Override
    public void onCreatePreferences(Bundle savedState, String rootKey) {
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            // Prevent sync settings changes from taking effect until the user leaves this screen.
            mSyncSetupInProgressHandle = syncService.getSetupInProgressHandle();
        }

        if (getArguments() != null) {
            mGaiaServiceType =
                    getArguments().getInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, mGaiaServiceType);
        }

        mProfile = Profile.getLastUsedRegularProfile();

        SigninMetricsUtils.logProfileAccountManagementMenu(
                ProfileAccountManagementMetrics.VIEW, mGaiaServiceType);

        mProfileDataCache = mProfile.isChild()
                ? ProfileDataCache.createWithDefaultImageSize(
                        requireContext(), R.drawable.ic_account_child_20dp)
                : ProfileDataCache.createWithDefaultImageSizeAndNoBadge(requireContext());
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        setDivider(null);

        // Disable animations of preference changes (crbug.com/986401).
        getListView().setItemAnimator(null);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mSyncSetupInProgressHandle != null) {
            mSyncSetupInProgressHandle.close();
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .addSignInStateObserver(this);
        mProfileDataCache.addObserver(this);
        update();
    }

    @Override
    public void onPause() {
        super.onPause();
        mProfileDataCache.removeObserver(this);
        IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .removeSignInStateObserver(this);
    }

    public void update() {
        final Context context = getActivity();
        if (context == null) return;

        if (getPreferenceScreen() != null) getPreferenceScreen().removeAll();

        mSignedInAccountName = CoreAccountInfo.getEmailFrom(
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(
                                ChromeFeatureList.isEnabled(
                                        ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                                        ? ConsentLevel.SIGNIN
                                        : ConsentLevel.SYNC));
        if (mSignedInAccountName == null) {
            // The AccountManagementFragment can only be shown when the user is signed in. If the
            // user is signed out, exit the fragment.
            getActivity().finish();
            return;
        }

        addPreferencesFromResource(R.xml.account_management_preferences);

        String fullName = mProfileDataCache.getProfileDataOrDefault(mSignedInAccountName)
                                  .getFullNameOrEmail();
        getActivity().setTitle(fullName);

        configureSignOutSwitch();
        configureChildAccountPreferences();

        updateAccountsList();
    }

    private boolean canAddAccounts() {
        UserManager userManager =
                (UserManager) getActivity().getSystemService(Context.USER_SERVICE);
        return !userManager.hasUserRestriction(UserManager.DISALLOW_MODIFY_ACCOUNTS);
    }

    private void configureSignOutSwitch() {
        Preference signOutPreference = findPreference(PREF_SIGN_OUT);
        if (mProfile.isChild()) {
            getPreferenceScreen().removePreference(signOutPreference);
            getPreferenceScreen().removePreference(findPreference(PREF_SIGN_OUT_DIVIDER));
        } else {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
                signOutPreference.setLayoutResource(R.layout.account_management_account_row);
                signOutPreference.setIcon(R.drawable.ic_signout_40dp);
            }
            signOutPreference.setTitle(getSignOutPreferenceText());
            signOutPreference.setOnPreferenceClickListener(preference -> {
                if (!isVisible() || !isResumed() || mSignedInAccountName == null) {
                    return false;
                }
                SigninMetricsUtils.logProfileAccountManagementMenu(
                        ProfileAccountManagementMetrics.TOGGLE_SIGNOUT, mGaiaServiceType);

                if (IdentityServicesProvider.get()
                                .getIdentityManager(Profile.getLastUsedRegularProfile())
                                .getPrimaryAccountInfo(ConsentLevel.SYNC)
                        != null) {
                    // Only show the sign-out dialog if the user has given sync consent.
                    SignOutDialogFragment signOutFragment =
                            SignOutDialogFragment.create(mGaiaServiceType);
                    signOutFragment.setTargetFragment(AccountManagementFragment.this, 0);
                    signOutFragment.show(getFragmentManager(), SIGN_OUT_DIALOG_TAG);
                } else {
                    IdentityServicesProvider.get()
                            .getSigninManager(Profile.getLastUsedRegularProfile())
                            .signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, null, false);
                }
                return true;
            });
        }
    }

    private void configureChildAccountPreferences() {
        Preference parentAccounts = findPreference(PREF_PARENT_ACCOUNTS);
        Preference childContent = findPreference(PREF_CHILD_CONTENT);
        if (mProfile.isChild()) {
            PrefService prefService = UserPrefs.get(mProfile);

            String firstParent = prefService.getString(Pref.SUPERVISED_USER_CUSTODIAN_EMAIL);
            String secondParent =
                    prefService.getString(Pref.SUPERVISED_USER_SECOND_CUSTODIAN_EMAIL);
            String parentText;

            if (!secondParent.isEmpty()) {
                parentText = getString(
                        R.string.account_management_two_parent_names, firstParent, secondParent);
            } else if (!firstParent.isEmpty()) {
                parentText = getString(R.string.account_management_one_parent_name, firstParent);
            } else {
                parentText = getString(R.string.account_management_no_parental_data);
            }
            parentAccounts.setSummary(parentText);

            final int childContentSummary;
            int defaultBehavior =
                    prefService.getInteger(Pref.DEFAULT_SUPERVISED_USER_FILTERING_BEHAVIOR);
            if (defaultBehavior == FilteringBehavior.BLOCK) {
                childContentSummary = R.string.account_management_child_content_approved;
            } else if (prefService.getBoolean(Pref.SUPERVISED_USER_SAFE_SITES)) {
                childContentSummary = R.string.account_management_child_content_filter_mature;
            } else {
                childContentSummary = R.string.account_management_child_content_all;
            }
            childContent.setSummary(childContentSummary);

            Drawable newIcon = ApiCompatibilityUtils.getDrawable(
                    getResources(), R.drawable.ic_drive_site_white_24dp);
            newIcon.mutate().setColorFilter(
                    ApiCompatibilityUtils.getColor(getResources(), R.color.default_icon_color),
                    PorterDuff.Mode.SRC_IN);
            childContent.setIcon(newIcon);
        } else {
            PreferenceScreen prefScreen = getPreferenceScreen();
            prefScreen.removePreference(findPreference(PREF_PARENTAL_SETTINGS));
            prefScreen.removePreference(parentAccounts);
            prefScreen.removePreference(childContent);
        }
    }

    private int getSignOutPreferenceText() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            if (!IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount()) {
                // There is no syncing account.
                return R.string.sign_out;
            }
        }
        return R.string.sign_out_and_turn_off_sync;
    }

    private void updateAccountsList() {
        PreferenceCategory accountsCategory =
                (PreferenceCategory) findPreference(PREF_ACCOUNTS_CATEGORY);
        if (accountsCategory == null) return;

        accountsCategory.removeAll();

        accountsCategory.addPreference(
                createAccountPreference(AccountUtils.createAccountFromName(mSignedInAccountName)));
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            accountsCategory.addPreference(
                    createDividerPreference(R.layout.account_divider_preference));
            accountsCategory.addPreference(createManageYourGoogleAccountPreference());
            accountsCategory.addPreference(createDividerPreference(R.layout.divider_preference));
        }

        List<Account> accounts = AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts();
        for (Account account : accounts) {
            if (!mSignedInAccountName.equals(account.name)) {
                accountsCategory.addPreference(createAccountPreference(account));
            }
        }

        if (!mProfile.isChild()) {
            accountsCategory.addPreference(createAddAccountPreference());
        }
    }

    private Preference createAccountPreference(Account account) {
        Preference accountPreference = new Preference(getStyledContext());
        accountPreference.setLayoutResource(R.layout.account_management_account_row);
        accountPreference.setTitle(account.name);
        accountPreference.setIcon(
                mProfileDataCache.getProfileDataOrDefault(account.name).getImage());

        accountPreference.setOnPreferenceClickListener(SyncSettingsUtils.toOnClickListener(
                this, () -> SigninUtils.openSettingsForAccount(getActivity(), account)));

        return accountPreference;
    }

    private Preference createManageYourGoogleAccountPreference() {
        Preference manageYourGoogleAccountPreference = new Preference(getStyledContext());
        manageYourGoogleAccountPreference.setLayoutResource(
                R.layout.account_management_account_row);
        manageYourGoogleAccountPreference.setTitle(R.string.manage_your_google_account);
        manageYourGoogleAccountPreference.setIcon(R.drawable.ic_google_services_48dp);
        manageYourGoogleAccountPreference.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(
                        this, () -> SyncSettingsUtils.openGoogleMyAccount(getActivity())));

        return manageYourGoogleAccountPreference;
    }

    private Preference createDividerPreference(@LayoutRes int layoutResId) {
        Preference dividerPreference = new Preference(getStyledContext());
        dividerPreference.setLayoutResource(layoutResId);

        return dividerPreference;
    }

    private ChromeBasePreference createAddAccountPreference() {
        ChromeBasePreference addAccountPreference = new ChromeBasePreference(getStyledContext());
        addAccountPreference.setLayoutResource(R.layout.account_management_account_row);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            addAccountPreference.setIcon(R.drawable.ic_person_add_40dp);
            addAccountPreference.setTitle(R.string.signin_add_account_to_device);
        } else {
            addAccountPreference.setIcon(R.drawable.ic_add_circle_40dp);
            addAccountPreference.setTitle(R.string.account_management_add_account_title);
        }
        addAccountPreference.setOnPreferenceClickListener(preference -> {
            if (!isVisible() || !isResumed()) return false;

            SigninMetricsUtils.logProfileAccountManagementMenu(
                    ProfileAccountManagementMetrics.ADD_ACCOUNT, mGaiaServiceType);

            AccountManagerFacade accountManagerFacade = AccountManagerFacadeProvider.getInstance();
            accountManagerFacade.createAddAccountIntent((@Nullable Intent intent) -> {
                if (!isVisible() || !isResumed()) return;

                if (intent != null) {
                    startActivity(intent);
                } else {
                    // AccountManagerFacade couldn't create intent, use SigninUtils to open
                    // settings instead.
                    SigninUtils.openSettingsForAllAccounts(getActivity());
                }

                // Return to the last opened tab if triggered from the content area.
                if (mGaiaServiceType != GAIAServiceType.GAIA_SERVICE_TYPE_NONE) {
                    if (isAdded()) getActivity().finish();
                }
            });
            return true;
        });
        addAccountPreference.setManagedPreferenceDelegate(
                (ChromeManagedPreferenceDelegate) preference -> !canAddAccounts());
        return addAccountPreference;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    // ProfileDataCache.Observer implementation:
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        updateAccountsList();
    }

    // SignOutDialogListener implementation:
    @Override
    public void onSignOutClicked(boolean forceWipeUserData) {
        // In case the user reached this fragment without being signed in, we guard the sign out so
        // we do not hit a native crash.
        if (IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN)
                == null) {
            return;
        }
        final DialogFragment clearDataProgressDialog = new ClearDataProgressDialog();
        IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                        new SigninManager.SignOutCallback() {
                            @Override
                            public void preWipeData() {
                                clearDataProgressDialog.show(
                                        getFragmentManager(), CLEAR_DATA_PROGRESS_DIALOG_TAG);
                            }

                            @Override
                            public void signOutComplete() {
                                if (clearDataProgressDialog.isAdded()) {
                                    clearDataProgressDialog.dismissAllowingStateLoss();
                                }
                            }
                        },
                        forceWipeUserData);
    }

    // SignInStateObserver implementation:

    @Override
    public void onSignedIn() {
        update();
    }

    @Override
    public void onSignedOut() {
        update();
    }

    /**
     * Open the account management UI.
     * @param serviceType A signin::GAIAServiceType that triggered the dialog.
     */
    public static void openAccountManagementScreen(
            Context context, @GAIAServiceType int serviceType) {
        Bundle arguments = new Bundle();
        arguments.putInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, serviceType);
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(
                context, AccountManagementFragment.class, arguments);
    }
}
