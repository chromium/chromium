// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.hasItem;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.backup.BackupDataInput;
import android.app.backup.BackupDataOutput;
import android.app.backup.BackupManager;
import android.content.SharedPreferences;
import android.os.ParcelFileDescriptor;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.init.AsyncInitTaskRunner;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.common.ContentProcessInfo;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;

/**
 * Unit tests for {@link org.chromium.chrome.browser.ChromeBackupAgent}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ChromeBackupAgentTest.BackupManagerShadow.class})
public class ChromeBackupAgentTest {
    @Rule
    public TemporaryFolder mTempDir = new TemporaryFolder();

    /**
     * Shadow to allow counting of dataChanged calls.
     */
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

    @Rule
    public JniMocker mocker = new JniMocker();
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();
    @Mock
    private ChromeBackupAgentImpl.Natives mChromeBackupAgentJniMock;
    @Mock
    private IdentityManager mIdentityManagerMock;
    @Mock
    private Profile mProfile;

    private ChromeBackupAgentImpl mAgent;
    private AsyncInitTaskRunner mTaskRunner;

    private final CoreAccountInfo mAccountInfo =
            CoreAccountInfo.createFromEmailAndGaiaId("user1", "gaia_id_user1");

    private static final String PREFERENCE_KEY_NOT_BACKED_UP = "not_backed_up";

    private void setUpTestPrefs(SharedPreferences prefs) {
        SharedPreferences.Editor editor = prefs.edit();
        // In production some of these prefs can't be present in SharedPreferences at the same time,
        // but ChromeBackupAgentImpl is agnostic to that. The focus of these tests is making sure
        // that all the allowlisted prefs are backed up, and none other.
        editor.putBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, true);
        editor.putBoolean(ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, false);
        editor.putBoolean(ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE, false);
        editor.putBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, false);

        editor.putBoolean(PREFERENCE_KEY_NOT_BACKED_UP, false);

        doReturn(mAccountInfo).when(mIdentityManagerMock).getPrimaryAccountInfo(anyInt());
        editor.apply();
    }

    @Before
    public void setUp() {
        // Create the agent to test; override fetching the task runner, and spy on the agent to
        // allow us to validate calls to these methods.
        mAgent = spy(new ChromeBackupAgentImpl() {
            @Override
            AsyncInitTaskRunner createAsyncInitTaskRunner(CountDownLatch latch) {
                latch.countDown();
                return mTaskRunner;
            }
        });

        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfile);
        mocker.mock(ChromeBackupAgentImplJni.TEST_HOOKS, mChromeBackupAgentJniMock);

        when(mChromeBackupAgentJniMock.getBoolBackupNames(mAgent))
                .thenReturn(new String[] {"pref1"});
        when(mChromeBackupAgentJniMock.getBoolBackupValues(mAgent))
                .thenReturn(new boolean[] {true});

        IdentityServicesProvider identityServicesProvider = mock(IdentityServicesProvider.class);
        IdentityServicesProvider.setInstanceForTests(identityServicesProvider);
        when(identityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.getPrimaryAccountInfo(ConsentLevel.SYNC)).thenReturn(null);

        // Mock initializing the browser
        doReturn(true).when(mAgent).initializeBrowser();

        // Mock the AsyncTaskRunner.
        mTaskRunner = mock(AsyncInitTaskRunner.class);

        ContentProcessInfo.setInChildProcess(false);
    }

    /**
     * Test method for {@link ChromeBackupAgent#onBackup} testing first backup
     *
     */
    @Test
    @SuppressWarnings("unchecked")
    public void testOnBackup_firstBackup() throws IOException, ClassNotFoundException {
        // Mock the backup data.
        BackupDataOutput backupData = mock(BackupDataOutput.class);

        // Set up some preferences to back up.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        setUpTestPrefs(prefs);

        File stateFile = mTempDir.newFile();
        try (ParcelFileDescriptor newState = ParcelFileDescriptor.open(
                     stateFile, ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Run the test function.
            mAgent.onBackup(null, backupData, newState);
        }

        // Check that the right things were written to the backup
        verify(backupData).writeEntityHeader("native.pref1", 1);
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault." + ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, 1);
        verify(backupData, times(2)).writeEntityData(new byte[] {1}, 1);
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault." + ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, 1);
        verify(backupData)
                .writeEntityHeader("AndroidDefault."
                                + ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE,
                        1);
        verify(backupData)
                .writeEntityHeader("AndroidDefault."
                                + ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER,
                        1);
        verify(backupData, times(3)).writeEntityData(new byte[] {0}, 1);
        byte[] unameBytes = ApiCompatibilityUtils.getBytesUtf8(mAccountInfo.getEmail());
        verify(backupData)
                .writeEntityHeader("AndroidDefault." + ChromeBackupAgentImpl.SIGNED_IN_ACCOUNT_KEY,
                        unameBytes.length);
        verify(backupData).writeEntityData(unameBytes, unameBytes.length);

        verify(backupData, times(0))
                .writeEntityHeader(eq("AndroidDefault." + PREFERENCE_KEY_NOT_BACKED_UP), anyInt());

        // Check that the state was saved correctly
        try (ObjectInputStream newStateStream =
                        new ObjectInputStream(new FileInputStream(stateFile))) {
            ArrayList<String> names = (ArrayList<String>) newStateStream.readObject();
            assertThat(names.size(), equalTo(6));
            assertThat(names, hasItem("native.pref1"));
            assertThat(names,
                    hasItem("AndroidDefault." + ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE));
            assertThat(names,
                    hasItem("AndroidDefault."
                            + ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED));
            assertThat(names,
                    hasItem("AndroidDefault."
                            + ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE));
            assertThat(names,
                    hasItem("AndroidDefault."
                            + ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER));
            assertThat(names,
                    hasItem("AndroidDefault." + ChromeBackupAgentImpl.SIGNED_IN_ACCOUNT_KEY));
            ArrayList<byte[]> values = (ArrayList<byte[]>) newStateStream.readObject();
            assertThat(values.size(), equalTo(6));
            assertThat(values, hasItem(unameBytes));
            assertThat(values, hasItem(new byte[] {0}));
            assertThat(values, hasItem(new byte[] {1}));

            // Make sure that there are no extra objects.
            assertThat(newStateStream.available(), equalTo(0));
        }
    }

    /**
     * Test method for {@link ChromeBackupAgent#onBackup} a second backup with the same data
     */
    @Test
    @SuppressWarnings("unchecked")
    public void testOnBackup_duplicateBackup()
            throws FileNotFoundException, IOException, ClassNotFoundException {
        // Mock the backup data.
        BackupDataOutput backupData = mock(BackupDataOutput.class);

        // Set up some preferences to back up.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        setUpTestPrefs(prefs);

        File stateFile1 = mTempDir.newFile();
        try (ParcelFileDescriptor newState = ParcelFileDescriptor.open(
                     stateFile1, ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a first backup.
            mAgent.onBackup(null, backupData, newState);
        }

        // Minimal check on first backup, this isn't the test here.
        verify(backupData, times(6)).writeEntityHeader(anyString(), anyInt());
        verify(backupData, times(6)).writeEntityData(any(byte[].class), anyInt());

        File stateFile2 = mTempDir.newFile();
        try (ParcelFileDescriptor oldState =
                        ParcelFileDescriptor.open(stateFile1, ParcelFileDescriptor.MODE_READ_ONLY);

                ParcelFileDescriptor newState = ParcelFileDescriptor.open(
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

    /**
     * Test method for {@link ChromeBackupAgent#onBackup} a second backup with different data
     */
    @Test
    @SuppressWarnings("unchecked")
    public void testOnBackup_dataChanged()
            throws FileNotFoundException, IOException, ClassNotFoundException {
        // Mock the backup data.
        BackupDataOutput backupData = mock(BackupDataOutput.class);

        // Set up some preferences to back up.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        setUpTestPrefs(prefs);

        // Create a state file.
        File stateFile1 = mTempDir.newFile();
        try (ParcelFileDescriptor newState = ParcelFileDescriptor.open(
                     stateFile1, ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a first backup.
            mAgent.onBackup(null, backupData, newState);
        }
        // Minimal check on first backup, this isn't the test here.
        verify(backupData, times(6)).writeEntityHeader(anyString(), anyInt());
        verify(backupData, times(6)).writeEntityData(any(byte[].class), anyInt());

        // Change some data.
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, true);
        editor.apply();
        reset(backupData);

        File stateFile2 = mTempDir.newFile();
        try (ParcelFileDescriptor oldState =
                        ParcelFileDescriptor.open(stateFile1, ParcelFileDescriptor.MODE_WRITE_ONLY);
                ParcelFileDescriptor newState = ParcelFileDescriptor.open(
                        stateFile2, ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a second backup.
            mAgent.onBackup(oldState, backupData, newState);
        }

        // Check that the second backup wrote something.
        verify(backupData, times(6)).writeEntityHeader(anyString(), anyInt());
        verify(backupData, times(6)).writeEntityData(any(byte[].class), anyInt());

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

    /**
     * Test method for {@link ChromeBackupAgent#onBackup} when browser startup fails
     */
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
                .putInt(ChromeBackupAgentImpl.BACKUP_FAILURE_COUNT,
                        ChromeBackupAgentImpl.MAX_BACKUP_FAILURES)
                .apply();
        mAgent.onBackup(null, backupData, mockState);
        assertThat(BackupManagerShadow.getDataChangedCalls(), equalTo(1));

        // Check that a successful backup resets the failure count
        doReturn(true).when(mAgent).initializeBrowser();
        // A successful backup needs a real state file, or lots more mocking.
        try (ParcelFileDescriptor newState = ParcelFileDescriptor.open(
                     mTempDir.newFile(), ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            mAgent.onBackup(null, backupData, newState);
        }
        assertThat(prefs.getInt(ChromeBackupAgentImpl.BACKUP_FAILURE_COUNT, 0), equalTo(0));
    }

    private BackupDataInput createMockBackupData() throws IOException {
        // Mock the backup data
        BackupDataInput backupData = mock(BackupDataInput.class);

        final String[] keys = {"native.pref1", "native.pref2",
                "AndroidDefault." + ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE,
                "AndroidDefault.junk",
                "AndroidDefault." + ChromeBackupAgentImpl.SIGNED_IN_ACCOUNT_KEY};
        byte[] unameBytes = ApiCompatibilityUtils.getBytesUtf8(mAccountInfo.getEmail());
        final byte[][] values = {{0}, {1}, {1}, {23, 42}, unameBytes};
        when(backupData.getKey()).thenAnswer(new Answer<String>() {
            private int mPos;

            @Override
            public String answer(InvocationOnMock invocation) {
                return keys[mPos++];
            }
        });

        when(backupData.getDataSize()).thenAnswer(new Answer<Integer>() {
            private int mPos;

            @Override
            public Integer answer(InvocationOnMock invocation) {
                return values[mPos++].length;
            }
        });

        when(backupData.readEntityData(any(byte[].class), anyInt(), anyInt()))
                .thenAnswer(new Answer<Integer>() {
                    private int mPos;

                    @Override
                    public Integer answer(InvocationOnMock invocation) {
                        byte[] buffer = invocation.getArgument(0);
                        for (int i = 0; i < values[mPos].length; i++) {
                            buffer[i] = values[mPos][i];
                        }
                        return values[mPos++].length;
                    }
                });

        when(backupData.readNextHeader()).thenAnswer(new Answer<Boolean>() {
            private int mPos;

            @Override
            public Boolean answer(InvocationOnMock invocation) {
                return mPos++ < 5;
            }
        });
        return backupData;
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore}.
     *
     * @throws IOException
     */
    @Test
    public void testOnRestore_normal() throws IOException {
        BackupDataInput backupData = createMockBackupData();
        mAccountManagerTestRule.addAccount(mAccountInfo.getEmail());

        try (ParcelFileDescriptor newState = ParcelFileDescriptor.open(
                     mTempDir.newFile(), ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a restore.
            mAgent.onRestore(backupData, 0, newState);
        }
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertTrue(prefs.getBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, false));
        assertFalse(prefs.contains("junk"));
        verify(mChromeBackupAgentJniMock)
                .setBoolBackupPrefs(
                        mAgent, new String[] {"pref1", "pref2"}, new boolean[] {false, true});
        verify(mTaskRunner)
                .startBackgroundTasks(
                        false /* allocateChildConnection */, true /* initVariationSeed */);

        // Test that the status of the restore has been recorded.
        assertThat(ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.RESTORE_COMPLETED));
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore} for a user that doesn't exist on the
     * device
     *
     * @throws IOException
     */
    @Test
    public void testOnRestore_badUser() throws IOException {
        BackupDataInput backupData = createMockBackupData();
        try (ParcelFileDescriptor newState = ParcelFileDescriptor.open(
                     mTempDir.newFile(), ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a restore.
            mAgent.onRestore(backupData, 0, newState);
        }
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertFalse(prefs.contains(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE));
        verify(mChromeBackupAgentJniMock, never())
                .setBoolBackupPrefs(eq(mAgent), any(String[].class), any(boolean[].class));
        verify(mTaskRunner)
                .startBackgroundTasks(
                        false /* allocateChildConnection */, true /* initVariationSeed */);

        // Test that the status of the restore has been recorded.
        assertThat(ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.NOT_SIGNED_IN));
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore} for browser startup failure
     *
     * @throws IOException
     */
    @Test
    public void testOnRestore_browserStartupFails() throws IOException {
        BackupDataInput backupData = createMockBackupData();
        doReturn(false).when(mAgent).initializeBrowser();

        try (ParcelFileDescriptor newState = ParcelFileDescriptor.open(
                     mTempDir.newFile(), ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a restore.
            mAgent.onRestore(backupData, 0, newState);
        }
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertFalse(prefs.contains(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE));

        // Test that the status of the restore has been recorded.
        assertThat(ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.BROWSER_STARTUP_FAILED));
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore} for browser startup failure
     *
     * @throws IOException
     */
    @Test
    public void testOnRestore_afterFirstRun() throws IOException {
        BackupDataInput backupData = createMockBackupData();
        FirstRunStatus.setFirstRunFlowComplete(true);

        try (ParcelFileDescriptor newState = ParcelFileDescriptor.open(
                     mTempDir.newFile(), ParcelFileDescriptor.MODE_WRITE_ONLY)) {
            // Do a restore.
            mAgent.onRestore(backupData, 0, newState);
        }
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertTrue(prefs.contains(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE));

        // Test that the status of the restore has been recorded.
        assertThat(ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.RESTORE_AFTER_FIRST_RUN));
    }

    /**
     * Test of {@link ChromeBackupAgent#getRestoreStatus}
     */
    @Test
    public void testGetRestoreStatus() {
        // Test default value
        assertThat(ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.NO_RESTORE));

        // Test that the value can be changed
        ChromeBackupAgentImpl.setRestoreStatus(
                ChromeBackupAgentImpl.RestoreStatus.RESTORE_AFTER_FIRST_RUN);
        assertThat(ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.RESTORE_AFTER_FIRST_RUN));

        // Prove that the value equalTo held in the app preferences (and not, for example, in a
        // static).
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        assertThat(ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.NO_RESTORE));

        // Test that ChromeBackupAgentImpl.setRestoreStatus really looks at the argument.
        ChromeBackupAgentImpl.setRestoreStatus(
                ChromeBackupAgentImpl.RestoreStatus.BROWSER_STARTUP_FAILED);
        assertThat(ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.BROWSER_STARTUP_FAILED));

        // Test the remaining values are implemented
        ChromeBackupAgentImpl.setRestoreStatus(ChromeBackupAgentImpl.RestoreStatus.NOT_SIGNED_IN);
        assertThat(ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.NOT_SIGNED_IN));
        ChromeBackupAgentImpl.setRestoreStatus(
                ChromeBackupAgentImpl.RestoreStatus.RESTORE_COMPLETED);
        assertThat(ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.RESTORE_COMPLETED));
        ChromeBackupAgentImpl.setRestoreStatus(
                ChromeBackupAgentImpl.RestoreStatus.RESTORE_STATUS_RECORDED);
        assertThat(ChromeBackupAgentImpl.getRestoreStatus(),
                equalTo(ChromeBackupAgentImpl.RestoreStatus.RESTORE_STATUS_RECORDED));
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
}
