// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.accounts.Account;
import android.app.backup.BackupDataInput;
import android.app.backup.BackupDataOutput;
import android.app.backup.BackupManager;
import android.content.SharedPreferences;
import android.os.ParcelFileDescriptor;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.base.SplitCompatApplication;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.init.AsyncInitTaskRunner;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.content_public.common.ContentProcessInfo;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Backup agent for Chrome, using Android key/value backup.
 */
@SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
public class ChromeBackupAgentImpl extends ChromeBackupAgent.Impl {
    private static final String ANDROID_DEFAULT_PREFIX = "AndroidDefault.";
    private static final String NATIVE_PREF_PREFIX = "native.";

    private static final String TAG = "ChromeBackupAgent";

    @VisibleForTesting
    static final String HISTOGRAM_ANDROID_RESTORE_RESULT = "Android.RestoreResult";

    // Restore status is used to pass the result of any restore to Chrome's first run, so that
    // it can be recorded as a histogram.
    @IntDef({RestoreStatus.NO_RESTORE, RestoreStatus.RESTORE_COMPLETED,
            RestoreStatus.RESTORE_AFTER_FIRST_RUN, RestoreStatus.BROWSER_STARTUP_FAILED,
            RestoreStatus.NOT_SIGNED_IN, RestoreStatus.RESTORE_STATUS_RECORDED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RestoreStatus {
        // Values must match those in histogram.xml AndroidRestoreResult.
        int NO_RESTORE = 0;
        int RESTORE_COMPLETED = 1;
        int RESTORE_AFTER_FIRST_RUN = 2;
        int BROWSER_STARTUP_FAILED = 3;
        int NOT_SIGNED_IN = 4;

        int NUM_ENTRIES = 5;

        // Set RESTORE_STATUS_RECORDED when the histogram has been recorded; so that it is only
        // recorded once.
        int RESTORE_STATUS_RECORDED = 5;
    }

    private static final String RESTORE_STATUS = "android_restore_status";

    // Keep track of backup failures, so that we give up in the end on persistent problems.
    @VisibleForTesting
    static final String BACKUP_FAILURE_COUNT = "android_backup_failure_count";
    @VisibleForTesting
    static final int MAX_BACKUP_FAILURES = 5;

    // List of preferences that should be restored unchanged.
    static final String[] BACKUP_ANDROID_BOOL_PREFS = {
            ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED,
            ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE,
            ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE,
            ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY,
            ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER,
    };

    // Key used to store the email of the signed in account. This email is obtained from
    // IdentityManager during the backup.
    static final String SIGNED_IN_ACCOUNT_KEY = "google.services.username";

    // Timeout for running the background tasks, needs to be quite long since they may be doing
    // network access, but must be less than the 1 minute restore timeout to be useful.
    private static final long BACKGROUND_TASK_TIMEOUT_SECS = 20;

    /**
     * Class to save and restore the backup state, used to decide if backups are needed. Since the
     * backup data is small, and stored as private data by the backup service, this can simply store
     * and compare a copy of the data.
     */
    private static final class BackupState {
        private ArrayList<String> mNames;
        private ArrayList<byte[]> mValues;

        @SuppressWarnings("unchecked")
        public BackupState(ParcelFileDescriptor parceledState) throws IOException {
            if (parceledState == null) return;
            try {
                FileInputStream instream = new FileInputStream(parceledState.getFileDescriptor());
                ObjectInputStream in = new ObjectInputStream(instream);
                mNames = (ArrayList<String>) in.readObject();
                mValues = (ArrayList<byte[]>) in.readObject();
            } catch (ClassNotFoundException e) {
                throw new RuntimeException(e);
            }
        }

        public BackupState(ArrayList<String> names, ArrayList<byte[]> values) {
            mNames = names;
            mValues = values;
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof BackupState)) return false;
            BackupState otherBackupState = (BackupState) other;
            return mNames.equals(otherBackupState.mNames)
                    && Arrays.deepEquals(mValues.toArray(), otherBackupState.mValues.toArray());
        }

        public void save(ParcelFileDescriptor parceledState) throws IOException {
            FileOutputStream outstream = new FileOutputStream(parceledState.getFileDescriptor());
            ObjectOutputStream out = new ObjectOutputStream(outstream);
            out.writeObject(mNames);
            out.writeObject(mValues);
        }
    }

