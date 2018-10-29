// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.annotation.SuppressLint;
import android.content.Context;

import com.google.android.gms.auth.AccountChangeEvent;
import com.google.android.gms.auth.GoogleAuthException;
import com.google.android.gms.auth.GoogleAuthUtil;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.invalidation.InvalidationServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninManager.SignInCallback;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.AndroidSyncSettings;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import javax.annotation.Nullable;

/**
 * A helper for tasks like re-signin.
 *
 * This should be merged into SigninManager when it is upstreamed.
 */
public class SigninHelper {

    private static final String TAG = "SigninHelper";

    private static final Object LOCK = new Object();

    private static final String ACCOUNTS_CHANGED_PREFS_KEY = "prefs_sync_accounts_changed";

    // Key to the shared pref that holds the new account's name if the currently signed
    // in account has been renamed.
    private static final String ACCOUNT_RENAMED_PREFS_KEY = "prefs_sync_account_renamed";

    // Key to the shared pref that holds the last read index of all the account changed
    // events of the current signed in account.
    private static final String ACCOUNT_RENAME_EVENT_INDEX_PREFS_KEY =
            "prefs_sync_account_rename_event_index";

    @SuppressLint("StaticFieldLeak")
    private static SigninHelper sInstance;

    /**
     * Retrieve more detailed information from account changed intents.
     */
    public interface AccountChangeEventChecker {
        List<String> getAccountChangeEvents(Context context, int index, String accountName);
    }

    /**
     * Uses GoogleAuthUtil.getAccountChangeEvents to detect if account
     * renaming has occurred.
     */
    public static final class SystemAccountChangeEventChecker
            implements SigninHelper.AccountChangeEventChecker {
        @Override
        public List<String> getAccountChangeEvents(
                Context context, int index, String accountName) {
            try {
                List<AccountChangeEvent> list = GoogleAuthUtil.getAccountChangeEvents(
                        context, index, accountName);
                List<String> result = new ArrayList<>(list.size());
                for (AccountChangeEvent e : list) {
                    if (e.getChangeType() == GoogleAuthUtil.CHANGE_TYPE_ACCOUNT_RENAMED_TO) {
                        result.add(e.getChangeData());
                    } else {
                        result.add(null);
                    }
                }
                return result;
            } catch (IOException e) {
                Log.w(TAG, "Failed to get change events", e);
            } catch (GoogleAuthException e) {
                Log.w(TAG, "Failed to get change events", e);
            }
            return new ArrayList<>(0);
        }
    }

    private final ChromeSigninController mChromeSigninController;

    @Nullable private final ProfileSyncService mProfileSyncService;

    private final SigninManager mSigninManager;

    private final AccountTrackerService mAccountTrackerService;

    private final OAuth2TokenService mOAuth2TokenService;

    public static SigninHelper get() {
        synchronized (LOCK) {
            if (sInstance == null) {
                sInstance = new SigninHelper();
            }
        }
        return sInstance;
    }

    private SigninHelper() {
        mProfileSyncService = ProfileSyncService.get();
        mSigninManager = SigninManager.get();
        mAccountTrackerService = AccountTrackerService.get();
        mOAuth2TokenService = OAuth2TokenService.getForProfile(Profile.getLastUsedProfile());
        mChromeSigninController = ChromeSigninController.get();
    }

    public void validateAccountSettings(boolean accountsChanged) {
        // Ensure System accounts have been seeded.
        mAccountTrackerService.checkAndSeedSystemAccounts();
        if (!accountsChanged) {
            mAccountTrackerService.validateSystemAccounts();
        }

        Account syncAccount = mChromeSigninController.getSignedInUser();
        if (syncAccount == null) {
            return;
        }

        String renamedAccount = getNewSignedInAccountName();
        if (accountsChanged && renamedAccount != null) {
            handleAccountRename(
                    ChromeSigninController.get().getSignedInAccountName(), renamedAccount);
            return;
        }

        // Always check for account deleted.
        if (!accountExists(syncAccount)) {
            // It is possible that Chrome got to this point without account
            // rename notification. Let us signout before doing a rename.
            AsyncTask<Void> task = new AsyncTask<Void>() {
                @Override
                protected Void doInBackground() {
                    updateAccountRenameData();
                    return null;
                }

                @Override
                protected void onPostExecute(Void result) {
                    String renamedAccount = getNewSignedInAccountName();
                    if (renamedAccount == null) {
                        mSigninManager.signOut(SignoutReason.ACCOUNT_REMOVED_FROM_DEVICE);
                    } else {
                        validateAccountSettings(true);
                    }
                }
            };
            task.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
            return;
        }

        if (accountsChanged) {
            // Account details have changed so inform the token service that credentials
            // should now be available.
            mOAuth2TokenService.validateAccounts(false);
        }

        if (mProfileSyncService != null && AndroidSyncSettings.isSyncEnabled()) {
            if (mProfileSyncService.isFirstSetupComplete()) {
                if (accountsChanged) {
                    // Nudge the syncer to ensure it does a full sync.
                    InvalidationServiceFactory.getForProfile(Profile.getLastUsedProfile())
                                        .requestSyncFromNativeChromeForAllTypes();
                }
            } else {
                // We should have set up sync but for some reason it's not enabled. Tell the sync
                // engine to start.
                mProfileSyncService.requestStart();
            }
        }
    }

