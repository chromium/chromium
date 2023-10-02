// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.UserManager;

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.SignOutDialogCoordinator;
import org.chromium.chrome.browser.ui.signin.SignOutDialogCoordinator.Listener;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

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
public class AccountManagementFragment extends ChromeBaseSettingsFragment
        implements Listener, SignInStateObserver, ProfileDataCache.Observer, CustomDividerFragment {
    private static final String CLEAR_DATA_PROGRESS_DIALOG_TAG = "clear_data_progress";

    /**
     * The key for an integer value in arguments bundle to
     * specify the correct GAIA service that has triggered the dialog.
     * If the argument is not set, GAIA_SERVICE_TYPE_NONE is used as the origin of the dialog.
     */
    private static final String SHOW_GAIA_SERVICE_TYPE_EXTRA = "ShowGAIAServiceType";

    private static final String PREF_ACCOUNTS_CATEGORY = "accounts_category";
    private static final String PREF_PARENT_ACCOUNT_CATEGORY = "parent_account_category";
    private static final String PREF_SIGN_OUT = "sign_out";
    private static final String PREF_SIGN_OUT_DIVIDER = "sign_out_divider";

    private @GAIAServiceType int mGaiaServiceType = GAIAServiceType.GAIA_SERVICE_TYPE_NONE;

    private CoreAccountInfo mSignedInCoreAccountInfo;
    private ProfileDataCache mProfileDataCache;
    private @Nullable SyncService.SyncSetupInProgressHandle mSyncSetupInProgressHandle;

    @Override
    public void onCreatePreferences(Bundle savedState, String rootKey) {
        SyncService syncService = SyncServiceFactory.getForProfile(getProfile());
        if (syncService != null) {
            // Prevent sync settings changes from taking effect until the user leaves this screen.
            mSyncSetupInProgressHandle = syncService.getSetupInProgressHandle();
        }

        if (getArguments() != null) {
            mGaiaServiceType =
                    getArguments().getInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, mGaiaServiceType);
        }

        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(requireContext());
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Disable animations of preference changes (crbug.com/986401).
        getListView().setItemAnimator(null);
    }

    @Override
    public boolean hasDivider() {
        return false;
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
        IdentityServicesProvider.get().getSigninManager(getProfile()).addSignInStateObserver(this);
        mProfileDataCache.addObserver(this);
        update();
    }

    @Override
    public void onPause() {
        super.onPause();
        mProfileDataCache.removeObserver(this);
        IdentityServicesProvider.get()
                .getSigninManager(getProfile())
                .removeSignInStateObserver(this);
    }

    public void update() {
        final Context context = getActivity();
        if (context == null) return;

        if (getPreferenceScreen() != null) getPreferenceScreen().removeAll();

        String signedInAccountEmail =
                CoreAccountInfo.getEmailFrom(IdentityServicesProvider.get()
                                                     .getIdentityManager(getProfile())
                                                     .getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        List<CoreAccountInfo> coreAccountInfos = AccountUtils.getCoreAccountInfosIfFulfilledOrEmpty(
                AccountManagerFacadeProvider.getInstance().getCoreAccountInfos());
        if (signedInAccountEmail == null || coreAccountInfos.isEmpty()) {
            // The AccountManagementFragment can only be shown when the user is signed in. If the
            // user is signed out, exit the fragment.
            getActivity().finish();
            return;
        }
        mSignedInCoreAccountInfo =
                AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, signedInAccountEmail);
        assert mSignedInCoreAccountInfo != null;

        DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(mSignedInCoreAccountInfo.getEmail());
        getActivity().setTitle(SyncSettingsUtils.getDisplayableFullNameOrEmailWithPreference(
                profileData, getContext(), SyncSettingsUtils.TitlePreference.FULL_NAME));
        addPreferencesFromResource(R.xml.account_management_preferences);
        configureSignOutSwitch();
        configureChildAccountPreferences();
        AccountManagerFacadeProvider.getInstance().getCoreAccountInfos().then(
                this::updateAccountsList);
    }

    /**
     * The ProfileDataCache object needs to be accessible in some tests, for example in order to
     * await the completion of async population of the cache.
     */
    public ProfileDataCache getProfileDataCacheForTesting() {
        return mProfileDataCache;
    }

    private boolean canAddAccounts() {
        UserManager userManager =
                (UserManager) getActivity().getSystemService(Context.USER_SERVICE);
        return !userManager.hasUserRestriction(UserManager.DISALLOW_MODIFY_ACCOUNTS);
    }

    private void configureSignOutSwitch() {
        Preference signOutPreference = findPreference(PREF_SIGN_OUT);
        if (getProfile().isChild()) {
            getPreferenceScreen().removePreference(signOutPreference);
            getPreferenceScreen().removePreference(findPreference(PREF_SIGN_OUT_DIVIDER));
        } else {
            signOutPreference.setLayoutResource(R.layout.account_management_account_row);
            signOutPreference.setIcon(R.drawable.ic_signout_40dp);
            signOutPreference.setTitle(IdentityServicesProvider.get()
                                               .getIdentityManager(getProfile())
                                               .hasPrimaryAccount(ConsentLevel.SYNC)
                            ? R.string.sign_out_and_turn_off_sync
                            : R.string.sign_out);
            signOutPreference.setOnPreferenceClickListener(preference -> {
                if (!isVisible() || !isResumed() || mSignedInCoreAccountInfo == null) {
                    return false;
                }

                if (IdentityServicesProvider.get()
                                .getIdentityManager(getProfile())
                                .getPrimaryAccountInfo(ConsentLevel.SYNC)
                        != null) {
                    // Only show the sign-out dialog if the user has given sync consent.
                    SignOutDialogCoordinator.show(requireContext(),
                            ((ModalDialogManagerHolder) getActivity()).getModalDialogManager(),
                            this, SignOutDialogCoordinator.ActionType.CLEAR_PRIMARY_ACCOUNT,
                            mGaiaServiceType);
                } else {
                    IdentityServicesProvider.get()
                            .getSigninManager(getProfile())
                            .signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, null, false);
                }
                return true;
            });
        }
    }

    private void configureChildAccountPreferences() {
        Preference parentAccounts = findPreference(PREF_PARENT_ACCOUNT_CATEGORY);
        if (getProfile().isChild()) {
            PrefService prefService = UserPrefs.get(getProfile());

            String firstParent = prefService.getString(Pref.SUPERVISED_USER_CUSTODIAN_EMAIL);
            String secondParent =
                    prefService.getString(Pref.SUPERVISED_USER_SECOND_CUSTODIAN_EMAIL);
            final String parentText;

            if (!secondParent.isEmpty()) {
                parentText = getString(R.string.account_management_header_two_parent_names,
                        firstParent, secondParent);
            } else if (!firstParent.isEmpty()) {
                parentText =
                        getString(R.string.account_management_header_one_parent_name, firstParent);
            } else {
                parentText = getString(R.string.account_management_header_no_parental_data);
            }
            parentAccounts.setSummary(parentText);
        } else {
            PreferenceScreen prefScreen = getPreferenceScreen();
            prefScreen.removePreference(findPreference(PREF_PARENT_ACCOUNT_CATEGORY));
        }
    }

    private void updateAccountsList(List<CoreAccountInfo> coreAccountInfos) {
        // This method is called asynchronously on accounts fetched from AccountManagerFacade.
        // Make sure the fragment is alive before updating preferences.
        if (!isResumed()) return;

        setAccountBadges(coreAccountInfos);

        PreferenceCategory accountsCategory = findPreference(PREF_ACCOUNTS_CATEGORY);
        if (accountsCategory == null) {
            // This pref is dynamically added/removed many times, so it might not be present by now.
            // More details can be found in crbug/1221491.
            return;
        }
        accountsCategory.removeAll();

        accountsCategory.addPreference(createAccountPreference(mSignedInCoreAccountInfo));
        accountsCategory.addPreference(
                createDividerPreference(R.layout.account_divider_preference));
        accountsCategory.addPreference(createManageYourGoogleAccountPreference());
        accountsCategory.addPreference(createDividerPreference(R.layout.divider_preference));

        for (CoreAccountInfo coreAccountInfo : coreAccountInfos) {
            if (!mSignedInCoreAccountInfo.equals(coreAccountInfo)) {
                accountsCategory.addPreference(createAccountPreference(coreAccountInfo));
            }
        }
        accountsCategory.addPreference(createAddAccountPreference());
    }

    private Preference createAccountPreference(CoreAccountInfo coreAccountInfo) {
        Preference accountPreference = new Preference(getStyledContext());
        accountPreference.setLayoutResource(R.layout.account_management_account_row);

        DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(coreAccountInfo.getEmail());
        accountPreference.setTitle(SyncSettingsUtils.getDisplayableFullNameOrEmailWithPreference(
                profileData, getContext(), SyncSettingsUtils.TitlePreference.EMAIL));
        accountPreference.setIcon(profileData.getImage());

        accountPreference.setOnPreferenceClickListener(SyncSettingsUtils.toOnClickListener(this,
                ()
                        -> SigninUtils.openSettingsForAccount(
                                getActivity(), coreAccountInfo.getEmail())));

        return accountPreference;
    }

    private Preference createManageYourGoogleAccountPreference() {
        Preference manageYourGoogleAccountPreference = new Preference(getStyledContext());
        manageYourGoogleAccountPreference.setLayoutResource(
                R.layout.account_management_account_row);
        manageYourGoogleAccountPreference.setTitle(R.string.manage_your_google_account);
        manageYourGoogleAccountPreference.setIcon(R.drawable.ic_google_services_48dp);
        manageYourGoogleAccountPreference.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(this, () -> {
                    assert IdentityServicesProvider.get()
                            .getIdentityManager(getProfile())
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
                    SyncSettingsUtils.openGoogleMyAccount(getActivity());
                }));

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
        addAccountPreference.setIcon(R.drawable.ic_person_add_40dp);
        addAccountPreference.setTitle(R.string.signin_add_account_to_device);
        addAccountPreference.setOnPreferenceClickListener(preference -> {
            if (!isVisible() || !isResumed()) return false;

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
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return !canAddAccounts();
                    }
                });
        return addAccountPreference;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    private void setAccountBadges(List<CoreAccountInfo> coreAccountInfos) {
        for (CoreAccountInfo coreAccountInfo : coreAccountInfos) {
            Account account = CoreAccountInfo.getAndroidAccountFrom(coreAccountInfo);
            AccountManagerFacadeProvider.getInstance().checkChildAccountStatus(
                    account, (isChild, childAccount) -> {
                        if (isChild) {
                            mProfileDataCache.setBadge(
                                    childAccount.name, R.drawable.ic_account_child_20dp);
                        }
                    });
        }
    }

    // ProfileDataCache.Observer implementation:
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        AccountManagerFacadeProvider.getInstance().getCoreAccountInfos().then(
                this::updateAccountsList);
    }

    // SignOutDialogListener implementation:
    @Override
    public void onSignOutClicked(boolean forceWipeUserData) {
        // In case the user reached this fragment without being signed in, we guard the sign out so
        // we do not hit a native crash.
        if (!IdentityServicesProvider.get()
                        .getIdentityManager(getProfile())
                        .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            return;
        }
        final DialogFragment clearDataProgressDialog = new ClearDataProgressDialog();
        IdentityServicesProvider.get()
                .getSigninManager(getProfile())
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
