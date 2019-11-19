// Copyright 2016 The Chromium Authors. All rights reserved.
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
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.backup.BackupDataInput;
import android.app.backup.BackupDataOutput;
import android.app.backup.BackupManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.ParcelFileDescriptor;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
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
import org.chromium.base.PathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.init.AsyncInitTaskRunner;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.components.signin.ChromeSigninController;
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
    @Mock
    private ChromeBackupAgent.Natives mChromeBackupAgentJniMock;

    private ChromeBackupAgent mAgent;
    private AsyncInitTaskRunner mTaskRunner;

    private void setUpTestPrefs(SharedPreferences prefs) {
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean(FirstRunStatus.FIRST_RUN_FLOW_COMPLETE, true);
        editor.putBoolean(FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_SETUP, false);
        editor.putString(ChromeSigninController.SIGNED_IN_ACCOUNT_KEY, "user1");
        editor.apply();
    }

    @Before
    public void setUp() {
        // Create the agent to test; override fetching the task runner, and spy on the agent to
        // allow us to validate calls to these methods.
        mAgent = spy(new ChromeBackupAgent() {
            @Override
            AsyncInitTaskRunner createAsyncInitTaskRunner(CountDownLatch latch) {
                latch.countDown();
                return mTaskRunner;
            }
        });

        MockitoAnnotations.initMocks(this);
        mocker.mock(ChromeBackupAgentJni.TEST_HOOKS, mChromeBackupAgentJniMock);

        when(mChromeBackupAgentJniMock.getBoolBackupNames(mAgent))
                .thenReturn(new String[] {"pref1"});
        when(mChromeBackupAgentJniMock.getBoolBackupValues(mAgent))
                .thenReturn(new boolean[] {true});

        // Mock initializing the browser
        doReturn(true).when(mAgent).initializeBrowser(any(Context.class));

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

        // Create a state file.
        File stateFile1 = File.createTempFile("Test", "");
        ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(stateFile1, ParcelFileDescriptor.parseMode("w"));

        // Set up some preferences to back up.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        setUpTestPrefs(prefs);

        // Run the test function.
        mAgent.onBackup(null, backupData, newState);

        // Check that the right things were written to the backup
        verify(backupData).writeEntityHeader("native.pref1", 1);
        verify(backupData)
                .writeEntityHeader("AndroidDefault." + FirstRunStatus.FIRST_RUN_FLOW_COMPLETE, 1);
        verify(backupData, times(2)).writeEntityData(new byte[] {1}, 1);
        verify(backupData)
                .writeEntityHeader(
                        "AndroidDefault." + FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_SETUP, 1);
        verify(backupData).writeEntityData(new byte[] {0}, 1);
        byte[] unameBytes = ApiCompatibilityUtils.getBytesUtf8("user1");
        verify(backupData)
                .writeEntityHeader("AndroidDefault." + ChromeSigninController.SIGNED_IN_ACCOUNT_KEY,
                        unameBytes.length);
        verify(backupData).writeEntityData(unameBytes, unameBytes.length);

        newState.close();

        // Check that the state was saved correctly
        ObjectInputStream newStateStream = new ObjectInputStream(new FileInputStream(stateFile1));
        ArrayList<String> names = (ArrayList<String>) newStateStream.readObject();
        assertThat(names.size(), equalTo(4));
        assertThat(names, hasItem("native.pref1"));
        assertThat(names, hasItem("AndroidDefault." + FirstRunStatus.FIRST_RUN_FLOW_COMPLETE));
        assertThat(names,
                hasItem("AndroidDefault." + FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_SETUP));
        assertThat(
                names, hasItem("AndroidDefault." + ChromeSigninController.SIGNED_IN_ACCOUNT_KEY));
        ArrayList<byte[]> values = (ArrayList<byte[]>) newStateStream.readObject();
        assertThat(values.size(), equalTo(4));
        assertThat(values, hasItem(unameBytes));
        assertThat(values, hasItem(new byte[] {0}));
        assertThat(values, hasItem(new byte[] {1}));

        // Make sure that there are no extra objects.
        assertThat(newStateStream.available(), equalTo(0));

        // Tidy up.
        newStateStream.close();
        stateFile1.delete();
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

        // Create a state file.
        File stateFile1 = File.createTempFile("Test", "");
        ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(stateFile1, ParcelFileDescriptor.parseMode("w"));

        // Set up some preferences to back up.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        setUpTestPrefs(prefs);

        // Do a first backup.
        mAgent.onBackup(null, backupData, newState);

        // Minimal check on first backup, this isn't the test here.
        verify(backupData, times(4)).writeEntityHeader(anyString(), anyInt());
        verify(backupData, times(4)).writeEntityData(any(byte[].class), anyInt());

        newState.close();

        ParcelFileDescriptor oldState =
                ParcelFileDescriptor.open(stateFile1, ParcelFileDescriptor.parseMode("r"));
        File stateFile2 = File.createTempFile("Test", "");
        newState = ParcelFileDescriptor.open(stateFile2, ParcelFileDescriptor.parseMode("w"));

        // Try a second backup without changing any data
        mAgent.onBackup(oldState, backupData, newState);

        // Check that the second backup didn't write anything.
        verifyNoMoreInteractions(backupData);

        oldState.close();
        newState.close();

        // The two state files should contain identical data.
        ObjectInputStream oldStateStream = new ObjectInputStream(new FileInputStream(stateFile1));
        ArrayList<String> oldNames = (ArrayList<String>) oldStateStream.readObject();
        ArrayList<byte[]> oldValues = (ArrayList<byte[]>) oldStateStream.readObject();
        ObjectInputStream newStateStream = new ObjectInputStream(new FileInputStream(stateFile2));
        ArrayList<String> newNames = (ArrayList<String>) newStateStream.readObject();
        ArrayList<byte[]> newValues = (ArrayList<byte[]>) newStateStream.readObject();
        assertThat(newNames, equalTo(oldNames));
        assertTrue(Arrays.deepEquals(newValues.toArray(), oldValues.toArray()));
        assertThat(newStateStream.available(), equalTo(0));

        // Tidy up.
        oldStateStream.close();
        newStateStream.close();
        stateFile1.delete();
        stateFile2.delete();
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

        // Create a state file.
        File stateFile1 = File.createTempFile("Test", "");
        ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(stateFile1, ParcelFileDescriptor.parseMode("w"));

        // Set up some preferences to back up.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        setUpTestPrefs(prefs);

        // Do a first backup.
        mAgent.onBackup(null, backupData, newState);

        // Minimal check on first backup, this isn't the test here.
        verify(backupData, times(4)).writeEntityHeader(anyString(), anyInt());
        verify(backupData, times(4)).writeEntityData(any(byte[].class), anyInt());

        newState.close();

        ParcelFileDescriptor oldState =
                ParcelFileDescriptor.open(stateFile1, ParcelFileDescriptor.parseMode("r"));
        File stateFile2 = File.createTempFile("Test", "");
        newState = ParcelFileDescriptor.open(stateFile2, ParcelFileDescriptor.parseMode("w"));

        // Change some data.
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean(FirstRunSignInProcessor.FIRST_RUN_FLOW_SIGNIN_SETUP, true);
        editor.apply();

        // Do a second backup.
        mAgent.onBackup(oldState, backupData, newState);

        // Check that the second backup wrote something.
        verify(backupData, times(8)).writeEntityHeader(anyString(), anyInt());
        verify(backupData, times(8)).writeEntityData(any(byte[].class), anyInt());

        oldState.close();
        newState.close();

        // the two state files should contain different data (although the names are unchanged).
        ObjectInputStream oldStateStream = new ObjectInputStream(new FileInputStream(stateFile1));
        ArrayList<String> oldNames = (ArrayList<String>) oldStateStream.readObject();
        ArrayList<byte[]> oldValues = (ArrayList<byte[]>) oldStateStream.readObject();
        ObjectInputStream newStateStream = new ObjectInputStream(new FileInputStream(stateFile2));
        ArrayList<String> newNames = (ArrayList<String>) newStateStream.readObject();
        ArrayList<byte[]> newValues = (ArrayList<byte[]>) newStateStream.readObject();
        assertThat(newNames, equalTo(oldNames));
        assertFalse(Arrays.deepEquals(newValues.toArray(), oldValues.toArray()));
        assertThat(newStateStream.available(), equalTo(0));

        // Tidy up.
        oldStateStream.close();
        newStateStream.close();
        stateFile1.delete();
        stateFile2.delete();
    }

    /**
     * Test method for {@link ChromeBackupAgent#onBackup} when browser startup fails
     */
    @Test
    public void testOnBackup_browserStartupFails() throws IOException {
        BackupDataOutput backupData = mock(BackupDataOutput.class);
        ParcelFileDescriptor mockState = mock(ParcelFileDescriptor.class);

        doReturn(false).when(mAgent).initializeBrowser(any(Context.class));

        BackupManagerShadow.clearDataChangedCalls();
        mAgent.onBackup(null, backupData, mockState);
        assertThat(BackupManagerShadow.getDataChangedCalls(), equalTo(1));
        verifyNoMoreInteractions(backupData);
        verifyNoMoreInteractions(mockState);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertThat(prefs.getInt(ChromeBackupAgent.BACKUP_FAILURE_COUNT, 0), equalTo(1));

        // Check that the backup agent gives up retrying after too many failures
        prefs.edit()
                .putInt(ChromeBackupAgent.BACKUP_FAILURE_COUNT,
                        ChromeBackupAgent.MAX_BACKUP_FAILURES)
                .apply();
        mAgent.onBackup(null, backupData, mockState);
        assertThat(BackupManagerShadow.getDataChangedCalls(), equalTo(1));

        // Check that a successful backup resets the failure count
        doReturn(true).when(mAgent).initializeBrowser(any(Context.class));
        // A successful backup needs a real state file, or lots more mocking.
        File stateFile = File.createTempFile("Test", "");
        ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(stateFile, ParcelFileDescriptor.parseMode("w"));

        mAgent.onBackup(null, backupData, newState);
        assertThat(prefs.getInt(ChromeBackupAgent.BACKUP_FAILURE_COUNT, 0), equalTo(0));
    }

    private BackupDataInput createMockBackupData() throws IOException {
        // Mock the backup data
        BackupDataInput backupData = mock(BackupDataInput.class);

        final String[] keys = {"native.pref1", "native.pref2",
                "AndroidDefault." + FirstRunStatus.FIRST_RUN_FLOW_COMPLETE, "AndroidDefault.junk",
                "AndroidDefault." + ChromeSigninController.SIGNED_IN_ACCOUNT_KEY};
        byte[] unameBytes = ApiCompatibilityUtils.getBytesUtf8("user1");
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
        // Create a state file.
        File stateFile = File.createTempFile("Test", "");
        ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(stateFile, ParcelFileDescriptor.parseMode("w"));

        BackupDataInput backupData = createMockBackupData();
        doReturn(true).when(mAgent).accountExistsOnDevice(any(String.class));

        // Do a restore.
        mAgent.onRestore(backupData, 0, newState);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertTrue(prefs.getBoolean(FirstRunStatus.FIRST_RUN_FLOW_COMPLETE, false));
        assertFalse(prefs.contains("junk"));
        verify(mChromeBackupAgentJniMock)
                .setBoolBackupPrefs(
                        mAgent, new String[] {"pref1", "pref2"}, new boolean[] {false, true});
        verify(mTaskRunner)
                .startBackgroundTasks(
                        false /* allocateChildConnection */, true /* initVariationSeed */);

        // Test that the status of the restore has been recorded.
        assertThat(ChromeBackupAgent.getRestoreStatus(),
                equalTo(ChromeBackupAgent.RestoreStatus.RESTORE_COMPLETED));

        // The test mocks out everything that forces the AsyncTask used by PathUtils setup to
        // complete. If it isn't completed before the test exits Robolectric crashes with a null
        // pointer exception (although the test passes). Force it to complete by getting some data.
        PathUtils.getDataDirectory();
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore} for a user that doesn't exist on the
     * device
     *
     * @throws IOException
     */
    @Test
    public void testOnRestore_badUser() throws IOException {
        // Create a state file.
        File stateFile = File.createTempFile("Test", "");
        ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(stateFile, ParcelFileDescriptor.parseMode("w"));

        BackupDataInput backupData = createMockBackupData();
        doReturn(false).when(mAgent).accountExistsOnDevice(any(String.class));

        // Do a restore.
        mAgent.onRestore(backupData, 0, newState);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertFalse(prefs.contains(FirstRunStatus.FIRST_RUN_FLOW_COMPLETE));
        verify(mChromeBackupAgentJniMock, never())
                .setBoolBackupPrefs(eq(mAgent), any(String[].class), any(boolean[].class));
        verify(mTaskRunner)
                .startBackgroundTasks(
                        false /* allocateChildConnection */, true /* initVariationSeed */);

        // Test that the status of the restore has been recorded.
        assertThat(ChromeBackupAgent.getRestoreStatus(),
                equalTo(ChromeBackupAgent.RestoreStatus.NOT_SIGNED_IN));

        // The test mocks out everything that forces the AsyncTask used by PathUtils setup to
        // complete. If it isn't completed before the test exits Robolectric crashes with a null
        // pointer exception (although the test passes). Force it to complete by getting some data.
        PathUtils.getDataDirectory();
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore} for browser startup failure
     *
     * @throws IOException
     */
    @Test
    public void testOnRestore_browserStartupFails() throws IOException {
        // Create a state file.
        File stateFile = File.createTempFile("Test", "");
        ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(stateFile, ParcelFileDescriptor.parseMode("w"));

        BackupDataInput backupData = createMockBackupData();
        doReturn(false).when(mAgent).initializeBrowser(any(Context.class));

        // Do a restore.
        mAgent.onRestore(backupData, 0, newState);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertFalse(prefs.contains(FirstRunStatus.FIRST_RUN_FLOW_COMPLETE));

        // Test that the status of the restore has been recorded.
        assertThat(ChromeBackupAgent.getRestoreStatus(),
                equalTo(ChromeBackupAgent.RestoreStatus.BROWSER_STARTUP_FAILED));

        // The test mocks out everything that forces the AsyncTask used by PathUtils setup to
        // complete. If it isn't completed before the test exits Robolectric crashes with a null
        // pointer exception (although the test passes). Force it to complete by getting some data.
        PathUtils.getDataDirectory();
    }

    /**
     * Test method for {@link ChromeBackupAgent#onRestore} for browser startup failure
     *
     * @throws IOException
     */
    @Test
    public void testOnRestore_afterFirstRun() throws IOException {
        // Create a state file.
        File stateFile = File.createTempFile("Test", "");
        ParcelFileDescriptor newState =
                ParcelFileDescriptor.open(stateFile, ParcelFileDescriptor.parseMode("w"));

        BackupDataInput backupData = createMockBackupData();
        FirstRunStatus.setFirstRunFlowComplete(true);

        // Do a restore.
        mAgent.onRestore(backupData, 0, newState);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertTrue(prefs.contains(FirstRunStatus.FIRST_RUN_FLOW_COMPLETE));

        // Test that the status of the restore has been recorded.
        assertThat(ChromeBackupAgent.getRestoreStatus(),
                equalTo(ChromeBackupAgent.RestoreStatus.RESTORE_AFTER_FIRST_RUN));

        // The test mocks out everything that forces the AsyncTask used by PathUtils setup to
        // complete. If it isn't completed before the test exits Robolectric crashes with a null
        // pointer exception (although the test passes). Force it to complete by getting some data.
        PathUtils.getDataDirectory();
    }

    /**
     * Test of {@link ChromeBackupAgent#getRestoreStatus}
     */
    @Test
    public void testGetRestoreStatus() {
        // Test default value
        assertThat(ChromeBackupAgent.getRestoreStatus(),
                equalTo(ChromeBackupAgent.RestoreStatus.NO_RESTORE));

        // Test that the value can be changed
        ChromeBackupAgent.setRestoreStatus(ChromeBackupAgent.RestoreStatus.RESTORE_AFTER_FIRST_RUN);
        assertThat(ChromeBackupAgent.getRestoreStatus(),
                equalTo(ChromeBackupAgent.RestoreStatus.RESTORE_AFTER_FIRST_RUN));

        // Prove that the value equalTo held in the app preferences (and not, for example, in a
        // static).
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        assertThat(ChromeBackupAgent.getRestoreStatus(),
                equalTo(ChromeBackupAgent.RestoreStatus.NO_RESTORE));

        // Test that ChromeBackupAgent.setRestoreStatus really looks at the argument.
        ChromeBackupAgent.setRestoreStatus(ChromeBackupAgent.RestoreStatus.BROWSER_STARTUP_FAILED);
        assertThat(ChromeBackupAgent.getRestoreStatus(),
                equalTo(ChromeBackupAgent.RestoreStatus.BROWSER_STARTUP_FAILED));

        // Test the remaining values are implemented
        ChromeBackupAgent.setRestoreStatus(ChromeBackupAgent.RestoreStatus.NOT_SIGNED_IN);
        assertThat(ChromeBackupAgent.getRestoreStatus(),
                equalTo(ChromeBackupAgent.RestoreStatus.NOT_SIGNED_IN));
        ChromeBackupAgent.setRestoreStatus(ChromeBackupAgent.RestoreStatus.RESTORE_COMPLETED);
        assertThat(ChromeBackupAgent.getRestoreStatus(),
                equalTo(ChromeBackupAgent.RestoreStatus.RESTORE_COMPLETED));
        ChromeBackupAgent.setRestoreStatus(ChromeBackupAgent.RestoreStatus.RESTORE_STATUS_RECORDED);
        assertThat(ChromeBackupAgent.getRestoreStatus(),
                equalTo(ChromeBackupAgent.RestoreStatus.RESTORE_STATUS_RECORDED));
    }

    /**
     * Test normal browser startup. This is not tested by the other tests, since, until recently,
     * it was not possible to mock ChromeBrowserInitializer, so initializeBrowser is mocked.
     *
     * TODO (aberent) remove mocking of initializeBrowser in the other tests.
     */
    @Test
    public void testInitializeBrowser_normal() {
        ChromeBackupAgent agent = new ChromeBackupAgent();
        ChromeBrowserInitializer initializer = mock(ChromeBrowserInitializer.class);
        ChromeBrowserInitializer.setForTesting(initializer);
        assertTrue(agent.initializeBrowser(ContextUtils.getApplicationContext()));
    }

    /**
     * Test that browser startup fails when in a child process. This is important because of
     * https://crbug.com/718166
     */
    @Test
    public void testInitializeBrowser_childProcess() {
        ContentProcessInfo.setInChildProcess(true);
        ChromeBackupAgent agent = new ChromeBackupAgent();
        ChromeBrowserInitializer initializer = mock(ChromeBrowserInitializer.class);
        ChromeBrowserInitializer.setForTesting(initializer);
        assertFalse(agent.initializeBrowser(ContextUtils.getApplicationContext()));
        verifyNoMoreInteractions(initializer);
    }
}
