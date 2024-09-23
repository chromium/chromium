// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.backup;

import android.app.backup.BackupDataInput;
import android.app.backup.BackupDataOutput;
import android.app.backup.BackupManager;
import android.content.SharedPreferences;
import android.os.ParcelFileDescriptor;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.base.SplitCompatApplication;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.AsyncInitTaskRunner;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.common.ContentProcessInfo;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.OutputStream;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Predicate;

/** Backup agent for Chrome, using Android key/value backup. */
@SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
public class ChromeBackupAgentImpl extends ChromeBackupAgent.Impl {
    private static final String ANDROID_DEFAULT_PREFIX = "AndroidDefault.";

    private static final String TAG = "ChromeBackupAgent";

    @VisibleForTesting
    static final String HISTOGRAM_ANDROID_RESTORE_RESULT = "Android.RestoreResult";

    // Restore status is used to pass the result of any restore to Chrome's first run, so that
    // it can be recorded as a histogram.
    @IntDef({
        RestoreStatus.NO_RESTORE,
        RestoreStatus.RESTORE_COMPLETED,
        RestoreStatus.RESTORE_AFTER_FIRST_RUN,
        RestoreStatus.BROWSER_STARTUP_FAILED,
        RestoreStatus.NOT_SIGNED_IN,
        RestoreStatus.DEPRECATED_SIGNIN_TIMED_OUT,
        RestoreStatus.DEPRECATED_RESTORE_STATUS_RECORDED,
        RestoreStatus.SIGNIN_TIMED_OUT,
        RestoreStatus.RESTORE_STARTED_NOT_FINISHED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface RestoreStatus {
        // Values must match those in histogram.xml AndroidRestoreResult.
        int NO_RESTORE = 0;
        int RESTORE_COMPLETED = 1;
        int RESTORE_AFTER_FIRST_RUN = 2;
        int BROWSER_STARTUP_FAILED = 3;
        int NOT_SIGNED_IN = 4;
        // This enum value has taken the previous value indicating that the histogram has been
        // recorded, when it was introduced. Deprecating since the metric is polluted consequently.
        int DEPRECATED_SIGNIN_TIMED_OUT = 5;
        // Previously, DEPRECATED_RESTORE_STATUS_RECORDED was set when the histogram has been
        // recorded, to prevent additional histogram record. This magic value is being replaced by
        // the boolean pref RESTORE_STATUS_RECORDED.
        // This value is kept for legacy pref support.
        int DEPRECATED_RESTORE_STATUS_RECORDED = 6;
        int SIGNIN_TIMED_OUT = 7;

        // Recorded if `onRestore` was called but the restore flow died or timed out before it could
        // record a more specific result.
        int RESTORE_STARTED_NOT_FINISHED = 8;

        int NUM_ENTRIES = RESTORE_STARTED_NOT_FINISHED;
    }

    @VisibleForTesting static final String RESTORE_STATUS = "android_restore_status";
    private static final String RESTORE_STATUS_RECORDED = "android_restore_status_recorded";

    // Keep track of backup failures, so that we give up in the end on persistent problems.
    @VisibleForTesting static final String BACKUP_FAILURE_COUNT = "android_backup_failure_count";
    @VisibleForTesting static final int MAX_BACKUP_FAILURES = 5;

    // Bool entries from SharedPreferences that should be backed up / restored.
    static final String[] BACKUP_ANDROID_BOOL_PREFS = {
        ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED,
        ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE,
        ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE,
        ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY,
        ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER,
    };

    // The supported PrefBackupSerializers, each responsible for allowlisting certain prefs for
    // backup & restore.
    static final List<PrefBackupSerializer> NATIVE_PREFS_SERIALIZERS =
            List.of(
                    new BoolPrefBackupSerializer(),
                    new DictPrefBackupSerializer(),
                    new IntPrefBackupSerializer());

    // Key used to store the email of the syncing account. This email is obtained from
    // IdentityManager during the backup.
    static final String SYNCING_ACCOUNT_KEY = "google.services.username";

    // Key used to store the email of the signed-in account. This email is obtained from
    // IdentityManager during the backup.
    static final String SIGNED_IN_ACCOUNT_ID_KEY = "Chrome.SignIn.SignedInAccountGaiaIdBackup";

    // Timeout for running the background tasks, needs to be quite long since they may be doing
    // network access, but must be less than the 1 minute restore timeout to be useful.
    private static final long BACKGROUND_TASK_TIMEOUT_SECS = 20;

    // Timeout for the sign-in flow and related preferences commit.
    private static final long SIGNIN_TIMEOUT_SECS = 10;

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
    public void onBackup(
            ParcelFileDescriptor oldState, BackupDataOutput data, ParcelFileDescriptor newState)
            throws IOException {
        final ArrayList<String> backupNames = new ArrayList<>();
        final ArrayList<byte[]> backupValues = new ArrayList<>();

        // TODO(crbug.com/40066949): Remove syncAccount once UNO is launched, given the sync feature
        // and consent will disappear.
        final AtomicReference<CoreAccountInfo> syncAccount = new AtomicReference<>();
        final AtomicReference<CoreAccountInfo> signedInAccount = new AtomicReference<>();

        // The native preferences can only be read on the UI thread.
        Boolean nativePrefsRead =
                PostTask.runSynchronously(
                        TaskTraits.UI_DEFAULT,
                        () -> {
                            // Start the browser if necessary, so that Chrome can access the native
                            // preferences. Although Chrome requests the backup, it doesn't happen
                            // immediately, so by the time it does Chrome may not be running.
                            if (!initializeBrowser()) return false;

                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            IdentityManager identityManager =
                                    IdentityServicesProvider.get().getIdentityManager(profile);
                            syncAccount.set(
                                    identityManager.getPrimaryAccountInfo(ConsentLevel.SYNC));
                            signedInAccount.set(
                                    identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN));

                            if (syncAccount.get() != null
                                    && !syncAccount.get().equals(signedInAccount.get())) {
                                throw new IllegalStateException(
                                        "Recorded signed in account differs from syncing account");
                            }

                            PrefService prefService = UserPrefs.get(profile);
                            for (PrefBackupSerializer serializer : NATIVE_PREFS_SERIALIZERS) {
                                for (Pair<String, byte[]> serializedNameAndValue :
                                        serializer.serializeAllowlistedPrefs(prefService)) {
                                    backupNames.add(serializedNameAndValue.first);
                                    backupValues.add(serializedNameAndValue.second);
                                }
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

        // Finally add the signed-in/syncing user ids.
        backupNames.add(ANDROID_DEFAULT_PREFIX + SYNCING_ACCOUNT_KEY);
        backupValues.add(
                ApiCompatibilityUtils.getBytesUtf8(
                        syncAccount.get() == null ? "" : syncAccount.get().getEmail()));
        backupNames.add(ANDROID_DEFAULT_PREFIX + SIGNED_IN_ACCOUNT_ID_KEY);
        backupValues.add(
                ApiCompatibilityUtils.getBytesUtf8(
                        signedInAccount.get() == null ? "" : signedInAccount.get().getGaiaId()));

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

        // TODO(https://crbug.com/353661640): Use return value to ensure `RestoreStatus` is provided
        //         by return statements.
        //
        // Non-timeout return statements in this method call `setRestoreStatus` before returning -
        // this is a fallback value that will be used if the restore flow times out or crashes.
        setRestoreStatus(RestoreStatus.RESTORE_STARTED_NOT_FINISHED);

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

        @Nullable String restoredSyncUserEmail = null;
        @Nullable String restoredSignedInUserID = null;
        while (data.readNextHeader()) {
            String key = data.getKey();
            int dataSize = data.getDataSize();
            byte[] buffer = new byte[dataSize];
            data.readEntityData(buffer, 0, dataSize);
            if (key.equals(ANDROID_DEFAULT_PREFIX + SYNCING_ACCOUNT_KEY)) {
                restoredSyncUserEmail = new String(buffer);
            } else if (key.equals(ANDROID_DEFAULT_PREFIX + SIGNED_IN_ACCOUNT_ID_KEY)) {
                restoredSignedInUserID = new String(buffer);
            } else {
                backupNames.add(key);
                backupValues.add(buffer);
            }
        }

        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    // Chrome library loading and metrics-related code below depend on PathUtils.
                    PathUtils.setPrivateDataDirectorySuffix(
                            SplitCompatApplication.PRIVATE_DATA_DIRECTORY_SUFFIX);
                });

        if (isMetricsReportingEnabled(backupNames, backupValues)) {
            try {
                enableRestoreFlowMetrics();
            } catch (IOException e) {
                // Couldn't enable metrics - log the error and try to proceed with the restore flow.
                Log.w(TAG, "Couldn't enable restore flow metrics", e);
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
        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    // TODO(crbug.com/40283943): Wait for AccountManagerFacade to load accounts.
                    createAsyncInitTaskRunner(latch)
                            .startBackgroundTasks(
                                    /* allocateChildConnection= */ false,
                                    /* fetchVariationSeed= */ true);
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
        boolean browserStarted =
                PostTask.runSynchronously(
                        TaskTraits.UI_DEFAULT,
                        () -> {
                            // Start the browser if necessary.
                            return initializeBrowser();
                        });
        if (!browserStarted) {
            // Something went wrong starting Chrome, skip the restore.
            setRestoreStatus(RestoreStatus.BROWSER_STARTUP_FAILED);
            return;
        }

        if (SigninFeatureMap.isEnabled(
                        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP)
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            final CountDownLatch accountsLatch = new CountDownLatch(1);
            PostTask.runSynchronously(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        AccountManagerFacadeProvider.getInstance()
                                .getCoreAccountInfos()
                                .then(
                                        (ignored) -> {
                                            accountsLatch.countDown();
                                        });
                    });
            try {
                // Explicit timeout is not needed here. In the scenario where accounts are not
                // available - the restore flow will be stopped several lines below. So, having an
                // explicit timeout would still result in the state not getting restored. Thus, it
                // is cleaner to just wait without an explicit timeout and rely on the BackupManager
                // killing the process if accounts never become available.
                accountsLatch.await();
            } catch (InterruptedException e) {
                // Normally, this shouldn't happen (Chrome process will just get killed). Use
                // `RESTORE_STARTED_NOT_FINISHED` as fallback in the unlikely scenario it happens.
                setRestoreStatus(RestoreStatus.RESTORE_STARTED_NOT_FINISHED);
                return;
            }
        }

        @Nullable
        CoreAccountInfo signedInAccountInfo = getDeviceAccountWithGaiaId(restoredSignedInUserID);
        @Nullable
        CoreAccountInfo syncAccountInfo = getDeviceAccountWithEmail(restoredSyncUserEmail);

        // If the user hasn't signed in, or can't sign in, then don't restore anything.
        if (syncAccountInfo == null
                && (signedInAccountInfo == null
                        || !SigninFeatureMap.isEnabled(
                                SigninFeatures
                                        .RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP))) {
            setRestoreStatus(RestoreStatus.NOT_SIGNED_IN);
            Log.i(TAG, "Chrome was not signed in with a known account name, not restoring");
            return;
        }

        // Restore the native preferences on the UI thread
        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    for (int i = 0; i < backupNames.size(); i++) {
                        for (PrefBackupSerializer s : NATIVE_PREFS_SERIALIZERS) {
                            if (s.tryDeserialize(
                                    prefService, backupNames.get(i), backupValues.get(i))) {
                                // Found the correct type.
                                break;
                            }
                        }
                    }

                    // Migrate global sync settings to account settings when necessary.
                    // It should be done after the restoration of the existing per-account settings
                    // from the backup to avoid override, as mentioned above.
                    final boolean shouldRestoreSelectedTypesAsAccountSettings =
                            syncAccountInfo != null
                                    && SigninFeatureMap.isEnabled(
                                            SigninFeatures
                                                    .RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP)
                                    && ChromeFeatureList.isEnabled(
                                            ChromeFeatureList
                                                    .REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);
                    if (shouldRestoreSelectedTypesAsAccountSettings) {
                        final String gaiaID =
                                syncAccountInfo != null
                                        ? syncAccountInfo.getGaiaId()
                                        : signedInAccountInfo.getGaiaId();
                        ChromeBackupAgentImplJni.get()
                                .migrateGlobalDataTypePrefsToAccount(prefService, gaiaID);
                    }

                    // TODO(crbug.com/332710541): Another commit is done for signed-in users in
                    // SigninManager.SignInCallback.onPrefsCommitted(). Do a single one instead.
                    ChromeBackupAgentImplJni.get().commitPendingPrefWrites(prefService);
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

        if (syncAccountInfo != null) {
            // Both accounts are recorded at the same time. Since only one account is in signed-in
            // state at a given time, they should be identical if both are valid.
            if (signedInAccountInfo != null && !signedInAccountInfo.equals(syncAccountInfo)) {
                throw new IllegalStateException(
                        "Recorded signed in account differs from syncing account");
            }

            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
                editor.apply();
                signInAndWaitForResult(syncAccountInfo);
            } else {
                // This will sign in the user on first run to the account in
                // BACKUP_FLOW_SIGNIN_ACCOUNT_NAME if any.
                editor.putString(
                        ChromePreferenceKeys.BACKUP_FLOW_SIGNIN_ACCOUNT_NAME,
                        restoredSyncUserEmail);
                editor.apply();

                // The silent first run will change things, so there is no point in trying to
                // prevent
                // additional backups at this stage. Don't write anything to |newState|.
                setRestoreStatus(RestoreStatus.RESTORE_COMPLETED);
            }
        } else {
            editor.apply();

            // signedInAccountInfo and syncAccountInfo should not be null at the same at this point.
            // If there's no valid syncing account and the signed-in account restore is disabled,
            // the restore should already be stopped and the restore state set to `NOT_SIGNED_IN`.
            if (signedInAccountInfo == null
                    || !SigninFeatureMap.isEnabled(
                            SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP)) {
                throw new IllegalStateException("No valid account can be signed-in");
            }

            signInAndWaitForResult(signedInAccountInfo);
        }
        Log.i(TAG, "Restore complete");
    }

    private boolean isMetricsReportingEnabled(
            ArrayList<String> backupNames, ArrayList<byte[]> backupValues) {
        Predicate<String> prefGetter =
                (String prefName) -> {
                    int index = backupNames.indexOf(ANDROID_DEFAULT_PREFIX + prefName);
                    return index != -1 && bytesToBoolean(backupValues.get(index));
                };
        return prefGetter.test(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY)
                && prefGetter.test(
                        ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER);
    }

    // TODO(crbug.com/338972271): Find a less hacky fix for restore flow metrics.
    private void enableRestoreFlowMetrics() throws IOException {
        File dataDirectory = new File(PathUtils.getDataDirectory());
        if (!dataDirectory.exists()) {
            dataDirectory.mkdir();
        }

        // Native code checks the whether metrics are enabled early during start-up - before
        // the restore flow can set the correct value in the Local State. To work around this,
        // create an empty file - the existence of this file will be used as the default value
        // for UMA reporting. It is safe to do here, because we know that metrics state will be
        // restored to enabled (due to the check above).
        final String consentFileName = "Consent To Send Stats";
        File consentFile = new File(dataDirectory, consentFileName);
        if (!consentFile.exists()) {
            consentFile.createNewFile();
        }

        // Chrome's process will be terminated after the restore flow is finished. To ensure
        // metrics from the restore process survive until the post-restore run and actually get
        // uploaded - persistent histograms should be backed by a memory-mapped file. This
        // memory-mapped file mechanic requires a spare file on Android (otherwise, histograms
        // are stored in memory, without the backing file and will be lost after the restore
        // process is finished). Normally, a spare file is created by the previous run of
        // Chrome. However, since the restore flow is the very first run for this particular
        // install - there won't be a spare file to be used, breaking persistent histograms and
        // thus restore flow metrics. To work around this issue and still get metrics from the
        // restore flow - create a spare file manually.
        // LINT.IfChange
        final String spareFileName = "BrowserMetrics-spare.pma";
        final int spareFileSize = 4 * 1024 * 1024;
        // LINT.ThenChange(/components/metrics/persistent_histograms.cc)

        File spareFile = new File(dataDirectory, spareFileName);
        try (OutputStream outputStream = new FileOutputStream(spareFile)) {
            // Zero-initialize the whole file to make sure the space is actually allocated and it
            // can be used for persisting histograms.
            byte[] buffer = new byte[8192];
            for (int writtenBytes = 0; writtenBytes < spareFileSize; ) {
                int writeSize = Math.min(buffer.length, spareFileSize - writtenBytes);
                outputStream.write(buffer, 0, writeSize);
                writtenBytes += writeSize;
            }
        } catch (IOException e) {
            // The writing failed in the middle - delete the file.
            spareFile.delete();
            throw e;
        }
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

    private @Nullable CoreAccountInfo getDeviceAccountWithEmail(@Nullable String accountEmail) {
        if (accountEmail == null) {
            return null;
        }

        return PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    return AccountUtils.findCoreAccountInfoByEmail(getAccountInfos(), accountEmail);
                });
    }

    private @Nullable CoreAccountInfo getDeviceAccountWithGaiaId(@Nullable String accountGaiaId) {
        if (accountGaiaId == null) {
            return null;
        }

        return PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    return AccountUtils.findCoreAccountInfoByGaiaId(
                            getAccountInfos(), accountGaiaId);
                });
    }

    private static List<CoreAccountInfo> getAccountInfos() {
        return AccountManagerFacadeProvider.getInstance().getCoreAccountInfos().getResult();
    }

    private static void signInAndWaitForResult(CoreAccountInfo accountInfo) {
        final CountDownLatch latch = new CountDownLatch(1);
        SigninManager.SignInCallback signInCallback =
                new SigninManager.SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        // Sign-in preferences need to be committed for the sign-in to be effective.
                        // Therefore the count down is done in `onPrefsCommitted` instead.
                    }

                    @Override
                    public void onPrefsCommitted() {
                        latch.countDown();
                    }

                    @Override
                    public void onSignInAborted() {
                        // Ignore failure as Chrome will simply remain signed-out otherwise, and the
                        // user is still able to sign-in manually after opening Chrome.
                        latch.countDown();
                    }
                };

        signIn(accountInfo, signInCallback);

        try {
            // Wait the sign-in to finish the restore. Otherwise, the account info request will be
            // cancelled one the restore ends. Timeout can be ignored as Chrome will simply remain
            // signed-out otherwise, and the user is still able to sign-in manually after opening
            // Chrome.
            boolean success = latch.await(SIGNIN_TIMEOUT_SECS, TimeUnit.SECONDS);
            int status = success ? RestoreStatus.RESTORE_COMPLETED : RestoreStatus.SIGNIN_TIMED_OUT;
            setRestoreStatus(status);
        } catch (InterruptedException e) {
            // Exception can be ignored as explained above.
            setRestoreStatus(RestoreStatus.SIGNIN_TIMED_OUT);
        }
    }

    private static void signIn(CoreAccountInfo accountInfo, SigninManager.SignInCallback callback) {
        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    SigninManager signinManager =
                            IdentityServicesProvider.get()
                                    .getSigninManager(ProfileManager.getLastUsedRegularProfile());
                    final AccountManagerFacade accountManagerFacade =
                            AccountManagerFacadeProvider.getInstance();

                    Callback<Boolean> accountManagedCallback =
                            (isManaged) -> {
                                // If restoring a managed account, the user most likely already
                                // accepted account management previously and we don't have the
                                // ability to re-show the confirmation dialog here anyways.
                                if (isManaged) signinManager.setUserAcceptedAccountManagement(true);
                                signinManager.runAfterOperationInProgress(
                                        () -> {
                                            signinManager.signin(
                                                    accountInfo,
                                                    SigninAccessPoint
                                                            .POST_DEVICE_RESTORE_BACKGROUND_SIGNIN,
                                                    callback);
                                        });
                            };

                    AccountManagerFacade.ChildAccountStatusListener listener =
                            (isChild, unused) -> {
                                if (isChild) {
                                    // TODO(crbug.com/40835324):
                                    // Pre-AllowSyncOffForChildAccounts, the backup sign-in for
                                    // child accounts would happen in SigninChecker anyways.
                                    // Maybe it should be handled by this  class once the
                                    // feature launches.
                                    callback.onSignInAborted();
                                    return;
                                }
                                signinManager.isAccountManaged(accountInfo, accountManagedCallback);
                            };

                    AccountUtils.checkChildAccountStatus(
                            accountManagerFacade, getAccountInfos(), listener);
                });
    }

    /**
     * Get the saved result of any restore that may have happened.
     *
     * @return the restore status, a RestoreStatus value.
     */
    @VisibleForTesting
    static @RestoreStatus int getRestoreStatus() {
        return ContextUtils.getAppSharedPreferences()
                .getInt(RESTORE_STATUS, RestoreStatus.NO_RESTORE);
    }

    /**
     * Save the restore status for later transfer to a histogram, and reset histogram recorded
     * status if needed.
     *
     * @param status the status.
     */
    @VisibleForTesting
    static void setRestoreStatus(@RestoreStatus int status) {
        assert status != RestoreStatus.DEPRECATED_RESTORE_STATUS_RECORDED
                && status != RestoreStatus.DEPRECATED_SIGNIN_TIMED_OUT;

        ContextUtils.getAppSharedPreferences().edit().putInt(RESTORE_STATUS, status).apply();
        if (isRestoreStatusRecorded()) {
            setRestoreStatusRecorded(false);
        }
    }

    /**
     * Get from the saved values whether the restore status histogram has been recorded.
     *
     * @return Whether the restore status has been recorded.
     */
    @VisibleForTesting
    static boolean isRestoreStatusRecorded() {
        return ContextUtils.getAppSharedPreferences().getBoolean(RESTORE_STATUS_RECORDED, false);
    }

    /**
     * Save the value indicating whether the restore status histogram has been recorded.
     *
     * @param isRecorded Whether the restore status is recorded.
     */
    @VisibleForTesting
    static void setRestoreStatusRecorded(boolean isRecorded) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(RESTORE_STATUS_RECORDED, isRecorded)
                .apply();
    }

    /** Record the restore histogram. To be called from Chrome itself once it is running. */
    public static void recordRestoreHistogram() {
        boolean isStatusRecorded = isRestoreStatusRecorded();
        // Ensure restore status is only recorded once.
        if (isStatusRecorded) {
            return;
        }

        @RestoreStatus int restoreStatus = getRestoreStatus();
        if (restoreStatus != RestoreStatus.DEPRECATED_RESTORE_STATUS_RECORDED
                && restoreStatus != RestoreStatus.DEPRECATED_SIGNIN_TIMED_OUT) {
            RecordHistogram.recordEnumeratedHistogram(
                    HISTOGRAM_ANDROID_RESTORE_RESULT, restoreStatus, RestoreStatus.NUM_ENTRIES);
        }
        setRestoreStatusRecorded(true);
    }

    @NativeMethods
    interface Natives {
        // See PrefService::CommitPendingWrite().
        void commitPendingPrefWrites(@JniType("PrefService*") PrefService prefService);

        // Calls syncer::MigrateGlobalDataTypePrefsToAccount() to migrate global boolean sync prefs
        // to account settings.
        void migrateGlobalDataTypePrefsToAccount(
                @JniType("PrefService*") PrefService prefService,
                @JniType("std::string") String gaiaId);
    }
}