    /**
     * Deal with account rename. The current approach is to sign out and then sign back in.
     * In the (near) future, we should just be clearing all the cached email address here
     * and have the UI re-fetch the emailing address based on the ID.
     */
    private void handleAccountRename(final String oldName, final String newName) {
        Log.i(TAG, "handleAccountRename from: " + oldName + " to " + newName);

        // TODO(acleung): I think most of the operations need to run on the main
        // thread. May be we should have a progress Dialog?

        // TODO(acleung): Deal with passphrase or just prompt user to re-enter it?
        // Perform a sign-out with a callback to sign-in again.
        mSigninManager.signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, () -> {
            // Clear the shared perf only after signOut is successful.
            // If Chrome dies, we can try it again on next run.
            // Otherwise, if re-sign-in fails, we'll just leave chrome
            // signed-out.
            clearNewSignedInAccountName();
            performResignin(newName);
        });
    }

    private void performResignin(String newName) {
        // This is the correct account now.
        final Account account = AccountManagerFacade.createAccountFromName(newName);

        mSigninManager.signIn(account, null, new SignInCallback() {
            @Override
            public void onSignInComplete() {
                if (mProfileSyncService != null) {
                    mProfileSyncService.setSetupInProgress(false);
                }
                validateAccountSettings(true);
            }

            @Override
            public void onSignInAborted() {}
        });
    }

    private static boolean accountExists(Account account) {
        Account[] accounts = AccountManagerFacade.get().tryGetGoogleAccounts();
        for (Account a : accounts) {
            if (a.equals(account)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Sets the ACCOUNTS_CHANGED_PREFS_KEY to true.
     */
    public static void markAccountsChangedPref() {
        // The process may go away as soon as we return from onReceive but Android makes sure
        // that in-flight disk writes from apply() complete before changing component states.
        ContextUtils.getAppSharedPreferences()
                .edit().putBoolean(ACCOUNTS_CHANGED_PREFS_KEY, true).apply();
    }

    /**
     * @return The new account name of the current user. Null if it wasn't renamed.
     */
    public static String getNewSignedInAccountName() {
        return (ContextUtils.getAppSharedPreferences()
                .getString(ACCOUNT_RENAMED_PREFS_KEY, null));
    }

    private static void clearNewSignedInAccountName() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putString(ACCOUNT_RENAMED_PREFS_KEY, null)
                .apply();
    }

    private static String getLastKnownAccountName() {
        // This is the last known name of the currently signed in user.
        // It can be:
        //  1. The signed in account name known to the ChromeSigninController.
        //  2. A pending newly choosen name that is differed from the one known to
        //     ChromeSigninController but is stored in ACCOUNT_RENAMED_PREFS_KEY.
        String name = ContextUtils.getAppSharedPreferences().getString(
                ACCOUNT_RENAMED_PREFS_KEY, null);

        // If there is no pending rename, take the name known to ChromeSigninController.
        return name == null ? ChromeSigninController.get().getSignedInAccountName() : name;
    }

    public static void updateAccountRenameData() {
        updateAccountRenameData(new SystemAccountChangeEventChecker());
    }

    @VisibleForTesting
    public static void updateAccountRenameData(AccountChangeEventChecker checker) {
        String curName = getLastKnownAccountName();

        // Skip the search if there is no signed in account.
        if (curName == null) return;

        String newName = curName;

        // This is the last read index of all the account change event.
        int eventIndex = ContextUtils.getAppSharedPreferences().getInt(
                ACCOUNT_RENAME_EVENT_INDEX_PREFS_KEY, 0);

        int newIndex = eventIndex;

        try {
        outerLoop:
            while (true) {
                List<String> nameChanges = checker.getAccountChangeEvents(
                        ContextUtils.getApplicationContext(), newIndex, newName);

                for (String name : nameChanges) {
                    if (name != null) {
                        // We have found a rename event of the current account.
                        // We need to check if that account is further renamed.
                        newName = name;
                        if (!accountExists(AccountManagerFacade.createAccountFromName(newName))) {
                            newIndex = 0; // Start from the beginning of the new account.
                            continue outerLoop;
                        }
                        break;
                    }
                }

                // If there is no rename event pending. Update the last read index to avoid
                // re-reading them in the future.
                newIndex = nameChanges.size();
                break;
            }
        } catch (Exception e) {
            Log.w(TAG, "Error while looking for rename events.", e);
        }

        if (!curName.equals(newName)) {
            ContextUtils.getAppSharedPreferences()
                    .edit().putString(ACCOUNT_RENAMED_PREFS_KEY, newName).apply();
        }

        if (newIndex != eventIndex) {
            ContextUtils.getAppSharedPreferences()
                    .edit().putInt(ACCOUNT_RENAME_EVENT_INDEX_PREFS_KEY, newIndex).apply();
        }
    }

    @VisibleForTesting
    public static void resetAccountRenameEventIndex() {
        ContextUtils.getAppSharedPreferences()
                .edit().putInt(ACCOUNT_RENAME_EVENT_INDEX_PREFS_KEY, 0).apply();
    }

    public static boolean checkAndClearAccountsChangedPref() {
        if (ContextUtils.getAppSharedPreferences()
                .getBoolean(ACCOUNTS_CHANGED_PREFS_KEY, false)) {
            // Clear the value in prefs.
            ContextUtils.getAppSharedPreferences()
                    .edit().putBoolean(ACCOUNTS_CHANGED_PREFS_KEY, false).apply();
            return true;
        } else {
            return false;
        }
    }
}