    // TODO (aberent) Refactor the tests to use a mocked ChromeBrowserInitializer, and make this
    // private again.
    @VisibleForTesting
    boolean initializeBrowser() {
        // Workaround for https://crbug.com/718166. The backup agent is sometimes being started in a
        // child process, before the child process loads its native library. If backup then loads
        // the native library the child process is left in a very confused state and crashes.
        if (ContentProcessInfo.inChildProcess()) {
            Log.e(TAG, "Backup agent started from child process");
            return false;
        }
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        return true;
    }

    private static byte[] booleanToBytes(boolean value) {
        return value ? new byte[] {1} : new byte[] {0};
    }

    private static boolean bytesToBoolean(byte[] bytes) {
        return bytes[0] != 0;
    }

    @Override
    public void onBackup(ParcelFileDescriptor oldState, BackupDataOutput data,
            ParcelFileDescriptor newState) throws IOException {
        final ArrayList<String> backupNames = new ArrayList<>();
        final ArrayList<byte[]> backupValues = new ArrayList<>();
        final AtomicReference<CoreAccountInfo> syncAccount = new AtomicReference<>();

        // The native preferences can only be read on the UI thread.
        Boolean nativePrefsRead = PostTask.runSynchronously(TaskTraits.UI_DEFAULT, () -> {
            // Start the browser if necessary, so that Chrome can access the native
            // preferences. Although Chrome requests the backup, it doesn't happen
            // immediately, so by the time it does Chrome may not be running.
            if (!initializeBrowser()) return false;

            syncAccount.set(IdentityServicesProvider.get()
                                    .getIdentityManager(Profile.getLastUsedRegularProfile())
                                    .getPrimaryAccountInfo(ConsentLevel.SYNC));

            String[] nativeBackupNames = ChromeBackupAgentImplJni.get().getBoolBackupNames(this);
            boolean[] nativeBackupValues = ChromeBackupAgentImplJni.get().getBoolBackupValues(this);
            assert nativeBackupNames.length == nativeBackupValues.length;

            for (String name : nativeBackupNames) {
                backupNames.add(NATIVE_PREF_PREFIX + name);
            }
            for (boolean val : nativeBackupValues) {
                backupValues.add(booleanToBytes(val));
            }
            return true;
        });
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();

        if (!nativePrefsRead) {
            // Something went wrong reading the native preferences, skip the backup, but try again
            // later.
            int backupFailureCount = sharedPrefs.getInt(BACKUP_FAILURE_COUNT, 0) + 1;
            if (backupFailureCount >= MAX_BACKUP_FAILURES) {
                // Too many re-tries, give up and force an unconditional backup next time one is
                // requested.
                return;
            }
            sharedPrefs.edit().putInt(BACKUP_FAILURE_COUNT, backupFailureCount).apply();
            if (oldState != null) {
                try {
                    // Copy the old state to the new state, so that next time Chrome only does a
                    // backup if necessary.
                    BackupState state = new BackupState(oldState);
                    state.save(newState);
                } catch (Exception e) {
                    // There was no old state, or it was corrupt; leave the newState unwritten,
                    // hence forcing an unconditional backup on the next attempt.
                }
            }
            // Ask Android to schedule a retry.
            new BackupManager(getBackupAgent()).dataChanged();
            return;
        }

        // The backup is going to work, clear the failure count.
        sharedPrefs.edit().remove(BACKUP_FAILURE_COUNT).apply();

        // Add the Android boolean prefs.
        for (String prefName : BACKUP_ANDROID_BOOL_PREFS) {
            if (sharedPrefs.contains(prefName)) {
                backupNames.add(ANDROID_DEFAULT_PREFIX + prefName);
                backupValues.add(booleanToBytes(sharedPrefs.getBoolean(prefName, false)));
            }
        }

        // Finally add the user id.
        backupNames.add(ANDROID_DEFAULT_PREFIX + SIGNED_IN_ACCOUNT_KEY);
        backupValues.add(ApiCompatibilityUtils.getBytesUtf8(
                syncAccount.get() == null ? "" : syncAccount.get().getEmail()));

        BackupState newBackupState = new BackupState(backupNames, backupValues);

        // Check if a backup is actually needed.
        try {
            BackupState oldBackupState = new BackupState(oldState);
            if (newBackupState.equals(oldBackupState)) {
                Log.i(TAG, "Nothing has changed since the last backup. Backup skipped.");
                newBackupState.save(newState);
                return;
            }
        } catch (IOException e) {
            // This will happen if Chrome has never written backup data, or if the backup status is
            // corrupt. Create a new backup in either case.
            Log.i(TAG, "Can't read backup status file");
        }

        // Write the backup data
        for (int i = 0; i < backupNames.size(); i++) {
            data.writeEntityHeader(backupNames.get(i), backupValues.get(i).length);
            data.writeEntityData(backupValues.get(i), backupValues.get(i).length);
        }

        // Remember the backup state.
        newBackupState.save(newState);

        Log.i(TAG, "Backup complete");
    }

