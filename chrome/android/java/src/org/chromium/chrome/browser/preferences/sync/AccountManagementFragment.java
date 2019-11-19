// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.sync;

import android.accounts.Account;
import android.annotation.TargetApi;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Point;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.os.UserManager;
import android.support.v4.app.DialogFragment;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceCategory;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.support.v7.preference.PreferenceScreen;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeBasePreference;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SignOutDialogFragment;
import org.chromium.chrome.browser.signin.SignOutDialogFragment.SignOutDialogListener;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.signin.SigninUtils;
import org.chromium.chrome.browser.superviseduser.FilteringBehavior;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.metrics.SignoutReason;

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

    public static final String SIGN_OUT_DIALOG_TAG = "sign_out_dialog_tag";
    private static final String CLEAR_DATA_PROGRESS_DIALOG_TAG = "clear_data_progress";

    /**
     * The key for an integer value in arguments bundle to
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
    public static final String PREF_SIGN_OUT = "sign_out";
    public static final String PREF_SIGN_OUT_DIVIDER = "sign_out_divider";

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

        mProfile = Profile.getLastUsedProfile();

        SigninUtils.logEvent(ProfileAccountManagementMetrics.VIEW, mGaiaServiceType);

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
        IdentityServicesProvider.getSigninManager().addSignInStateObserver(this);
        mProfileDataCache.addObserver(this);
        mProfileDataCache.update(AccountManagerFacade.get().tryGetGoogleAccountNames());
        update();
    }

    @Override
    public void onPause() {
        super.onPause();
        IdentityServicesProvider.getSigninManager().removeSignInStateObserver(this);
        mProfileDataCache.removeObserver(this);
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

        updateAccountsList();
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private boolean canAddAccounts() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return true;

        UserManager userManager =
                (UserManager) getActivity().getSystemService(Context.USER_SERVICE);
        return !userManager.hasUserRestriction(UserManager.DISALLOW_MODIFY_ACCOUNTS);
    }

    private void configureSignOutSwitch() {
        Preference signOutSwitch = findPreference(PREF_SIGN_OUT);
        if (mProfile.isChild()) {
            getPreferenceScreen().removePreference(signOutSwitch);
            getPreferenceScreen().removePreference(findPreference(PREF_SIGN_OUT_DIVIDER));
        } else {
            signOutSwitch.setTitle(R.string.sign_out_and_turn_off_sync);
            signOutSwitch.setEnabled(getSignOutAllowedPreferenceValue());
            signOutSwitch.setOnPreferenceClickListener(preference -> {
                if (!isVisible() || !isResumed()) return false;

                if (mSignedInAccountName != null && getSignOutAllowedPreferenceValue()) {
                    SigninUtils.logEvent(
                            ProfileAccountManagementMetrics.TOGGLE_SIGNOUT, mGaiaServiceType);

                    SignOutDialogFragment signOutFragment = new SignOutDialogFragment();
                    Bundle args = new Bundle();
                    args.putInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, mGaiaServiceType);
                    signOutFragment.setArguments(args);

                    signOutFragment.setTargetFragment(AccountManagementFragment.this, 0);
                    signOutFragment.show(getFragmentManager(), SIGN_OUT_DIALOG_TAG);

                    return true;
                }

                return false;
            });
        }
    }

    private void configureChildAccountPreferences() {
        Preference parentAccounts = findPreference(PREF_PARENT_ACCOUNTS);
        Preference childContent = findPreference(PREF_CHILD_CONTENT);
        if (mProfile.isChild()) {
            PrefServiceBridge prefService = PrefServiceBridge.getInstance();

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
            prefScreen.removePreference(findPreference(PREF_CHILD_CONTENT_DIVIDER));
        }
    }

    private void updateAccountsList() {
        PreferenceCategory accountsCategory =
                (PreferenceCategory) findPreference(PREF_ACCOUNTS_CATEGORY);
        if (accountsCategory == null) return;

        accountsCategory.removeAll();

        List<Account> accounts = AccountManagerFacade.get().tryGetGoogleAccounts();
        for (int i = 0; i < accounts.size(); i++) {
            Account account = accounts.get(i);
            Preference pref = new Preference(getStyledContext());
            pref.setLayoutResource(R.layout.account_management_account_row);
            pref.setTitle(account.name);
            pref.setIcon(mProfileDataCache.getProfileDataOrDefault(account.name).getImage());

            pref.setOnPreferenceClickListener(
                    preference -> SigninUtils.openSettingsForAccount(getActivity(), account));

            accountsCategory.addPreference(pref);
        }

        if (!mProfile.isChild()) {
            accountsCategory.addPreference(createAddAccountPreference());
        }
    }

    private ChromeBasePreference createAddAccountPreference() {
        ChromeBasePreference addAccountPreference = new ChromeBasePreference(getStyledContext());
        addAccountPreference.setLayoutResource(R.layout.account_management_account_row);
        addAccountPreference.setIcon(
                AppCompatResources.getDrawable(getActivity(), R.drawable.ic_add_circle_40dp));
        addAccountPreference.setTitle(R.string.account_management_add_account_title);
        addAccountPreference.setOnPreferenceClickListener(preference -> {
            if (!isVisible() || !isResumed()) return false;

            SigninUtils.logEvent(ProfileAccountManagementMetrics.ADD_ACCOUNT, mGaiaServiceType);

            AccountManagerFacade.get().createAddAccountIntent((@Nullable Intent intent) -> {
                if (!isVisible() || !isResumed()) return;

                if (intent != null) {
                    startActivity(intent);
                    return;
                }

                // AccountManagerFacade couldn't create intent, use SigninUtils to open settings
                // instead.
                SigninUtils.openSettingsForAllAccounts(getActivity());
            });

            // Return to the last opened tab if triggered from the content area.
            if (mGaiaServiceType != GAIAServiceType.GAIA_SERVICE_TYPE_NONE) {
                if (isAdded()) getActivity().finish();
            }

            return true;
        });
        addAccountPreference.setManagedPreferenceDelegate(preference -> !canAddAccounts());
        return addAccountPreference;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
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
    public void onSignOutClicked(boolean forceWipeUserData) {
        // In case the user reached this fragment without being signed in, we guard the sign out so
        // we do not hit a native crash.
        if (!ChromeSigninController.get().isSignedIn()) return;

        final DialogFragment clearDataProgressDialog = new ClearDataProgressDialog();
        IdentityServicesProvider.getSigninManager().signOut(
                SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, new SigninManager.SignOutCallback() {
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
                }, forceWipeUserData);
        SigninUtils.logEvent(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT, mGaiaServiceType);
    }

    @Override
    public void onSignOutDialogDismissed(boolean signOutClicked) {
        if (!signOutClicked) {
            SigninUtils.logEvent(ProfileAccountManagementMetrics.SIGNOUT_CANCEL, mGaiaServiceType);
        }
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
    public static void openAccountManagementScreen(@GAIAServiceType int serviceType) {
        Bundle arguments = new Bundle();
        arguments.putInt(SHOW_GAIA_SERVICE_TYPE_EXTRA, serviceType);
        PreferencesLauncher.launchSettingsPage(
                ContextUtils.getApplicationContext(), AccountManagementFragment.class, arguments);
    }

    /**
     * @return Whether the sign out is not disabled due to a child/EDU account.
     */
    private static boolean getSignOutAllowedPreferenceValue() {
        return ContextUtils.getAppSharedPreferences().getBoolean(SIGN_OUT_ALLOWED, true);
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
