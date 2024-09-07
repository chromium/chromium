// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.backup;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.hasItem;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static java.util.function.Function.identity;

import android.app.backup.BackupDataInput;
import android.app.backup.BackupDataOutput;
import android.app.backup.BackupManager;
import android.content.SharedPreferences;
import android.os.ParcelFileDescriptor;
import android.util.Pair;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.AsyncInitTaskRunner;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.sync.internal.SyncPrefNames;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.common.ContentProcessInfo;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.stream.Collectors;

/** Unit tests for {@link org.chromium.chrome.browser.backup.ChromeBackupAgent}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            ChromeBackupAgentTest.BackupManagerShadow.class,
        })
@LooperMode(LooperMode.Mode.INSTRUMENTATION_TEST)
public class ChromeBackupAgentTest {
    @Rule public TemporaryFolder mTempDir = new TemporaryFolder();

    /** Shadow to allow counting of dataChanged calls. */
    @Implements(BackupManager.class)
    public static class BackupManagerShadow {
        private static int sDataChangedCalls;

        public static int getDataChangedCalls() {
            return sDataChangedCalls;
        }

        public static void clearDataChangedCalls() {
            sDataChangedCalls = 0;
        }

        @Implementation
        public void dataChanged() {
            sDataChangedCalls++;
        }
    }

    @Rule public JniMocker mocker = new JniMocker();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private ChromeBackupAgentImpl.Natives mChromeBackupAgentJniMock;
    @Mock private DictPrefBackupSerializer.Natives mDictPrefBackupSerializerJniMock;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefService;
    @Mock private Profile mProfile;
    @Mock private SigninManager mSigninManager;

    private ChromeBackupAgentImpl mAgent;
    private AsyncInitTaskRunner mTaskRunner;
    private boolean mIsAccountManaged;

    private final CoreAccountInfo mAccountInfo =
            CoreAccountInfo.createFromEmailAndGaiaId(
                    "user1", FakeAccountManagerFacade.toGaiaId("user1"));

    private static final String SHARED_PREF_NOT_BACKED_UP = "shared_pref_not_backed_up";
    private static final String NATIVE_PREF_NOT_BACKED_UP = "native_pref_not_backed_up";
    private static final String ACCOUNT_SETTINGS_PREF_VALUE = "account_settings_pref_value";
    private static final int BACKUP_BOOL_PREF_COUNT =
            new BoolPrefBackupSerializer().getAllowlistedPrefs().size()
                    + ChromeBackupAgentImpl.BACKUP_ANDROID_BOOL_PREFS.length;
    // The 2 additional preferences are: the syncing account and the signed-in account.
    private static final int BACKUP_PREF_COUNT =
            ChromeBackupAgentImpl.NATIVE_PREFS_SERIALIZERS.stream()
                            .mapToInt(serializer -> serializer.getAllowlistedPrefs().size())
                            .sum()
                    + ChromeBackupAgentImpl.BACKUP_ANDROID_BOOL_PREFS.length
                    + 2;
    // Number of preferences that default to true in the test, see setUpPrefsToBackup().
    private static final int DEFAULT_TRUE_BOOL_PREF_COUNT = 2;

    // Mutable map containing boolean preferences names and their values for the fake backup.
    private Map<String, Boolean> mNativeBoolPrefBackupValues =
            new BoolPrefBackupSerializer()
                    .getAllowlistedPrefs().stream()
                            .collect(Collectors.toMap(identity(), pref -> false));

    // Sets up default values for native and android prefs to be backed up.
    private void setUpPrefsToBackup(SharedPreferences prefs) {
        when(mDictPrefBackupSerializerJniMock.getSerializedDict(
                        mPrefService, SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT))
                .thenReturn(ACCOUNT_SETTINGS_PREF_VALUE);
        // Other boolean prefs in BACKUP_NATIVE_SYNC_TYPE_BOOL_PREFS are false by default.
        when(mPrefService.getBoolean(SyncPrefNames.SYNC_PASSWORDS)).thenReturn(true);

        SharedPreferences.Editor editor = prefs.edit();
        // In production some of these prefs can't be present in SharedPreferences at the same time,
        // but ChromeBackupAgentImpl is agnostic to that. The focus of these tests is making sure
        // that all the allowlisted prefs are backed up, and none other.
        editor.putBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, true);
        editor.putBoolean(ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, false);
        editor.putBoolean(ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE, false);
        editor.putBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, false);
        editor.putBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY, false);

        editor.putBoolean(SHARED_PREF_NOT_BACKED_UP, false);

        doReturn(mAccountInfo).when(mIdentityManagerMock).getPrimaryAccountInfo(anyInt());
        editor.apply();
    }

    @Before
    public void setUp() {
        // Create the agent to test; override fetching the task runner, and spy on the agent to
        // allow us to validate calls to these methods.
        mAgent =
                spy(
                        new ChromeBackupAgentImpl() {
                            @Override
                            AsyncInitTaskRunner createAsyncInitTaskRunner(CountDownLatch latch) {
                                latch.countDown();
                                return mTaskRunner;
                            }
                        });

        MockitoAnnotations.initMocks(this);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        mocker.mock(ChromeBackupAgentImplJni.TEST_HOOKS, mChromeBackupAgentJniMock);
        mocker.mock(DictPrefBackupSerializerJni.TEST_HOOKS, mDictPrefBackupSerializerJniMock);

        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);

        IdentityServicesProvider identityServicesProvider = mock(IdentityServicesProvider.class);
        IdentityServicesProvider.setInstanceForTests(identityServicesProvider);
        when(identityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.getPrimaryAccountInfo(ConsentLevel.SYNC)).thenReturn(null);
        when(identityServicesProvider.getSigninManager(any())).thenReturn(mSigninManager);
        doAnswer(
                        (invocation) -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mSigninManager)
                .runAfterOperationInProgress(any());

        doAnswer(
                        (invocation) -> {
                            SigninManager.SignInCallback callback = invocation.getArgument(2);
                            callback.onSignInComplete();
                            callback.onPrefsCommitted();
                            return null;
                        })
                .when(mSigninManager)
                .signin(eq(mAccountInfo), anyInt(), any());

        doAnswer(
                        (invocation) -> {
                            Callback<Boolean> callback = invocation.getArgument(1);
                            callback.onResult(mIsAccountManaged);
                            return null;
                        })
                .when(mSigninManager)
                .isAccountManaged(eq(mAccountInfo), any());

        // Mock initializing the browser
        doReturn(true).when(mAgent).initializeBrowser();

        // Mock the AsyncTaskRunner.
        mTaskRunner = mock(AsyncInitTaskRunner.class);

        ContentProcessInfo.setInChildProcess(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onBackup} testing first backup with a syncing
     * account.
     */
    @Test
    public void testOnBackup_firstBackup_syncing() throws IOException, ClassNotFoundException {
        // Mock the backup data.
        BackupDataOutput backupData = mock(BackupDataOutput.class);

        // Set up some preferences to back up.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        setUpPrefsToBackup(prefs);

        File stateFile = mTempDir.newFile();
        try (ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(stateFile, ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Run the test function.
            mAgent.onBackup(null, backupData, newState);
        }

        // Check that the right things were written to the backup
        verify(backupData).writeEntityHeader("native." + SyncPrefNames.SYNC_PASSWORDS, 1);
        byte[] accountSettingsPrefBytes =
                ApiCompatibilityUtils.getBytesUtf8(ACCOUNT_SETTINGS_PREF_VALUE);
        verify(backupData)
                .writeEntityHeader(
                        "NativeJsonDict." + SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT,
                        accountSettingsPrefBytes.length);
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault." + ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, 1);
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault." + ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, 1);
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault."
                                + ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE,
                        1);
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault."
                                + ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER,
                        1);

        verify(backupData, times(DEFAULT_TRUE_BOOL_PREF_COUNT)).writeEntityData(new byte[] {1}, 1);
        verify(backupData, times(BACKUP_BOOL_PREF_COUNT - DEFAULT_TRUE_BOOL_PREF_COUNT))
                .writeEntityData(new byte[] {0}, 1);
        byte[] unameBytes = ApiCompatibilityUtils.getBytesUtf8(mAccountInfo.getEmail());
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault." + ChromeBackupAgentImpl.SYNCING_ACCOUNT_KEY,
                        unameBytes.length);
        verify(backupData).writeEntityData(unameBytes, unameBytes.length);
        byte[] uidBytes = ApiCompatibilityUtils.getBytesUtf8(mAccountInfo.getGaiaId());
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault." + ChromeBackupAgentImpl.SIGNED_IN_ACCOUNT_ID_KEY,
                        uidBytes.length);
        verify(backupData).writeEntityData(uidBytes, uidBytes.length);

        verify(backupData, times(0))
                .writeEntityHeader(eq("AndroidDefault." + SHARED_PREF_NOT_BACKED_UP), anyInt());

        // Check that the state was saved correctly.
        try (ObjectInputStream newStateStream =
                new ObjectInputStream(new FileInputStream(stateFile))) {
            ArrayList<String> names = (ArrayList<String>) newStateStream.readObject();
            assertThat(names.size(), equalTo(BACKUP_PREF_COUNT));
            assertThat(names, hasItem("native." + SyncPrefNames.SYNC_PASSWORDS));
            assertThat(
                    names, hasItem("NativeJsonDict." + SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT));
            assertThat(
                    names,
                    hasItem("AndroidDefault." + ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE));
            assertThat(
                    names,
                    hasItem(
                            "AndroidDefault."
                                    + ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED));
            assertThat(
                    names,
                    hasItem(
                            "AndroidDefault."
                                    + ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE));
            assertThat(
                    names,
                    hasItem(
                            "AndroidDefault."
                                    + ChromePreferenceKeys
                                            .PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER));
            assertThat(
                    names, hasItem("AndroidDefault." + ChromeBackupAgentImpl.SYNCING_ACCOUNT_KEY));
            assertThat(
                    names,
                    hasItem("AndroidDefault." + ChromeBackupAgentImpl.SIGNED_IN_ACCOUNT_ID_KEY));
            ArrayList<byte[]> values = (ArrayList<byte[]>) newStateStream.readObject();
            assertThat(values.size(), equalTo(BACKUP_PREF_COUNT));
            assertThat(values, hasItem(unameBytes));
            assertThat(values, hasItem(uidBytes));
            assertThat(values, hasItem(accountSettingsPrefBytes));
            assertThat(values, hasItem(new byte[] {0}));
            assertThat(values, hasItem(new byte[] {1}));

            // Make sure that there are no extra objects.
            assertThat(newStateStream.available(), equalTo(0));
        }
    }

    /**
     * Test method for {@link ChromeBackupAgent#onBackup} testing first backup with a signed-in only
     * user.
     */
    @Test
    public void testOnBackup_firstBackup_signedInNotSyncing()
            throws IOException, ClassNotFoundException {
        // Mock the backup data.
        BackupDataOutput backupData = mock(BackupDataOutput.class);

        // Set up some preferences to back up.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        setUpPrefsToBackup(prefs);
        doReturn(null).when(mIdentityManagerMock).getPrimaryAccountInfo(ConsentLevel.SYNC);

        File stateFile = mTempDir.newFile();
        try (ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(stateFile, ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Run the test function.
            mAgent.onBackup(null, backupData, newState);
        }

        // Check that the right things were written to the backup
        verify(backupData).writeEntityHeader("native." + SyncPrefNames.SYNC_PASSWORDS, 1);
        byte[] accountSettingsPrefBytes =
                ApiCompatibilityUtils.getBytesUtf8(ACCOUNT_SETTINGS_PREF_VALUE);
        verify(backupData)
                .writeEntityHeader(
                        "NativeJsonDict." + SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT,
                        accountSettingsPrefBytes.length);
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault." + ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, 1);
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault." + ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, 1);
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault."
                                + ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE,
                        1);
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault."
                                + ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER,
                        1);

        verify(backupData, times(DEFAULT_TRUE_BOOL_PREF_COUNT)).writeEntityData(new byte[] {1}, 1);
        verify(backupData, times(BACKUP_BOOL_PREF_COUNT - DEFAULT_TRUE_BOOL_PREF_COUNT))
                .writeEntityData(new byte[] {0}, 1);
        byte[] unameBytes = ApiCompatibilityUtils.getBytesUtf8(mAccountInfo.getEmail());
        verify(backupData, times(0))
                .writeEntityHeader(
                        "AndroidDefault." + ChromeBackupAgentImpl.SYNCING_ACCOUNT_KEY,
                        unameBytes.length);
        byte[] uidBytes = ApiCompatibilityUtils.getBytesUtf8(mAccountInfo.getGaiaId());
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault." + ChromeBackupAgentImpl.SIGNED_IN_ACCOUNT_ID_KEY,
                        uidBytes.length);
        verify(backupData).writeEntityData(uidBytes, uidBytes.length);

        verify(backupData, times(0))
                .writeEntityHeader(eq("AndroidDefault." + SHARED_PREF_NOT_BACKED_UP), anyInt());

        // Check that the state was saved correctly
        try (ObjectInputStream newStateStream =
                new ObjectInputStream(new FileInputStream(stateFile))) {
            ArrayList<String> names = (ArrayList<String>) newStateStream.readObject();
            assertThat(names.size(), equalTo(BACKUP_PREF_COUNT));
            assertThat(names, hasItem("native." + SyncPrefNames.SYNC_PASSWORDS));
            assertThat(
                    names, hasItem("NativeJsonDict." + SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT));
            assertThat(
                    names,
                    hasItem("AndroidDefault." + ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE));
            assertThat(
                    names,
                    hasItem(
                            "AndroidDefault."
                                    + ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED));
            assertThat(
                    names,
                    hasItem(
                            "AndroidDefault."
                                    + ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE));
            assertThat(
                    names,
                    hasItem(
                            "AndroidDefault."
                                    + ChromePreferenceKeys
                                            .PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER));
            assertThat(
                    names, hasItem("AndroidDefault." + ChromeBackupAgentImpl.SYNCING_ACCOUNT_KEY));
            assertThat(
                    names,
                    hasItem("AndroidDefault." + ChromeBackupAgentImpl.SIGNED_IN_ACCOUNT_ID_KEY));
            ArrayList<byte[]> values = (ArrayList<byte[]>) newStateStream.readObject();
            assertThat(values.size(), equalTo(BACKUP_PREF_COUNT));
            assertThat(values, not(hasItem(unameBytes)));
            assertThat(values, hasItem(uidBytes));
            assertThat(values, hasItem(accountSettingsPrefBytes));
            assertThat(values, hasItem(new byte[] {0}));
            assertThat(values, hasItem(new byte[] {1}));

            // Make sure that there are no extra objects.
            assertThat(newStateStream.available(), equalTo(0));
        }
    }

    /** Test method for {@link ChromeBackupAgent#onBackup} a second backup with the same data */
    @Test
    @SuppressWarnings("unchecked")
    public void testOnBackup_duplicateBackup()
            throws FileNotFoundException, IOException, ClassNotFoundException {
        // Mock the backup data.
        BackupDataOutput backupData = mock(BackupDataOutput.class);

        // Set up some preferences to back up.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        setUpPrefsToBackup(prefs);

        File stateFile1 = mTempDir.newFile();
        try (ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(stateFile1, ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a first backup.
            mAgent.onBackup(null, backupData, newState);
        }

        // Minimal check on first backup, this isn't the test here.
        verify(backupData, times(BACKUP_PREF_COUNT)).writeEntityHeader(anyString(), anyInt());
        verify(backupData, times(BACKUP_PREF_COUNT)).writeEntityData(any(byte[].class), anyInt());

        File stateFile2 = mTempDir.newFile();
        try (ParcelFileDescriptor oldState =
                        ParcelFileDescriptor.open(stateFile1, ParcelFileDescriptor.MODE_READ_ONLY);
                ParcelFileDescriptor newState =
                        ParcelFileDescriptor.open(
                                stateFile2, ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Try a second backup without changing any data
            mAgent.onBackup(oldState, backupData, newState);
        }
        // Check that the second backup didn't write anything.
        verifyNoMoreInteractions(backupData);

        // The two state files should contain identical data.
        try (ObjectInputStream oldStateStream =
                        new ObjectInputStream(new FileInputStream(stateFile1));
                ObjectInputStream newStateStream =
                        new ObjectInputStream(new FileInputStream(stateFile2))) {
            ArrayList<String> oldNames = (ArrayList<String>) oldStateStream.readObject();
            ArrayList<byte[]> oldValues = (ArrayList<byte[]>) oldStateStream.readObject();
            ArrayList<String> newNames = (ArrayList<String>) newStateStream.readObject();
            ArrayList<byte[]> newValues = (ArrayList<byte[]>) newStateStream.readObject();
            assertThat(newNames, equalTo(oldNames));
            assertTrue(Arrays.deepEquals(newValues.toArray(), oldValues.toArray()));
            assertThat(newStateStream.available(), equalTo(0));
        }
    }

    /** Test method for {@link ChromeBackupAgent#onBackup} a second backup with different data */
    @Test
    @SuppressWarnings("unchecked")
    public void testOnBackup_dataChanged()
            throws FileNotFoundException, IOException, ClassNotFoundException {
        // Mock the backup data.
        BackupDataOutput backupData = mock(BackupDataOutput.class);

        // Set up some preferences to back up.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        setUpPrefsToBackup(prefs);

        // Create a state file.
        File stateFile1 = mTempDir.newFile();
        try (ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(stateFile1, ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a first backup.
            mAgent.onBackup(null, backupData, newState);
        }
        // Minimal check on first backup, this isn't the test here.
        verify(backupData, times(BACKUP_PREF_COUNT)).writeEntityHeader(anyString(), anyInt());
        verify(backupData, times(BACKUP_PREF_COUNT)).writeEntityData(any(byte[].class), anyInt());

        // Change some data.
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, true);
        editor.apply();
        reset(backupData);

        File stateFile2 = mTempDir.newFile();
        try (ParcelFileDescriptor oldState =
                        ParcelFileDescriptor.open(
                                stateFile1, ParcelFileDescriptor.MODE_WRITE_ONLY);
                ParcelFileDescriptor newState =
                        ParcelFileDescriptor.open(
                                stateFile2, ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a second backup.
            mAgent.onBackup(oldState, backupData, newState);
        }

        // Check that the second backup wrote something.
        verify(backupData, times(BACKUP_PREF_COUNT)).writeEntityHeader(anyString(), anyInt());
        verify(backupData, times(BACKUP_PREF_COUNT)).writeEntityData(any(byte[].class), anyInt());

        // the two state files should contain different data (although the names are unchanged).
        try (ObjectInputStream oldStateStream =
                        new ObjectInputStream(new FileInputStream(stateFile1));
                ObjectInputStream newStateStream =
                        new ObjectInputStream(new FileInputStream(stateFile2))) {
            ArrayList<String> oldNames = (ArrayList<String>) oldStateStream.readObject();
            ArrayList<byte[]> oldValues = (ArrayList<byte[]>) oldStateStream.readObject();
            ArrayList<String> newNames = (ArrayList<String>) newStateStream.readObject();
            ArrayList<byte[]> newValues = (ArrayList<byte[]>) newStateStream.readObject();
            assertThat(newNames, equalTo(oldNames));
            assertFalse(Arrays.deepEquals(newValues.toArray(), oldValues.toArray()));
            assertThat(newStateStream.available(), equalTo(0));
        }
    }

    /** Test method for {@link ChromeBackupAgent#onBackup} when browser startup fails */
    @Test
    public void testOnBackup_browserStartupFails() throws IOException {
        BackupDataOutput backupData = mock(BackupDataOutput.class);
        ParcelFileDescriptor mockState = mock(ParcelFileDescriptor.class);

        doReturn(false).when(mAgent).initializeBrowser();

        BackupManagerShadow.clearDataChangedCalls();
        mAgent.onBackup(null, backupData, mockState);
        assertThat(BackupManagerShadow.getDataChangedCalls(), equalTo(1));
        verifyNoMoreInteractions(backupData);
        verifyNoMoreInteractions(mockState);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertThat(prefs.getInt(ChromeBackupAgentImpl.BACKUP_FAILURE_COUNT, 0), equalTo(1));

        // Check that the backup agent gives up retrying after too many failures
        prefs.edit()
                .putInt(
                        ChromeBackupAgentImpl.BACKUP_FAILURE_COUNT,
                        ChromeBackupAgentImpl.MAX_BACKUP_FAILURES)
                .apply();
        mAgent.onBackup(null, backupData, mockState);
        assertThat(BackupManagerShadow.getDataChangedCalls(), equalTo(1));

        // Check that a successful backup resets the failure count
        doReturn(true).when(mAgent).initializeBrowser();
        // Set up some preferences to back up.
        setUpPrefsToBackup(prefs);
        // A successful backup needs a real state file, or lots more mocking.
        try (ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(
                        mTempDir.newFile(), ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            mAgent.onBackup(null, backupData, newState);
        }
        assertThat(prefs.getInt(ChromeBackupAgentImpl.BACKUP_FAILURE_COUNT, 0), equalTo(0));
        verify(backupData, times(DEFAULT_TRUE_BOOL_PREF_COUNT)).writeEntityData(new byte[] {1}, 1);
    }

    private BackupDataInput createMockBackupData(
            boolean hasSyncingUser, boolean hasSignedInUser, boolean hasAccountSettings)
            throws IOException {
        // Mock the backup data
        BackupDataInput backupData = mock(BackupDataInput.class);

        String syncingUserEmail = hasSyncingUser ? mAccountInfo.getEmail() : "";
        String signedInUserGaiaId = hasSignedInUser ? mAccountInfo.getGaiaId() : "";
        ArrayList<Pair<String, byte[]>> keysAndValues =
                new ArrayList(
                        Arrays.asList(
                                new Pair<>("native." + NATIVE_PREF_NOT_BACKED_UP, new byte[] {1}),
                                new Pair<>(
                                        "AndroidDefault."
                                                + ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE,
                                        new byte[] {1}),
                                new Pair<>("AndroidDefault.junk", new byte[] {23, 42}),
                                new Pair<>(
                                        "AndroidDefault."
                                                + ChromeBackupAgentImpl.SYNCING_ACCOUNT_KEY,
                                        ApiCompatibilityUtils.getBytesUtf8(syncingUserEmail)),
                                new Pair<>(
                                        "AndroidDefault."
                                                + ChromeBackupAgentImpl.SIGNED_IN_ACCOUNT_ID_KEY,
                                        ApiCompatibilityUtils.getBytesUtf8(signedInUserGaiaId))));

        for (Map.Entry<String, Boolean> entry : mNativeBoolPrefBackupValues.entrySet()) {
            byte[] value = entry.getValue() ? new byte[] {1} : new byte[] {0};
            keysAndValues.add(new Pair<>("native." + entry.getKey(), value));
        }

        if (hasAccountSettings) {
            keysAndValues.add(
                    new Pair<>(
                            "NativeJsonDict." + SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT,
                            ApiCompatibilityUtils.getBytesUtf8(ACCOUNT_SETTINGS_PREF_VALUE)));
        }

        when(backupData.getKey())
                .thenAnswer(
                        new Answer<String>() {
                            private int mPos;

                            @Override
                            public String answer(InvocationOnMock invocation) {
                                return keysAndValues.get(mPos++).first;
                            }
                        });

        when(backupData.getDataSize())
                .thenAnswer(
                        new Answer<Integer>() {
                            private int mPos;

                            @Override
                            public Integer answer(InvocationOnMock invocation) {
                                return keysAndValues.get(mPos++).second.length;
                            }
                        });

        when(backupData.readEntityData(any(byte[].class), anyInt(), anyInt()))
                .thenAnswer(
                        new Answer<Integer>() {
                            private int mPos;

                            @Override
                            public Integer answer(InvocationOnMock invocation) {
                                byte[] buffer = invocation.getArgument(0);
                                for (int i = 0; i < keysAndValues.get(mPos).second.length; i++) {
                                    buffer[i] = keysAndValues.get(mPos).second[i];
                                }
                                return keysAndValues.get(mPos++).second.length;
                            }
                        });

        when(backupData.readNextHeader())
                .thenAnswer(
                        new Answer<Boolean>() {
                            private int mPos;

                            @Override
                            public Boolean answer(InvocationOnMock invocation) {
                                return mPos++ < keysAndValues.size();
                            }
                        });
        return backupData;
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains the previously
     * syncing user only.
     */
    @Test
    @DisableFeatures({
        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void testOnRestore_withSyncUser_signInRestoreDisabled_replaceSyncBySigninDisabled()
            throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ true,
                /* withSignedInUser= */ false,
                /* withAccountSettings= */ true);
        verify(mTaskRunner)
                .startBackgroundTasks(
                        /* allocateChildConnection= */ false, /* initVariationSeed= */ true);

        verifyRestoreFinishWithSigninAndSync();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(false);
        // Verify that bool prefs are not migrated to account settings, since flags are disabled.
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains the previously
     * syncing user only.
     */
    @Test
    @DisableFeatures({SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testOnRestore_withSyncUser_signInRestoreDisabled_syncToSigninEnabled()
            throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ true,
                /* withSignedInUser= */ false,
                /* withAccountSettings= */ true);

        verifyRestoreFinishWithSignin();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(false);
        // Verify that bool prefs are not migrated to account settings, since account settings
        // restore flag is disabled.
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains the previously
     * signed-in user only.
     */
    @Test
    @EnableFeatures({SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP})
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testOnRestore_withSignInUser_signInRestoreEnabled_replaceSyncBySigninDisabled()
            throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ false,
                /* withSignedInUser= */ true,
                /* withAccountSettings= */ true);

        verifyRestoreFinishWithSignin();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(true);
        // Verify that bool prefs are not migrated to account settings, since the UNO flag is
        // disabled.
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains the previously
     * signed-in user only, and does not contain account settings backup.
     */
    @Test
    @EnableFeatures({SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP})
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void
            testOnRestore_withSignInUser_signInRestoreEnabled_replaceSyncBySigninDisabled_noAccountSettings()
                    throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ false,
                /* withSignedInUser= */ true,
                /* withAccountSettings= */ false);

        verifyRestoreFinishWithSignin();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(false);
        // Verify that bool prefs are not migrated to account settings, since the UNO flag is
        // disabled.
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains the previously
     * signed-in user only.
     */
    @Test
    @EnableFeatures({
        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void testOnRestore_withSignInUser_signInRestoreEnabled_syncToSigninEnabled()
            throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ false,
                /* withSignedInUser= */ true,
                /* withAccountSettings= */ true);

        verifyRestoreFinishWithSignin();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(true);
        // Verify that bool prefs are not migrated to account settings, since the backed-up user
        // is not previously syncing, even if the flags are enabled.
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains the previously
     * signed-in user only, and does not contain account settings backup.
     */
    @Test
    @EnableFeatures({
        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void
            testOnRestore_withSignInUser_signInRestoreEnabled_syncToSigninEnabled_noAccountSettings()
                    throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ false,
                /* withSignedInUser= */ true,
                /* withAccountSettings= */ false);

        verifyRestoreFinishWithSignin();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(false);
        // Verify that bool prefs are not migrated to account settings, since the backed-up user
        // is not previously syncing.
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains the previously
     * signed-in user only.
     */
    @Test
    @EnableFeatures({
        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void testOnRestore_withSignInUser() throws IOException {
        mIsAccountManaged = true;
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ false,
                /* withSignedInUser= */ true,
                /* withAccountSettings= */ true);

        verifyRestoreFinishWithSignin();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(true);
        // Verify that bool prefs are not migrated to account settings, since the backed-up user
        // is not previously syncing.
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains the previously
     * signed-in user only.
     */
    @Test
    @EnableFeatures({
        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
    })
    public void testOnRestore_withSignInUser_notManaged() throws IOException {
        mIsAccountManaged = false;
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ false,
                /* withSignedInUser= */ true,
                /* withAccountSettings= */ true);

        verifyRestoreFinishWithSignin();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(true);
        // Verify that bool prefs are not migrated to account settings, since the backed-up user
        // is not previously syncing.
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains a record for the
     * previously signed-in user and another for the syncing user.
     */
    @Test
    @EnableFeatures({SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP})
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void
            testOnRestore_withSignInAndSyncUser_signInRestoreEnabled_replaceSyncBySigninDisabled()
                    throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ true,
                /* withSignedInUser= */ true,
                /* withAccountSettings= */ true);

        verifyRestoreFinishWithSigninAndSync();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(true);
        // Verify that bool prefs are not migrated to account settings, since we are not converting
        // previously syncing user to sign-in.
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains a record for the
     * previously signed-in user and another for the syncing user.
     */
    @Test
    @EnableFeatures({
        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void testOnRestore_withSignInAndSyncUser_signInRestoreEnabled_syncToSigninEnabled()
            throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ true,
                /* withSignedInUser= */ true,
                /* withAccountSettings= */ true);

        // Verify sign-in restoration.
        verifyRestoreFinishWithSignin();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(true);
        // Verify that global prefs are migrated to account settings, given that flags are enabled,
        // and the backed-up account was a syncing one.
        verifyBoolPrefsMigratedToAccountSettings(true);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains a record for the
     * previously signed-in user and another for the syncing user, and no account settings.
     */
    @Test
    @EnableFeatures({
        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void
            testOnRestore_withSignInAndSyncUser_signInRestoreEnabled_syncToSigninEnabled_noAccountSettings()
                    throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ true,
                /* withSignedInUser= */ true,
                /* withAccountSettings= */ false);

        // Verify sign-in restoration.
        verifyRestoreFinishWithSignin();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(false);
        // Verify that global prefs are migrated to account settings, given that flags are enabled,
        // and the backed-up account was a syncing one.
        verifyBoolPrefsMigratedToAccountSettings(true);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains a record for the
     * previously signed-in user and another for the syncing user.
     */
    @Test
    @DisableFeatures({
        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void
            testOnRestore_withSignInAndSyncUser_signInRestoreDisabled_replaceSyncBySigninDisabled()
                    throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ true,
                /* withSignedInUser= */ true,
                /* withAccountSettings= */ true);

        verifyRestoreFinishWithSigninAndSync();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(false);
        // Verify that bool prefs are not migrated to account settings since flags are disabled.
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains a record for the
     * previously signed-in user and another for the syncing user.
     */
    @Test
    @DisableFeatures({SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testOnRestore_withSignInAndSyncUser_signInRestoreDisabled_syncToSigninEnabled()
            throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ true,
                /* withSignedInUser= */ true,
                /* withAccountSettings= */ true);

        verifyRestoreFinishWithSignin();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(false);
        // Verify that bool prefs are not migrated to account settings, since account settings
        // restore flag is disabled.
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains a record for the
     * previously syncing user only.
     */
    @Test
    @EnableFeatures({SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP})
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testOnRestore_withSyncUser_signInRestoreEnabled_replaceSyncBySigninDisabled()
            throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ true,
                /* withSignedInUser= */ false,
                /* withAccountSettings= */ true);

        verifyRestoreFinishWithSigninAndSync();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(true);
        // Verify that bool prefs are not migrated to account settings, since syncing user is not
        // converted to sign-in only mode.
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains a record for the
     * previously syncing user, and a record for account settings.
     */
    @Test
    @EnableFeatures({
        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void
            testOnRestore_withSyncUserAndAccountSettings_signInRestoreEnabled_syncToSigninEnabled()
                    throws IOException {
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ true,
                /* withSignedInUser= */ false,
                /* withAccountSettings= */ true);

        verifyRestoreFinishWithSignin();
        verifySyncTypeBoolPrefsRestored(true);
        InOrder inOrder =
                inOrder(mChromeBackupAgentJniMock, mDictPrefBackupSerializerJniMock, mPrefService);
        inOrder.verify(mPrefService, times(mNativeBoolPrefBackupValues.size()))
                .setBoolean(anyString(), anyBoolean());
        inOrder.verify(mDictPrefBackupSerializerJniMock, times(1))
                .setDict(
                        mPrefService,
                        SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT,
                        ACCOUNT_SETTINGS_PREF_VALUE);
        // Verify that global prefs are migrated to account settings, after the account setting
        // backup is restored.
        // The migration is done since the corresponding flags are enabled, and the backed-up
        // account was a syncing one.
        inOrder.verify(mChromeBackupAgentJniMock, times(1)).commitPendingPrefWrites(mPrefService);
        inOrder.verifyNoMoreInteractions();
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}. The backup contains a record for the
     * previously syncing user only, and the backup value for SYNC_KEEP_EVERYTHING_SYNCED is true.
     */
    @Test
    @EnableFeatures({
        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void testOnRestore_withSyncUser_syncEverything_signInRestoreEnabled_syncToSigninEnabled()
            throws IOException {
        mNativeBoolPrefBackupValues.put(SyncPrefNames.SYNC_KEEP_EVERYTHING_SYNCED, true);
        executeNormalRestoreAndCheckPrefs(
                /* withSyncingUser= */ true,
                /* withSignedInUser= */ false,
                /* withAccountSettings= */ true);

        verifyRestoreFinishWithSignin();
        verifySyncTypeBoolPrefsRestored(true);
        verifyAccountSettingsBackupRestored(true);
        // Verify that global prefs are migrated to account settings, given that flags are enabled,
        // and the backed-up account was a syncing one.
        verifyBoolPrefsMigratedToAccountSettings(true);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore} with valid signed-in user but no syncing
     * user. With signed-in user restore disabled, the restore should fail since no valid syncing
     * user was recorded.
     */
    @Test
    @DisableFeatures({
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
        SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP
    })
    public void testOnRestore_failure_signinUserOnly_signedInRestoreDisabled() throws IOException {
        BackupDataInput backupData =
                createMockBackupData(
                        /* hasSyncingUser= */ false,
                        /* hasSignedInUser= */ true,
                        /* hasAccountSettings= */ true);
        mAccountManagerTestRule.addAccount(mAccountInfo.getEmail());

        try (ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(
                        mTempDir.newFile(), ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a restore.
            mAgent.onRestore(backupData, 0, newState);
        }

        // Verify that the restore is not done since no valid account can be signed-in.
        // The sign-in account was recorded, but the sign-in account restore flag is disabled. Given
        // that no syncing account has been recorded, no account can be restored here so the restore
        // should be skipped.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertFalse(prefs.contains(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE));
        verify(mPrefService, never()).setBoolean(any(), anyBoolean());
        verify(mTaskRunner)
                .startBackgroundTasks(
                        /* allocateChildConnection= */ false, /* initVariationSeed= */ true);

        // Verify that no sign-in or prefs restoration is done.
        verifyRestoreFinishWithoutSignin();
        verifySyncTypeBoolPrefsRestored(false);
        verifyAccountSettingsBackupRestored(false);
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore} for a user that doesn't exist on the
     * device. Since the recorded signed-in account is not present on the device and can't be
     * signed-in, the restore should be skipped.
     */
    @Test
    @EnableFeatures({SigninFeatures.RESTORE_SIGNED_IN_ACCOUNT_AND_SETTINGS_FROM_BACKUP})
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testOnRestore_badUser_signedInRestoreEnabled() throws IOException {
        BackupDataInput backupData =
                createMockBackupData(
                        /* hasSyncingUser= */ true,
                        /* hasSignedInUser= */ true,
                        /* hasAccountSettings= */ true);

        try (ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(
                        mTempDir.newFile(), ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a restore.
            mAgent.onRestore(backupData, 0, newState);
        }

        // Verify that the restore is not done since no valid account can be signed-in.
        // The signed-in & syncing account is recorded in the backup, but is not present on the
        // device, so the sign-in can't be done.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertFalse(prefs.contains(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE));
        verify(mPrefService, never()).setBoolean(any(), anyBoolean());
        verify(mTaskRunner)
                .startBackgroundTasks(
                        /* allocateChildConnection= */ false, /* initVariationSeed= */ true);

        // Verify that no sign-in or prefs restoration is done.
        verifyRestoreFinishWithoutSignin();
        verifySyncTypeBoolPrefsRestored(false);
        verifyAccountSettingsBackupRestored(false);
        verifyBoolPrefsMigratedToAccountSettings(false);
    }

    /** Test method for {@link ChromeBackupAgent#onRestore} for browser startup failure */
    @Test
    public void testOnRestore_browserStartupFails() throws IOException {
        BackupDataInput backupData =
                createMockBackupData(
                        /* hasSyncingUser= */ true,
                        /* hasSignedInUser= */ true,
                        /* hasAccountSettings= */ true);
        doReturn(false).when(mAgent).initializeBrowser();

        try (ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(
                        mTempDir.newFile(), ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a restore.
            mAgent.onRestore(backupData, 0, newState);
        }
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertFalse(prefs.contains(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE));

        // Test that the status of the restore has been recorded.
        assertThat(
                ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.BROWSER_STARTUP_FAILED));
    }

    /** Test method for {@link ChromeBackupAgent#onRestore} for browser startup failure */
    @Test
    public void testOnRestore_afterFirstRun() throws IOException {
        BackupDataInput backupData =
                createMockBackupData(
                        /* hasSyncingUser= */ true,
                        /* hasSignedInUser= */ true,
                        /* hasAccountSettings= */ true);
        FirstRunStatus.setFirstRunFlowComplete(true);

        try (ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(
                        mTempDir.newFile(), ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a restore.
            mAgent.onRestore(backupData, 0, newState);
        }
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertTrue(prefs.contains(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE));

        // Test that the status of the restore has been recorded.
        assertThat(
                ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.RESTORE_AFTER_FIRST_RUN));
    }

    /** Test of {@link ChromeBackupAgent#getRestoreStatus} */
    @Test
    public void testGetRestoreStatus() {
        // Test default value
        assertThat(
                ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.NO_RESTORE));

        // Test that the value can be changed
        ChromeBackupAgentImpl.setRestoreStatus(
                ChromeBackupAgentImpl.RestoreStatus.RESTORE_AFTER_FIRST_RUN);
        assertThat(
                ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.RESTORE_AFTER_FIRST_RUN));

        // Prove that the value equalTo held in the app preferences (and not, for example, in a
        // static).
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        assertThat(
                ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.NO_RESTORE));

        // Test that ChromeBackupAgentImpl.setRestoreStatus really looks at the argument.
        ChromeBackupAgentImpl.setRestoreStatus(
                ChromeBackupAgentImpl.RestoreStatus.BROWSER_STARTUP_FAILED);
        assertThat(
                ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.BROWSER_STARTUP_FAILED));

        // Test the remaining values are implemented
        ChromeBackupAgentImpl.setRestoreStatus(ChromeBackupAgentImpl.RestoreStatus.NOT_SIGNED_IN);
        assertThat(
                ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.NOT_SIGNED_IN));
        ChromeBackupAgentImpl.setRestoreStatus(
                ChromeBackupAgentImpl.RestoreStatus.RESTORE_COMPLETED);
        assertThat(
                ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.RESTORE_COMPLETED));
        ChromeBackupAgentImpl.setRestoreStatus(
                ChromeBackupAgentImpl.RestoreStatus.SIGNIN_TIMED_OUT);
        assertThat(
                ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.SIGNIN_TIMED_OUT));
    }

    /**
     * Test normal browser startup. This is not tested by the other tests, since, until recently,
     * it was not possible to mock ChromeBrowserInitializer, so initializeBrowser is mocked.
     *
     * TODO (aberent) remove mocking of initializeBrowser in the other tests.
     */
    @Test
    public void testInitializeBrowser_normal() {
        ChromeBackupAgentImpl agent = new ChromeBackupAgentImpl();
        ChromeBrowserInitializer initializer = mock(ChromeBrowserInitializer.class);
        ChromeBrowserInitializer.setForTesting(initializer);
        assertTrue(agent.initializeBrowser());
    }

    /**
     * Test that browser startup fails when in a child process. This is important because of
     * https://crbug.com/718166
     */
    @Test
    public void testInitializeBrowser_childProcess() {
        ContentProcessInfo.setInChildProcess(true);
        ChromeBackupAgentImpl agent = new ChromeBackupAgentImpl();
        ChromeBrowserInitializer initializer = mock(ChromeBrowserInitializer.class);
        ChromeBrowserInitializer.setForTesting(initializer);
        assertFalse(agent.initializeBrowser());
        verifyNoMoreInteractions(initializer);
    }

    private void executeNormalRestoreAndCheckPrefs(
            boolean withSyncingUser, boolean withSignedInUser, boolean withAccountSettings)
            throws IOException {
        BackupDataInput backupData =
                createMockBackupData(
                        /* hasSyncingUser= */ withSyncingUser,
                        /* hasSignedInUser= */ withSignedInUser,
                        /* hasAccountSettings= */ withAccountSettings);
        mAccountManagerTestRule.addAccount(mAccountInfo.getEmail());

        try (ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(
                        mTempDir.newFile(), ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            mAgent.onRestore(backupData, 0, newState);
        }
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertTrue(prefs.getBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, false));
        assertFalse(prefs.contains("junk"));
        assertFalse(prefs.contains(ChromeBackupAgentImpl.SIGNED_IN_ACCOUNT_ID_KEY));
        assertFalse(prefs.contains(SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT));
        verify(mPrefService, never()).setBoolean(eq(NATIVE_PREF_NOT_BACKED_UP), anyBoolean());
    }

    private void verifyRestoreFinishWithSignin() {
        // Verify that the restore is marked as completed.
        assertThat(
                ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.RESTORE_COMPLETED));

        // Verify that the account is not recorded to trigger the sign-in & sync flow later.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertFalse(prefs.contains(ChromePreferenceKeys.BACKUP_FLOW_SIGNIN_ACCOUNT_NAME));

        // Verify that sign-in without sync is triggered for the given account.
        verify(mSigninManager, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .signin(eq(mAccountInfo), anyInt(), any());

        if (mIsAccountManaged) {
            verify(mSigninManager).setUserAcceptedAccountManagement(true);
        } else {
            verify(mSigninManager, never()).setUserAcceptedAccountManagement(anyBoolean());
        }
    }

    private void verifySyncTypeBoolPrefsRestored(boolean isRestored) {
        for (Map.Entry<String, Boolean> entry : mNativeBoolPrefBackupValues.entrySet()) {
            if (isRestored) {
                verify(mPrefService, times(1)).setBoolean(entry.getKey(), entry.getValue());
            } else {
                verify(mPrefService, never()).setBoolean(eq(entry.getKey()), anyBoolean());
            }
        }
    }

    private void verifyAccountSettingsBackupRestored(boolean isRestored) {
        if (isRestored) {
            verify(mDictPrefBackupSerializerJniMock, times(1))
                    .setDict(
                            mPrefService,
                            SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT,
                            ACCOUNT_SETTINGS_PREF_VALUE);
        } else {
            verify(mDictPrefBackupSerializerJniMock, never())
                    .setDict(any(), eq(SyncPrefNames.SELECTED_TYPES_PER_ACCOUNT), anyString());
        }
    }

    private void verifyBoolPrefsMigratedToAccountSettings(boolean isMigrated) {
        if (isMigrated) {
            verify(mChromeBackupAgentJniMock, times(1))
                    .migrateGlobalDataTypePrefsToAccount(mPrefService, mAccountInfo.getGaiaId());
        } else {
            verify(mChromeBackupAgentJniMock, never())
                    .migrateGlobalDataTypePrefsToAccount(any(), anyString());
        }
    }

    private void verifyRestoreFinishWithSigninAndSync() {
        // Verify that the restore is marked as completed.
        assertThat(
                ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.RESTORE_COMPLETED));

        // Verify that the account is recorded to trigger the sign-in & sync flow later.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertTrue(prefs.contains(ChromePreferenceKeys.BACKUP_FLOW_SIGNIN_ACCOUNT_NAME));
        assertThat(
                prefs.getString(ChromePreferenceKeys.BACKUP_FLOW_SIGNIN_ACCOUNT_NAME, ""),
                equalTo(mAccountInfo.getEmail()));

        // Verify that the sign-in is not triggered immediately.
        verify(mSigninManager, never()).signin(any(CoreAccountInfo.class), anyInt(), any());
    }

    private void verifyRestoreFinishWithoutSignin() {
        // Verify that the status of the restore has been recorded.
        assertThat(
                ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.NOT_SIGNED_IN));

        // Verify that the sign-in is not triggered immediately.
        verify(mSigninManager, never()).signin(any(CoreAccountInfo.class), anyInt(), any());

        // Verify that the account is not recorded to trigger the sign-in & sync flow later.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertFalse(prefs.contains(ChromePreferenceKeys.BACKUP_FLOW_SIGNIN_ACCOUNT_NAME));
    }
}