    @Override
    public void onRestore(BackupDataInput data, int appVersionCode, ParcelFileDescriptor newState)
            throws IOException {
        // TODO(aberent) Check that this is not running on the UI thread. Doing so, however, makes
        // testing difficult since the test code runs on the UI thread.

        // Check that the user hasn't already seen FRE (not sure if this can ever happen, but if it
        // does then restoring the backup will overwrite the user's choices).
        SharedPreferences sharedPrefs = ContextUtils.getAppSharedPreferences();
        if (FirstRunStatus.getFirstRunFlowComplete()
                || FirstRunStatus.getLightweightFirstRunFlowComplete()) {
            setRestoreStatus(RestoreStatus.RESTORE_AFTER_FIRST_RUN);
            Log.w(TAG, "Restore attempted after first run");
            return;
        }

        final ArrayList<String> backupNames = new ArrayList<>();
        final ArrayList<byte[]> backupValues = new ArrayList<>();

        String restoredUserName = null;
        while (data.readNextHeader()) {
            String key = data.getKey();
            int dataSize = data.getDataSize();
            byte[] buffer = new byte[dataSize];
            data.readEntityData(buffer, 0, dataSize);
            if (key.equals(ANDROID_DEFAULT_PREFIX + SIGNED_IN_ACCOUNT_KEY)) {
                restoredUserName = new String(buffer);
            } else {
                backupNames.add(key);
                backupValues.add(buffer);
            }
        }

        // Start and wait for the Async init tasks. This loads the library, and attempts to load the
        // first run variations seed. Since these are both slow it makes sense to run them in
        // parallel as Android AsyncTasks, reusing some of Chrome's async startup logic.
        //
        // Note that this depends on onRestore being run from a background thread, since
        // if it were called from the UI thread the broadcast would not be received until after it
        // exited.
        final CountDownLatch latch = new CountDownLatch(1);
        PostTask.runSynchronously(TaskTraits.UI_DEFAULT, () -> {
            // Chrome library loading depends on PathUtils.
            PathUtils.setPrivateDataDirectorySuffix(
                    SplitCompatApplication.PRIVATE_DATA_DIRECTORY_SUFFIX);
            createAsyncInitTaskRunner(latch).startBackgroundTasks(
                    false /* allocateChildConnection */, true /* initVariationSeed */);
        });

        try {
            // Ignore result. It will only be false if it times out. Problems with fetching the
            // variation seed can be ignored, and other problems will either recover or be repeated
            // when Chrome is started synchronously.
            latch.await(BACKGROUND_TASK_TIMEOUT_SECS, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            // Should never happen, but can be ignored (as explained above) anyway.
        }

        // Chrome has to be running before it can check if the account exists. Because the native
        // library is already loaded Chrome startup should be fast.
        boolean browserStarted = PostTask.runSynchronously(TaskTraits.UI_DEFAULT, () -> {
            // Start the browser if necessary.
            return initializeBrowser();
        });
        if (!browserStarted) {
            // Something went wrong starting Chrome, skip the restore.
            setRestoreStatus(RestoreStatus.BROWSER_STARTUP_FAILED);
            return;
        }

        // If the user hasn't signed in, or can't sign in, then don't restore anything.
        if (!accountExistsOnDevice(restoredUserName)) {
            setRestoreStatus(RestoreStatus.NOT_SIGNED_IN);
            Log.i(TAG, "Chrome was not signed in with a known account name, not restoring");
            return;
        }

        // Restore the native preferences on the UI thread
        PostTask.runSynchronously(TaskTraits.UI_DEFAULT, () -> {
            ArrayList<String> nativeBackupNames = new ArrayList<>();
            boolean[] nativeBackupValues = new boolean[backupNames.size()];
            int count = 0;
            int prefixLength = NATIVE_PREF_PREFIX.length();
            for (int i = 0; i < backupNames.size(); i++) {
                String name = backupNames.get(i);
                if (name.startsWith(NATIVE_PREF_PREFIX)) {
                    nativeBackupNames.add(name.substring(prefixLength));
                    nativeBackupValues[count] = bytesToBoolean(backupValues.get(i));
                    count++;
                }
            }
            ChromeBackupAgentImplJni.get().setBoolBackupPrefs(this,
                    nativeBackupNames.toArray(new String[count]),
                    Arrays.copyOf(nativeBackupValues, count));
        });

        // Now that everything looks good so restore the Android preferences.
        SharedPreferences.Editor editor = sharedPrefs.edit();

        // Only restore preferences that we know about.
        int prefixLength = ANDROID_DEFAULT_PREFIX.length();
        for (int i = 0; i < backupNames.size(); i++) {
            String name = backupNames.get(i);
            if (name.startsWith(ANDROID_DEFAULT_PREFIX)
                    && Arrays.asList(BACKUP_ANDROID_BOOL_PREFS)
                               .contains(name.substring(prefixLength))) {
                editor.putBoolean(
                        name.substring(prefixLength), bytesToBoolean(backupValues.get(i)));
            }
        }

        // This will sign in the user on first run to the account in BACKUP_FLOW_SIGNIN_ACCOUNT_NAME
        // if any.
        editor.putString(ChromePreferenceKeys.BACKUP_FLOW_SIGNIN_ACCOUNT_NAME, restoredUserName);
        editor.apply();

        // The silent first run will change things, so there is no point in trying to prevent
        // additional backups at this stage. Don't write anything to |newState|.
        setRestoreStatus(RestoreStatus.RESTORE_COMPLETED);
        Log.i(TAG, "Restore complete");
    }

    @VisibleForTesting
    AsyncInitTaskRunner createAsyncInitTaskRunner(final CountDownLatch latch) {
        return new AsyncInitTaskRunner() {
            @Override
            protected void onSuccess() {
                latch.countDown();
            }

            @Override
            protected void onFailure(Exception failureCause) {
                // Ignore failure. Problems with the variation seed can be ignored, and other
                // problems will either recover or be repeated when Chrome is started synchronously.
                latch.countDown();
            }
        };
    }

    private boolean accountExistsOnDevice(String accountName) {
        return PostTask.runSynchronously(TaskTraits.UI_DEFAULT, () -> {
            List<Account> accounts = AccountUtils.getAccountsIfFulfilledOrEmpty(
                    AccountManagerFacadeProvider.getInstance().getAccounts());
            return accountName != null
                    && AccountUtils.findAccountByName(accounts, accountName) != null;
        });
    }

    /**
     * Get the saved result of any restore that may have happened.
     *
     * @return the restore status, a RestoreStatus value.
     */
    @VisibleForTesting
    @RestoreStatus
    static int getRestoreStatus() {
        return ContextUtils.getAppSharedPreferences().getInt(
                RESTORE_STATUS, RestoreStatus.NO_RESTORE);
    }

    /**
     * Save the restore status for later transfer to a histogram.
     *
     * @param status the status.
     */
    @VisibleForTesting
    static void setRestoreStatus(@RestoreStatus int status) {
        ContextUtils.getAppSharedPreferences().edit().putInt(RESTORE_STATUS, status).apply();
    }

    /**
     * Record the restore histogram. To be called from Chrome itself once it is running.
     */
    public static void recordRestoreHistogram() {
        @RestoreStatus
        int restoreStatus = getRestoreStatus();
        // Ensure restore status is only recorded once
        if (restoreStatus != RestoreStatus.RESTORE_STATUS_RECORDED) {
            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_ANDROID_RESTORE_RESULT, restoreStatus, RestoreStatus.NUM_ENTRIES);
            setRestoreStatus(RestoreStatus.RESTORE_STATUS_RECORDED);
        }
    }

    @NativeMethods
    interface Natives {
        String[] getBoolBackupNames(ChromeBackupAgentImpl caller);
        boolean[] getBoolBackupValues(ChromeBackupAgentImpl caller);
        void setBoolBackupPrefs(ChromeBackupAgentImpl caller, String[] name, boolean[] value);
    }
}
