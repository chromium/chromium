// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.Dialog;
import android.app.DialogFragment;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Point;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.os.UserManager;
import android.preference.Preference;
import android.preference.PreferenceCategory;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.support.annotation.Nullable;
import android.support.v4.app.FragmentActivity;
import android.widget.ListView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeBasePreference;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.SyncPreference;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.chrome.browser.signin.SignOutDialogFragment.SignOutDialogListener;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.ProfileSyncService.SyncStateChangedListener;
import org.chromium.chrome.browser.sync.ui.SyncCustomizationFragment;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;

/**
 * The settings screen with information and settings related to the user's accounts.
 *
 * This shows which accounts the user is signed in with, allows the user to sign out of Chrome,
 * links to sync settings, has links to add accounts and go incognito, and shows parental settings
 * if a child account is in use.
 *
 * Note: This can be triggered from a web page, e.g. a GAIA sign-in page.
 */
public class AccountManagementFragment extends PreferenceFragment
        implements SignOutDialogListener, SyncStateChangedListener, SignInStateObserver,
                   ConfirmManagedSyncDataDialog.Listener, ProfileDataCache.Observer {
    private static final String TAG = "AcctManagementPref";

    public static final String SIGN_OUT_DIALOG_TAG = "sign_out_dialog_tag";
    private static final String CLEAR_DATA_PROGRESS_DIALOG_TAG = "clear_data_progress";

    /**
     * The key for an integer value in
     * {@link Preferences#EXTRA_SHOW_FRAGMENT_ARGUMENTS} bundle to
     * specify the correct GAIA service that has triggered the dialog.
     * If the argument is not set, GAIA_SERVICE_TYPE_NONE is used as the origin of the dialog.
     */
    public static final String SHOW_GAIA_SERVICE_TYPE_EXTRA = "ShowGAIAServiceType";

    /**
     * SharedPreference name for the preference that disables signing out of Chrome.
     * Signing out is forever disabled once Chrome signs the user in automatically
     * if the device has a child account or if the device is an Android EDU device.
     */
    private static final String SIGN_OUT_ALLOWED = "auto_signed_in_school_account";

    public static final String PREF_ACCOUNTS_CATEGORY = "accounts_category";
    public static final String PREF_PARENTAL_SETTINGS = "parental_settings";
    public static final String PREF_PARENT_ACCOUNTS = "parent_accounts";
    public static final String PREF_CHILD_CONTENT = "child_content";
    public static final String PREF_CHILD_CONTENT_DIVIDER = "child_content_divider";
    public static final String PREF_GOOGLE_ACTIVITY_CONTROLS = "google_activity_controls";
    public static final String PREF_GOOGLE_ACTIVITY_CONTROLS_DIVIDER =
            "google_activity_controls_divider";
    public static final String PREF_SYNC_SETTINGS = "sync_settings";
    public static final String PREF_SYNC_SETTINGS_DIVIDER = "sync_settings_divider";
    public static final String PREF_SIGN_OUT = "sign_out";
    public static final String PREF_SIGN_OUT_DIVIDER = "sign_out_divider";

    private int mGaiaServiceType;

    private Profile mProfile;
    private String mSignedInAccountName;
    private ProfileDataCache mProfileDataCache;

    @Override
    public void onCreate(Bundle savedState) {
        super.onCreate(savedState);

        // Prevent sync from starting if it hasn't already to give the user a chance to change
        // their sync settings.
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.setSetupInProgress(true);
        }

        mGaiaServiceType = AccountManagementScreenHelper.GAIA_SERVICE_TYPE_NONE;
        if (getArguments() != null) {
            mGaiaServiceType =
                    getArguments().getInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, mGaiaServiceType);
        }

        mProfile = Profile.getLastUsedProfile();

        AccountManagementScreenHelper.logEvent(
                ProfileAccountManagementMetrics.VIEW,
                mGaiaServiceType);

        int avatarImageSize = getResources().getDimensionPixelSize(R.dimen.user_picture_size);
        ProfileDataCache.BadgeConfig badgeConfig = null;
        if (mProfile.isChild()) {
            Bitmap badge =
                    BitmapFactory.decodeResource(getResources(), R.drawable.ic_account_child_20dp);
            int badgePositionX = getResources().getDimensionPixelOffset(R.dimen.badge_position_x);
            int badgePositionY = getResources().getDimensionPixelOffset(R.dimen.badge_position_y);
            int badgeBorderSize = getResources().getDimensionPixelSize(R.dimen.badge_border_size);
            badgeConfig = new ProfileDataCache.BadgeConfig(
                    badge, new Point(badgePositionX, badgePositionY), badgeBorderSize);
        }
        mProfileDataCache = new ProfileDataCache(getActivity(), avatarImageSize, badgeConfig);
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        ListView list = getView().findViewById(android.R.id.list);
        list.setDivider(null);
    }

    @Override
    public void onResume() {
        super.onResume();
        SigninManager.get().addSignInStateObserver(this);
        mProfileDataCache.addObserver(this);
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.addSyncStateChangedListener(this);
        }

        mProfileDataCache.update(AccountManagerFacade.get().tryGetGoogleAccountNames());
        update();
    }

    @Override
    public void onPause() {
        super.onPause();
        SigninManager.get().removeSignInStateObserver(this);
        mProfileDataCache.removeObserver(this);
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.removeSyncStateChangedListener(this);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        // Allow sync to begin syncing if it hasn't yet.
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.setSetupInProgress(false);
        }
    }

    public void update() {
        final Context context = getActivity();
        if (context == null) return;

        if (getPreferenceScreen() != null) getPreferenceScreen().removeAll();

        mSignedInAccountName = ChromeSigninController.get().getSignedInAccountName();
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
        configureSyncSettings();
        configureGoogleActivityControls();

        updateAccountsList();
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private boolean canAddAccounts() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return true;

        UserManager userManager = (UserManager) getActivity()
                .getSystemService(Context.USER_SERVICE);
        return !userManager.hasUserRestriction(UserManager.DISALLOW_MODIFY_ACCOUNTS);
    }

    private void configureSignOutSwitch() {
        Preference signOutSwitch = findPreference(PREF_SIGN_OUT);
        if (mProfile.isChild()) {
            getPreferenceScreen().removePreference(signOutSwitch);
            getPreferenceScreen().removePreference(findPreference(PREF_SIGN_OUT_DIVIDER));
        } else {
            signOutSwitch.setTitle(ChromeFeatureList.isEnabled(ChromeFeatureList.UNIFIED_CONSENT)
                            ? R.string.sign_out_and_turn_off_sync
                            : R.string.account_management_sign_out);

            signOutSwitch.setEnabled(getSignOutAllowedPreferenceValue());
            signOutSwitch.setOnPreferenceClickListener(preference -> {
                if (!isVisible() || !isResumed()) return false;

                if (mSignedInAccountName != null && getSignOutAllowedPreferenceValue()) {
                    AccountManagementScreenHelper.logEvent(
                            ProfileAccountManagementMetrics.TOGGLE_SIGNOUT, mGaiaServiceType);

                    String managementDomain = SigninManager.get().getManagementDomain();
                    if (managementDomain != null) {
                        // Show the 'You are signing out of a managed account' dialog.

                        // TODO(https://crbug.com/710657): Migrate to AccountManagementFragment to
                        // extend android.support.v7.preference.Preference and remove this cast.
                        FragmentActivity fragmentActivity = (FragmentActivity) getActivity();
                        ConfirmManagedSyncDataDialog.showSignOutFromManagedAccountDialog(
                                AccountManagementFragment.this,
                                fragmentActivity.getSupportFragmentManager(), getResources(),
                                managementDomain);
                    } else {
                        // Show the 'You are signing out' dialog.
                        SignOutDialogFragment signOutFragment = new SignOutDialogFragment();
                        Bundle args = new Bundle();
                        args.putInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, mGaiaServiceType);
                        signOutFragment.setArguments(args);

                        signOutFragment.setTargetFragment(AccountManagementFragment.this, 0);
                        signOutFragment.show(getFragmentManager(), SIGN_OUT_DIALOG_TAG);
                    }

                    return true;
                }

                return false;
            });
        }
    }

    private void configureSyncSettings() {
        Preference syncSettings = findPreference(PREF_SYNC_SETTINGS);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UNIFIED_CONSENT)) {
            getPreferenceScreen().removePreference(syncSettings);
            getPreferenceScreen().removePreference(findPreference(PREF_SYNC_SETTINGS_DIVIDER));
            return;
        }
        final Preferences preferences = (Preferences) getActivity();
        syncSettings.setOnPreferenceClickListener(preference -> {
            if (!isVisible() || !isResumed()) return false;

            if (ProfileSyncService.get() == null) return true;

            preferences.startFragment(SyncCustomizationFragment.class.getName(), new Bundle());
            return true;
        });
    }

    private void configureGoogleActivityControls() {
        Preference pref = findPreference(PREF_GOOGLE_ACTIVITY_CONTROLS);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UNIFIED_CONSENT)) {
            getPreferenceScreen().removePreference(pref);
            getPreferenceScreen().removePreference(
                    findPreference(PREF_GOOGLE_ACTIVITY_CONTROLS_DIVIDER));
            return;
        }
        if (mProfile.isChild()) {
            pref.setSummary(R.string.sign_in_google_activity_controls_message_child_account);
        }
        pref.setOnPreferenceClickListener(preference -> {
            Activity activity = getActivity();
            AppHooks.get().createGoogleActivityController().openWebAndAppActivitySettings(
                    activity, mSignedInAccountName);
            RecordUserAction.record("Signin_AccountSettings_GoogleActivityControlsClicked");
            return true;
        });
    }

    private void configureChildAccountPreferences() {
        Preference parentAccounts = findPreference(PREF_PARENT_ACCOUNTS);
        Preference childContent = findPreference(PREF_CHILD_CONTENT);
        if (mProfile.isChild()) {
            Resources res = getActivity().getResources();
            PrefServiceBridge prefService = PrefServiceBridge.getInstance();

            String firstParent = prefService.getSupervisedUserCustodianEmail();
            String secondParent = prefService.getSupervisedUserSecondCustodianEmail();
            String parentText;

            if (!secondParent.isEmpty()) {
                parentText = res.getString(R.string.account_management_two_parent_names,
                        firstParent, secondParent);
            } else if (!firstParent.isEmpty()) {
                parentText = res.getString(R.string.account_management_one_parent_name,
                        firstParent);
            } else {
                parentText = res.getString(R.string.account_management_no_parental_data);
            }
            parentAccounts.setSummary(parentText);

            final int childContentSummary;
            int defaultBehavior = prefService.getDefaultSupervisedUserFilteringBehavior();
            if (defaultBehavior == PrefServiceBridge.SUPERVISED_USER_FILTERING_BLOCK) {
                childContentSummary = R.string.account_management_child_content_approved;
            } else if (prefService.isSupervisedUserSafeSitesEnabled()) {
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
            prefScreen.removePreference(findPreference(PREF_CHILD_CONTENT_DIVIDER));
        }
    }

    private void updateAccountsList() {
        PreferenceCategory accountsCategory =
                (PreferenceCategory) findPreference(PREF_ACCOUNTS_CATEGORY);
        if (accountsCategory == null) return;

        accountsCategory.removeAll();

        Account[] accounts = AccountManagerFacade.get().tryGetGoogleAccounts();
        for (final Account account : accounts) {
            Preference pref = new Preference(getActivity());
            pref.setLayoutResource(R.layout.account_management_account_row);
            pref.setTitle(account.name);
            pref.setIcon(mProfileDataCache.getProfileDataOrDefault(account.name).getImage());

            pref.setOnPreferenceClickListener(
                    preference -> SigninUtils.openAccountSettingsPage(getActivity(), account));

            accountsCategory.addPreference(pref);
        }

        if (!mProfile.isChild()) {
            accountsCategory.addPreference(createAddAccountPreference());
        }
    }

    private ChromeBasePreference createAddAccountPreference() {
        ChromeBasePreference addAccountPreference = new ChromeBasePreference(getActivity());
        addAccountPreference.setLayoutResource(R.layout.account_management_account_row);
        addAccountPreference.setIcon(R.drawable.add_circle_blue);
        addAccountPreference.setTitle(R.string.account_management_add_account_title);
        addAccountPreference.setOnPreferenceClickListener(preference -> {
            if (!isVisible() || !isResumed()) return false;

            AccountManagementScreenHelper.logEvent(
                    ProfileAccountManagementMetrics.ADD_ACCOUNT, mGaiaServiceType);

            AccountAdder.getInstance().addAccount(getActivity(), AccountAdder.ADD_ACCOUNT_RESULT);

            // Return to the last opened tab if triggered from the content area.
            if (mGaiaServiceType != AccountManagementScreenHelper.GAIA_SERVICE_TYPE_NONE) {
                if (isAdded()) getActivity().finish();
            }

            return true;
        });
        addAccountPreference.setManagedPreferenceDelegate(preference -> !canAddAccounts());
        return addAccountPreference;
    }

    // ProfileDataCache.Observer implementation:
    @Override
    public void onProfileDataUpdated(String accountId) {
        updateAccountsList();
    }

    // SignOutDialogListener implementation:

    /**
     * This class must be public and static. Otherwise an exception will be thrown when Android
     * recreates the fragment (e.g. after a configuration change).
     */
    public static class ClearDataProgressDialog extends DialogFragment {
        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            // Don't allow the dialog to be recreated by Android, since it wouldn't ever be
            // dismissed after recreation.
            if (savedInstanceState != null) dismiss();
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            setCancelable(false);
            ProgressDialog dialog = new ProgressDialog(getActivity());
            dialog.setTitle(getString(R.string.wiping_profile_data_title));
            dialog.setMessage(getString(R.string.wiping_profile_data_message));
            dialog.setIndeterminate(true);
            return dialog;
        }
    }

    @Override
    public void onSignOutClicked() {
        // In case the user reached this fragment without being signed in, we guard the sign out so
        // we do not hit a native crash.
        if (!ChromeSigninController.get().isSignedIn()) return;

        final Activity activity = getActivity();
        final DialogFragment clearDataProgressDialog = new ClearDataProgressDialog();
        SigninManager.get().signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, null,
                new SigninManager.WipeDataHooks() {
                    @Override
                    public void preWipeData() {
                        clearDataProgressDialog.show(
                                activity.getFragmentManager(), CLEAR_DATA_PROGRESS_DIALOG_TAG);
                    }
                    @Override
                    public void postWipeData() {
                        if (clearDataProgressDialog.isAdded()) {
                            clearDataProgressDialog.dismissAllowingStateLoss();
                        }
                    }
                });
        AccountManagementScreenHelper.logEvent(
                ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                mGaiaServiceType);
    }

    @Override
    public void onSignOutDialogDismissed(boolean signOutClicked) {
        if (!signOutClicked) {
            AccountManagementScreenHelper.logEvent(
                    ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                    mGaiaServiceType);
        }
    }

    // ConfirmManagedSyncDataDialog.Listener implementation
    @Override
    public void onConfirm() {
        onSignOutClicked();
    }

    @Override
    public void onCancel() {
        onSignOutDialogDismissed(false);
    }

    // ProfileSyncServiceListener implementation:

    @Override
    public void syncStateChanged() {
        SyncPreference pref = (SyncPreference) findPreference(PREF_SYNC_SETTINGS);
        if (pref != null) {
            pref.updateSyncSummaryAndIcon();
        }

        // TODO(crbug/557784): Show notification for sync error
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
    public static void openAccountManagementScreen(int serviceType) {
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                ContextUtils.getApplicationContext(), AccountManagementFragment.class.getName());
        Bundle arguments = new Bundle();
        arguments.putInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, serviceType);
        intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, arguments);
        ContextUtils.getApplicationContext().startActivity(intent);
    }

    /**
     * @return Whether the sign out is not disabled due to a child/EDU account.
     */
    private static boolean getSignOutAllowedPreferenceValue() {
        return ContextUtils.getAppSharedPreferences()
                .getBoolean(SIGN_OUT_ALLOWED, true);
    }

    /**
     * Sets the sign out allowed preference value.
     *
     * @param isAllowed True if the sign out is not disabled due to a child/EDU account
     */
    public static void setSignOutAllowedPreferenceValue(boolean isAllowed) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(SIGN_OUT_ALLOWED, isAllowed)
                .apply();
    }
}
