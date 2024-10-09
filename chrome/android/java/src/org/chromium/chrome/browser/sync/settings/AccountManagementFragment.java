// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.UserManager;

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SignOutCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
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
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

import java.util.List;

/**
 * The settings screen with information and settings related to the user's accounts.
 *
 * <p>This shows which accounts the user is signed in with, allows the user to sign out of Chrome,
 * links to sync settings, has links to add accounts and go incognito, and shows parental settings
 * if a child account is in use.
 *
 * <p>Note: This can be triggered from a web page, e.g. a GAIA sign-in page.
 */
public class AccountManagementFragment extends ChromeBaseSettingsFragment
        implements SignInStateObserver,
                ProfileDataCache.Observer,
                CustomDividerFragment,
                IdentityErrorCardPreference.Listener,
                PassphraseDialogFragment.Delegate {
    private static final int REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL = 1;
    private static final int REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED = 2;

    @VisibleForTesting public static final String FRAGMENT_ENTER_PASSPHRASE = "enter_passphase";

    /**
     * The key for an integer value in arguments bundle to specify the correct GAIA service that has
     * triggered the dialog. If the argument is not set, GAIA_SERVICE_TYPE_NONE is used as the
     * origin of the dialog.
     */
    private static final String SHOW_GAIA_SERVICE_TYPE_EXTRA = "ShowGAIAServiceType";

    private static final String PREF_IDENTITY_ERROR_CARD_PREFERENCE = "identity_error_card";
    private static final String PREF_ACCOUNTS_CATEGORY = "accounts_category";
    private static final String PREF_PARENT_ACCOUNT_CATEGORY = "parent_account_category";
    private static final String PREF_SIGN_OUT = "sign_out";
    private static final String PREF_SIGN_OUT_DIVIDER = "sign_out_divider";

    private @GAIAServiceType int mGaiaServiceType = GAIAServiceType.GAIA_SERVICE_TYPE_NONE;

    private IdentityErrorCardPreference mIdentityErrorCardPreference;

    private CoreAccountInfo mSignedInCoreAccountInfo;
    private ProfileDataCache mProfileDataCache;
    private SyncService mSyncService;
    private @Nullable SyncService.SyncSetupInProgressHandle mSyncSetupInProgressHandle;
    private OneshotSupplier<SnackbarManager> mSnackbarManagerSupplier;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedState, String rootKey) {
        mSyncService = SyncServiceFactory.getForProfile(getProfile());
        if (mSyncService != null) {
            // Prevent sync settings changes from taking effect until the user leaves this screen.
            mSyncSetupInProgressHandle = mSyncService.getSetupInProgressHandle();
        }

        if (getArguments() != null) {
            mGaiaServiceType =
                    getArguments().getInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, mGaiaServiceType);
        }

        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(requireContext());
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
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

        mSignedInCoreAccountInfo =
                IdentityServicesProvider.get()
                        .getIdentityManager(getProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        List<CoreAccountInfo> coreAccountInfos =
                AccountUtils.getCoreAccountInfosIfFulfilledOrEmpty(
                        AccountManagerFacadeProvider.getInstance().getCoreAccountInfos());
        if (mSignedInCoreAccountInfo == null || coreAccountInfos.isEmpty()) {
            // The AccountManagementFragment can only be shown when the user is signed in. If the
            // user is signed out, exit the fragment.
            SettingsNavigationFactory.createSettingsNavigation().finishCurrentSettings(this);
            return;
        }

        DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(mSignedInCoreAccountInfo.getEmail());
        mPageTitle.set(
                SyncSettingsUtils.getDisplayableFullNameOrEmailWithPreference(
                        profileData, getContext(), SyncSettingsUtils.TitlePreference.FULL_NAME));
        addPreferencesFromResource(R.xml.account_management_preferences);
        configureSignOutSwitch();
        configureChildAccountPreferences();
        AccountManagerFacadeProvider.getInstance()
                .getCoreAccountInfos()
                .then(this::updateAccountsList);

        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            // TODO(crbug.com/40944114): Figure out the behaviour for child accounts.
            mIdentityErrorCardPreference =
                    (IdentityErrorCardPreference)
                            findPreference(PREF_IDENTITY_ERROR_CARD_PREFERENCE);
            mIdentityErrorCardPreference.initialize(getProfile(), this);
        }
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

    public void setSnackbarManagerSupplier(
            OneshotSupplier<SnackbarManager> snackbarManagerSupplier) {
        mSnackbarManagerSupplier = snackbarManagerSupplier;
    }

    private void configureSignOutSwitch() {
        Preference signOutPreference = findPreference(PREF_SIGN_OUT);
        if (getProfile().isChild()) {
            getPreferenceScreen().removePreference(signOutPreference);
            getPreferenceScreen().removePreference(findPreference(PREF_SIGN_OUT_DIVIDER));
        } else {
            signOutPreference.setLayoutResource(R.layout.account_management_account_row);
            signOutPreference.setIcon(R.drawable.ic_signout_40dp);
            signOutPreference.setTitle(
                    IdentityServicesProvider.get()
                                    .getIdentityManager(getProfile())
                                    .hasPrimaryAccount(ConsentLevel.SYNC)
                            ? R.string.sign_out_and_turn_off_sync
                            : R.string.sign_out);
            signOutPreference.setOnPreferenceClickListener(
                    preference -> {
                        if (!isVisible() || !isResumed() || mSignedInCoreAccountInfo == null) {
                            return false;
                        }

                        if (ChromeFeatureList.isEnabled(
                                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
                            SignOutCoordinator.startSignOutFlow(
                                    requireContext(),
                                    getProfile(),
                                    getActivity().getSupportFragmentManager(),
                                    ((ModalDialogManagerHolder) getActivity())
                                            .getModalDialogManager(),
                                    mSnackbarManagerSupplier.get(),
                                    SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                                    /* showConfirmDialog= */ false,
                                    CallbackUtils.emptyRunnable());
                            return true;
                        }

                        if (IdentityServicesProvider.get()
                                        .getIdentityManager(getProfile())
                                        .getPrimaryAccountInfo(ConsentLevel.SYNC)
                                != null) {
                            // Only show the sign-out dialog if the user has given sync consent.
                            SignOutCoordinator.startSignOutFlow(
                                    requireContext(),
                                    getProfile(),
                                    getActivity().getSupportFragmentManager(),
                                    ((ModalDialogManagerHolder) getActivity())
                                            .getModalDialogManager(),
                                    mSnackbarManagerSupplier.get(),
                                    SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                                    /* showConfirmDialog= */ false,
                                    CallbackUtils.emptyRunnable());
                        } else {
                            IdentityServicesProvider.get()
                                    .getSigninManager(getProfile())
                                    .signOut(
                                            SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                                            null,
                                            false);
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
                parentText =
                        getString(
                                R.string.account_management_header_two_parent_names,
                                firstParent,
                                secondParent);
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
        accountsCategory.addPreference(createDividerPreference(R.layout.horizontal_divider));

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
        accountPreference.setTitle(
                SyncSettingsUtils.getDisplayableFullNameOrEmailWithPreference(
                        profileData, getContext(), SyncSettingsUtils.TitlePreference.EMAIL));
        accountPreference.setIcon(profileData.getImage());

        accountPreference.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(
                        this,
                        () ->
                                SigninUtils.openSettingsForAccount(
                                        getActivity(), coreAccountInfo.getEmail())));

        return accountPreference;
    }

    private Preference createManageYourGoogleAccountPreference() {
        Preference manageYourGoogleAccountPreference = new Preference(getStyledContext());
        manageYourGoogleAccountPreference.setLayoutResource(
                R.layout.account_management_account_row);
        manageYourGoogleAccountPreference.setTitle(R.string.manage_your_google_account);
        Drawable googleServicesIcon =
                UiUtils.getTintedDrawable(
                        getContext(),
                        R.drawable.ic_google_services_48dp,
                        R.color.default_icon_color_tint_list);
        manageYourGoogleAccountPreference.setIcon(googleServicesIcon);
        manageYourGoogleAccountPreference.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(
                        this,
                        () -> {
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
        addAccountPreference.setOnPreferenceClickListener(
                preference -> {
                    if (!isVisible() || !isResumed()) return false;

                    AccountManagerFacade accountManagerFacade =
                            AccountManagerFacadeProvider.getInstance();
                    accountManagerFacade.createAddAccountIntent(
                            (@Nullable Intent intent) -> {
                                if (!isVisible() || !isResumed()) return;

                                if (intent != null) {
                                    startActivity(intent);
                                } else {
                                    // AccountManagerFacade couldn't create intent, use SigninUtils
                                    // to open settings instead.
                                    SigninUtils.openSettingsForAllAccounts(getActivity());
                                }

                                // Return to the last opened tab if triggered from the content area.
                                if (mGaiaServiceType != GAIAServiceType.GAIA_SERVICE_TYPE_NONE) {
                                    SettingsNavigationFactory.createSettingsNavigation()
                                            .finishCurrentSettings(this);
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
            AccountManagerFacadeProvider.getInstance()
                    .checkChildAccountStatus(
                            coreAccountInfo,
                            (isChild, childAccount) -> {
                                if (isChild) {
                                    mProfileDataCache.setBadge(
                                            childAccount.getEmail(),
                                            R.drawable.ic_account_child_20dp);
                                }
                            });
        }
    }

    // ProfileDataCache.Observer implementation:
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        AccountManagerFacadeProvider.getInstance()
                .getCoreAccountInfos()
                .then(this::updateAccountsList);
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

    // IdentityErrorCardPreference.Listener implementation.
    @Override
    public void onIdentityErrorCardButtonClicked(@SyncError int error) {
        assert mSignedInCoreAccountInfo != null;
        switch (error) {
            case SyncError.AUTH_ERROR:
                AccountManagerFacadeProvider.getInstance()
                        .updateCredentials(
                                CoreAccountInfo.getAndroidAccountFrom(mSignedInCoreAccountInfo),
                                getActivity(),
                                null);
                return;
            case SyncError.CLIENT_OUT_OF_DATE:
                // Opens the client in play store for update.
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setData(
                        Uri.parse(
                                "market://details?id="
                                        + ContextUtils.getApplicationContext().getPackageName()));
                startActivity(intent);
                return;
            case SyncError.PASSPHRASE_REQUIRED:
                displayPassphraseDialog();
                return;
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                SyncSettingsUtils.openTrustedVaultKeyRetrievalDialog(
                        this, mSignedInCoreAccountInfo, REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL);
                return;
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
            case SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                SyncSettingsUtils.openTrustedVaultRecoverabilityDegradedDialog(
                        this,
                        mSignedInCoreAccountInfo,
                        REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED);
                return;
            case SyncError.OTHER_ERRORS:
            case SyncError.SYNC_SETUP_INCOMPLETE:
                // Identity error card is not shown for unrecoverable errors nor for sync setup
                // incomplete error (the latter is shown as part of sync error in the manage sync
                // settings page).
                assert false; // NOTREACHED()
                // fall through
            case SyncError.NO_ERROR:
            default:
                return;
        }
    }

    /**
     * Called upon completion of an activity started by a previous call to startActivityForResult()
     * via SyncSettingsUtils.openTrustedVaultKeyRetrievalDialog() or
     * SyncSettingsUtils.openTrustedVaultRecoverabilityDegradedDialog().
     *
     * @param requestCode Request code of the requested intent.
     * @param resultCode Result code of the requested intent.
     * @param data The data returned by the intent.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        // Upon key retrieval completion, the keys in TrustedVaultClient could have changed. This is
        // done even if the user cancelled the flow (i.e. resultCode != RESULT_OK) because it's
        // harmless to issue a redundant notifyKeysChanged().
        if (requestCode == REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL) {
            TrustedVaultClient.get().notifyKeysChanged();
        }
        if (requestCode == REQUEST_CODE_TRUSTED_VAULT_RECOVERABILITY_DEGRADED) {
            TrustedVaultClient.get().notifyRecoverabilityChanged();
        }
    }

    private void displayPassphraseDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseDialogFragment.newInstance(this).show(ft, FRAGMENT_ENTER_PASSPHRASE);
    }

    /** Returns whether the passphrase successfully decrypted the pending keys. */
    private boolean handleDecryption(String passphrase) {
        if (passphrase.isEmpty() || !mSyncService.setDecryptionPassphrase(passphrase)) {
            return false;
        }
        // PassphraseDialogFragment doesn't handle closing itself, so do it here. This is not done
        // in updateSyncStateFromAndroidSyncSettings() because that happens onResume and possibly in
        // other cases where the dialog should stay open.
        closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
        // Update UI.
        update();
        return true;
    }

    /** Callback for PassphraseDialogFragment.Listener */
    @Override
    public boolean onPassphraseEntered(String passphrase) {
        if (!mSyncService.isEngineInitialized()
                || !mSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            // If the engine was shut down since the dialog was opened, or the passphrase isn't
            // required anymore, do nothing.
            return false;
        }
        return handleDecryption(passphrase);
    }

    /** Callback for PassphraseDialogFragment.Listener */
    @Override
    public void onPassphraseCanceled() {}

    /**
     * Open the account management UI.
     * @param serviceType A signin::GAIAServiceType that triggered the dialog.
     */
    public static void openAccountManagementScreen(
            Context context, @GAIAServiceType int serviceType) {
        Bundle arguments = new Bundle();
        arguments.putInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, serviceType);
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        settingsNavigation.startSettings(context, AccountManagementFragment.class, arguments);
    }

    private void closeDialogIfOpen(String tag) {
        FragmentManager manager = getFragmentManager();
        if (manager == null) {
            // Do nothing if the manager doesn't exist yet; see http://crbug.com/480544.
            return;
        }
        DialogFragment df = (DialogFragment) manager.findFragmentByTag(tag);
        if (df != null) {
            df.dismiss();
        }
    }
}
