// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.StreamUtil;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.ChromeRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.CustomTabProfileType;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelInternal;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelJniBridge;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabPersistenceFileInfo;
import org.chromium.chrome.browser.tabmodel.TabPersistenceFileInfo.TabStateFileInfo;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.TabModelMetadata;
import org.chromium.chrome.browser.tabpersistence.TabMetadataFileManager.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link CustomTabAppTabModelOrchestrator}. */
@RunWith(ChromeRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE,
    ChromeFeatureList.ANDROID_TAB_DECLUTTER_DEDUPE_TAB_IDS_KILL_SWITCH
})
public class CustomTabAppTabModelOrchestratorUnitTest {
    private static final String CLIENT_PACKAGE = "com.google.android.googlequicksearchbox";
    private static final String ACCOUNT_A = "account_a@example.com";
    private static final String ACCOUNT_B = "account_b@example.com";
    private static final int TASK_ID = 77;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public final TemporaryFolder mTempFolder = new TemporaryFolder();

    @Mock private CustomTabTabPersistencePolicy mPolicy1;
    @Mock private CustomTabTabPersistencePolicy mPolicy2;
    @Mock private CustomTabTabPersistencePolicy mPolicyAccountB;
    @Mock private CustomTabActivityNavigationController mNavigationController1;
    @Mock private ActivityWindowAndroid mWindowAndroid;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private CustomTabDelegateFactory mDelegateFactory;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private CipherFactory mCipherFactory;

    private TestCustomTabActivity mActivity1;
    private TestCustomTabActivity mActivity2;
    private TestCustomTabActivity mActivityAccountB;
    private OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier;
    private File mBaseStateDir;

    private static class TestCustomTabActivity extends CustomTabActivity {
        private final int mTaskId;
        private final CustomTabTabPersistencePolicy mPolicy;
        private final CustomTabActivityNavigationController mNavController;

        TestCustomTabActivity(
                int taskId,
                CustomTabTabPersistencePolicy policy,
                CustomTabActivityNavigationController navController) {
            mTaskId = taskId;
            mPolicy = policy;
            mNavController = navController;
        }

        @Override
        public int getTaskId() {
            return mTaskId;
        }

        @Override
        public CustomTabTabPersistencePolicy getCustomTabTabPersistencePolicy() {
            return mPolicy;
        }

        @Override
        public CustomTabActivityNavigationController getCustomTabActivityNavigationController() {
            return mNavController;
        }
    }

    @Before
    public void setUp() throws Exception {
        mBaseStateDir = mTempFolder.newFolder("tabs");
        TabStateDirectory.setBaseStateDirectoryForTests(mBaseStateDir);

        mProfileProviderSupplier = new OneshotSupplierImpl<>();
        mProfileProviderSupplier.set(mProfileProvider);

        mActivity1 = new TestCustomTabActivity(TASK_ID, mPolicy1, mNavigationController1);
        mActivity2 =
                new TestCustomTabActivity(
                        TASK_ID, mPolicy2, mock(CustomTabActivityNavigationController.class));
        mActivityAccountB =
                new TestCustomTabActivity(
                        TASK_ID,
                        mPolicyAccountB,
                        mock(CustomTabActivityNavigationController.class));
    }

    private CustomTabActivityTabFactory createTabFactory(
            Activity activity, CustomTabTabPersistencePolicy policy, String metadataFileName) {
        when(policy.getMetadataFileName()).thenReturn(metadataFileName);
        when(policy.getOrCreateStateDirectory())
                .thenAnswer(
                        inv ->
                                TabStateDirectory.getOrCreateCustomTabAppStateDirectory(
                                        CLIENT_PACKAGE, ACCOUNT_A));
        when(mIntentDataProvider.getCustomTabMode()).thenReturn(CustomTabProfileType.REGULAR);

        CustomTabActivityTabFactory factory =
                new CustomTabActivityTabFactory(
                        activity,
                        policy,
                        mWindowAndroid,
                        mProfileProviderSupplier,
                        mDelegateFactory,
                        mIntentDataProvider,
                        mTabCreatorManager,
                        () -> null,
                        () -> null,
                        mCipherFactory);
        factory.setActivityType(ActivityType.CUSTOM_TAB);
        return factory;
    }

    private static void publishSelectorToActivity(
            TestCustomTabActivity activity, TabModelSelector selector) {
        ((SettableMonotonicObservableSupplier<TabModelSelector>)
                        activity.getTabModelSelectorSupplier())
                .set(selector);
    }

    private static void writeMetadataFile(File file, int[] tabIds, String[] urls) {
        writeMetadataFile(file, tabIds, urls, new int[0], new String[0]);
    }

    private static void writeMetadataFile(
            File file, int[] tabIds, String[] urls, int[] incognitoTabIds, String[] incognitoUrls) {
        TabModelMetadata normalMetadata =
                new TabModelMetadata(tabIds.length == 0 ? TabModel.INVALID_TAB_INDEX : 0);
        for (int i = 0; i < tabIds.length; i++) {
            normalMetadata.ids.add(tabIds[i]);
            normalMetadata.urls.add(urls[i]);
        }
        TabModelMetadata incognitoMetadata =
                new TabModelMetadata(incognitoTabIds.length == 0 ? TabModel.INVALID_TAB_INDEX : 0);
        for (int i = 0; i < incognitoTabIds.length; i++) {
            incognitoMetadata.ids.add(incognitoTabIds[i]);
            incognitoMetadata.urls.add(incognitoUrls[i]);
        }
        TabMetadataFileManager.saveListToFile(
                file, new TabModelSelectorMetadata(normalMetadata, incognitoMetadata));
    }

    private static List<Integer> readTabIdsFromMetadataFile(File file) throws Exception {
        List<Integer> ids = new ArrayList<>();
        DataInputStream stream = null;
        try {
            stream = new DataInputStream(new BufferedInputStream(new FileInputStream(file)));
            TabMetadataFileManager.readSavedMetadataFile(
                    stream, (_, id, _, _, _, _) -> ids.add(id), /* tabIds= */ null);
        } finally {
            StreamUtil.closeQuietly(stream);
        }
        return ids;
    }

    @Test
    public void testPerActivityOrchestratorsAndSharedAppSingletonForSameAppAndAccount() {
        CustomTabActivityTabFactory factory1 =
                createTabFactory(mActivity1, mPolicy1, "tab_state_active_77_uuid1");
        CustomTabActivityTabFactory factory2 =
                createTabFactory(mActivity2, mPolicy2, "tab_state_active_77_uuid2");

        TabModelOrchestrator activityOrch1 = factory1.createTabModelOrchestrator();
        TabModelOrchestrator activityOrch2 = factory2.createTabModelOrchestrator();
        mActivity1.setTabModelOrchestratorForTesting(activityOrch1);
        mActivity2.setTabModelOrchestratorForTesting(activityOrch2);

        // Each CustomTabActivity owns its own distinct CustomTabsTabModelOrchestrator.
        assertNotSame(activityOrch1, activityOrch2);

        // Both activities share the same singleton CustomTabAppTabModelOrchestrator.
        CustomTabAppTabModelOrchestrator appOrch1 =
                CustomTabAppTabModelOrchestrator.getOrCreate(mActivity1, CLIENT_PACKAGE, ACCOUNT_A);
        CustomTabAppTabModelOrchestrator appOrch2 =
                CustomTabAppTabModelOrchestrator.getOrCreate(mActivity2, CLIENT_PACKAGE, ACCOUNT_A);
        assertSame(appOrch1, appOrch2);
        assertEquals(CLIENT_PACKAGE, appOrch1.getClientPackageName());
        assertEquals(ACCOUNT_A, appOrch1.getAccountId());
        assertEquals(2, appOrch1.getActiveActivityCountForTesting());
        assertTrue(appOrch1.getActivityTabModelSelectors().isEmpty());

        factory1.createTabModels();
        factory2.createTabModels();
        TabModelSelector selector1 = activityOrch1.getTabModelSelector();
        TabModelSelector selector2 = activityOrch2.getTabModelSelector();
        assertNotNull(selector1);
        assertNotNull(selector2);
        assertNotSame(selector1, selector2);
        publishSelectorToActivity(mActivity1, selector1);
        publishSelectorToActivity(mActivity2, selector2);
        assertEquals(List.of(selector1, selector2), appOrch1.getActivityTabModelSelectors());

        // Destroying the first activity releases its reference without tearing down the shared
        // app orchestrator while the second activity is still alive.
        appOrch1.releaseActivity(mActivity1);
        factory1.destroyTabModelOrchestrator();
        assertFalse(appOrch1.isDestroyedForTesting());
        assertEquals(1, appOrch1.getActiveActivityCountForTesting());
        assertEquals(List.of(selector2), appOrch1.getActivityTabModelSelectors());
        assertSame(
                appOrch1,
                CustomTabAppTabModelOrchestrator.getInstanceForPackageForTesting(CLIENT_PACKAGE));

        // Destroying the last activity destroys and unregisters the app orchestrator.
        appOrch1.releaseActivity(mActivity2);
        factory2.destroyTabModelOrchestrator();
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(appOrch1.isDestroyedForTesting());
        assertEquals(0, appOrch1.getActiveActivityCountForTesting());
        assertTrue(appOrch1.getActivityTabModelSelectors().isEmpty());
        assertNull(
                CustomTabAppTabModelOrchestrator.getInstanceForPackageForTesting(CLIENT_PACKAGE));
    }

    @Test
    public void testMergesRetiredTabStateFilesIntoTabStateAppOnInstantiation() throws Exception {
        File stateDir =
                TabStateDirectory.getOrCreateCustomTabAppStateDirectory(CLIENT_PACKAGE, ACCOUNT_A);

        // Pre-populate tab_state_app with tab 100.
        File appMetadataFile =
                new File(stateDir, CustomTabTabPersistencePolicy.APP_METADATA_FILE_NAME);
        writeMetadataFile(appMetadataFile, new int[] {100}, new String[] {"https://example.com/0"});

        // Create two retired metadata files and one active metadata file.
        File retiredFile1 = new File(stateDir, "tab_state_retired_77_uuidA_1000");
        writeMetadataFile(
                retiredFile1,
                new int[] {101, 102},
                new String[] {"https://example.com/1", "https://example.com/2"});

        File retiredFile2 = new File(stateDir, "tab_state_retired_77_uuidB_2000");
        writeMetadataFile(
                retiredFile2,
                new int[] {103},
                new String[] {"https://example.com/3"},
                new int[] {999},
                new String[] {"https://example.com/incognito"});

        File activeFile = new File(stateDir, "tab_state_active_77_uuidLive");
        writeMetadataFile(activeFile, new int[] {104}, new String[] {"https://example.com/live"});

        // Instantiate CustomTabAppTabModelOrchestrator.
        CustomTabAppTabModelOrchestrator.getOrCreate(mActivity1, CLIENT_PACKAGE, ACCOUNT_A);
        RobolectricUtil.runAllBackgroundAndUi();

        // Retired files should be merged into tab_state_app (excluding incognito tabs) and deleted;
        // active file untouched.
        assertFalse(retiredFile1.exists());
        assertFalse(retiredFile2.exists());
        assertTrue(activeFile.exists());
        assertTrue(appMetadataFile.exists());

        List<Integer> mergedIds = readTabIdsFromMetadataFile(appMetadataFile);
        assertEquals(4, mergedIds.size());
        assertTrue(mergedIds.containsAll(List.of(100, 101, 102, 103)));
        assertFalse(mergedIds.contains(999));
    }

    @Test
    public void testRetireMetadataWaitsForInitializationToFinish() throws Exception {
        when(mIntentDataProvider.isTabbedCctEnabled()).thenReturn(true);
        when(mIntentDataProvider.getClientPackageName()).thenReturn(CLIENT_PACKAGE);
        when(mIntentDataProvider.getTabbedCctAccountId()).thenReturn(ACCOUNT_A);

        // Passing an empty savedInstanceState Bundle sets mIsLegacyRestore = true, so
        // performInitialization() will migrate "tab_state77" -> "tab_state_active_77_<uuid>".
        // Until performInitialization() finishes, the active metadata file does not exist yet.
        Bundle legacySavedInstanceState = new Bundle();
        CustomTabTabPersistencePolicy realPolicy =
                new CustomTabTabPersistencePolicy(
                        mActivity1, legacySavedInstanceState, mIntentDataProvider);
        TestCustomTabActivity activityWithRealPolicy =
                new TestCustomTabActivity(TASK_ID, realPolicy, mNavigationController1);

        File stateDir = realPolicy.getOrCreateStateDirectory();
        File legacyFile =
                new File(
                        stateDir,
                        TabMetadataFileManager.getMetadataFileName(Integer.toString(TASK_ID)));
        writeMetadataFile(legacyFile, new int[] {201}, new String[] {"https://example.com/real"});
        File activeFile = new File(stateDir, realPolicy.getMetadataFileName());
        assertTrue(legacyFile.exists());
        assertFalse(activeFile.exists());

        CustomTabAppTabModelOrchestrator appOrch =
                CustomTabAppTabModelOrchestrator.getOrCreate(
                        activityWithRealPolicy, CLIENT_PACKAGE, ACCOUNT_A);
        realPolicy.setTaskRunner(appOrch.getTaskRunner());

        // Run performInitialization on a separate TaskRunner and immediately call
        // retireMetadataStateFileAsync(); waitForInitializationToFinish() ensures legacyFile is
        // migrated to activeFile before retireMetadataStateFileAsync() renames activeFile.
        realPolicy.performInitialization(
                PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING_MAY_BLOCK));
        realPolicy.retireMetadataStateFileAsync();
        CustomTabAppTabModelOrchestrator.getOrCreate(
                activityWithRealPolicy, CLIENT_PACKAGE, ACCOUNT_A);
        RobolectricUtil.runAllBackgroundAndUi();

        assertFalse(legacyFile.exists());
        assertFalse(activeFile.exists());

        File appMetadataFile =
                new File(stateDir, CustomTabTabPersistencePolicy.APP_METADATA_FILE_NAME);
        assertTrue(appMetadataFile.exists());
        assertEquals(List.of(201), readTabIdsFromMetadataFile(appMetadataFile));
    }

    @Test
    public void testAccountSwitchTearsDownExistingOrchestratorAndClosesLiveTabsAndCcts() {
        CustomTabActivityTabFactory factoryA =
                createTabFactory(mActivity1, mPolicy1, "tab_state_active_77_uuid1");
        TabModelOrchestrator activityOrchA = factoryA.createTabModelOrchestrator();
        mActivity1.setTabModelOrchestratorForTesting(activityOrchA);
        factoryA.createTabModels();

        CustomTabAppTabModelOrchestrator appOrchA =
                CustomTabAppTabModelOrchestrator.getOrCreate(mActivity1, CLIENT_PACKAGE, ACCOUNT_A);
        // Also register an uninitialized activity for Account A.
        CustomTabAppTabModelOrchestrator.getOrCreate(mActivity2, CLIENT_PACKAGE, ACCOUNT_A);

        // Attach a mock TabModelJniBridge with 1 live tab to Activity 1's TabModelSelector to
        // verify live tabs for account A are closed on account switch.
        TabModelSelectorImpl selectorA = (TabModelSelectorImpl) activityOrchA.getTabModelSelector();
        TabModelJniBridge mockNormalModel = mock(TabModelJniBridge.class);
        IncognitoTabModelInternal mockIncognitoModel = mock(IncognitoTabModelInternal.class);
        TabRemover mockTabRemover = mock(TabRemover.class);
        when(mockNormalModel.getCount()).thenReturn(1);
        when(mockNormalModel.getTabRemover()).thenReturn(mockTabRemover);
        when(mockNormalModel.iterator()).thenAnswer(inv -> Collections.emptyIterator());
        when(mockIncognitoModel.isIncognito()).thenReturn(true);
        when(mockIncognitoModel.iterator()).thenAnswer(inv -> Collections.emptyIterator());
        selectorA.initializeForTesting(mockNormalModel, mockIncognitoModel);
        selectorA.markTabStateInitialized();
        publishSelectorToActivity(mActivity1, selectorA);

        // Now launch a Custom Tab for the same app under Account B.
        CustomTabAppTabModelOrchestrator appOrchB =
                CustomTabAppTabModelOrchestrator.getOrCreate(
                        mActivityAccountB, CLIENT_PACKAGE, ACCOUNT_B);
        RobolectricUtil.runAllBackgroundAndUi();

        assertNotSame(appOrchA, appOrchB);
        assertTrue(appOrchA.isDestroyedForTesting());
        assertEquals(ACCOUNT_B, appOrchB.getAccountId());
        assertSame(
                appOrchB,
                CustomTabAppTabModelOrchestrator.getInstanceForPackageForTesting(CLIENT_PACKAGE));

        // Verify Account A's active metadata was saved and retired for initialized activities only,
        // its CCTs were finished, and its live tabs were closed.
        File stateDir =
                TabStateDirectory.getOrCreateCustomTabAppStateDirectory(CLIENT_PACKAGE, ACCOUNT_A);
        assertTrue(new File(stateDir, "tab_state_active_77_uuid1").exists());
        verify(mPolicy1).retireMetadataStateFileAsync();
        verify(mPolicy2, never()).retireMetadataStateFileAsync();
        verify(mNavigationController1).finish(FinishReason.OTHER);
        verify(mActivity2.getCustomTabActivityNavigationController()).finish(FinishReason.OTHER);
        verify(mockTabRemover).closeTabs(any(TabClosureParams.class), anyBoolean());
        assertEquals(0, appOrchA.getActiveActivityCountForTesting());
        assertTrue(appOrchA.getActivityTabModelSelectors().isEmpty());
        factoryA.destroyTabModelOrchestrator();
    }

    @Test
    public void testAccountSwitchDoesNotOverwriteActiveMetadataWhenTabStateUninitialized()
            throws Exception {
        when(mIntentDataProvider.isTabbedCctEnabled()).thenReturn(true);
        when(mIntentDataProvider.getClientPackageName()).thenReturn(CLIENT_PACKAGE);
        when(mIntentDataProvider.getTabbedCctAccountId()).thenReturn(ACCOUNT_A);
        when(mIntentDataProvider.getCustomTabMode()).thenReturn(CustomTabProfileType.REGULAR);

        // Simulate a recreating activity whose savedInstanceState preserves its active metadata
        // filename, with persisted tab 301 on disk, before loadState()/markTabStateInitialized().
        CustomTabTabPersistencePolicy initialPolicy =
                new CustomTabTabPersistencePolicy(mActivity1, null, mIntentDataProvider);
        Bundle savedInstanceState = new Bundle();
        initialPolicy.saveToBundle(savedInstanceState);

        CustomTabTabPersistencePolicy recreatingPolicy =
                new CustomTabTabPersistencePolicy(
                        mActivity1, savedInstanceState, mIntentDataProvider);
        TestCustomTabActivity recreatingActivity =
                new TestCustomTabActivity(TASK_ID, recreatingPolicy, mNavigationController1);

        File stateDir = recreatingPolicy.getOrCreateStateDirectory();
        File activeFile = new File(stateDir, recreatingPolicy.getMetadataFileName());
        writeMetadataFile(activeFile, new int[] {301}, new String[] {"https://example.com/saved"});

        CustomTabActivityTabFactory factoryA =
                new CustomTabActivityTabFactory(
                        recreatingActivity,
                        recreatingPolicy,
                        mWindowAndroid,
                        mProfileProviderSupplier,
                        mDelegateFactory,
                        mIntentDataProvider,
                        mTabCreatorManager,
                        () -> null,
                        () -> null,
                        mCipherFactory);
        factoryA.setActivityType(ActivityType.CUSTOM_TAB);

        TabModelOrchestrator activityOrchA = factoryA.createTabModelOrchestrator();
        recreatingActivity.setTabModelOrchestratorForTesting(activityOrchA);
        CustomTabAppTabModelOrchestrator appOrchA =
                CustomTabAppTabModelOrchestrator.getOrCreate(
                        recreatingActivity, CLIENT_PACKAGE, ACCOUNT_A);
        recreatingPolicy.setTaskRunner(appOrchA.getTaskRunner());
        factoryA.createTabModels();
        assertFalse(activityOrchA.getTabModelSelector().isTabStateInitialized());

        // Trigger account switch before tab state is initialized; saveState() must not overwrite
        // activeFile with 0 tabs before retireMetadataStateFileAsync() retires it.
        CustomTabAppTabModelOrchestrator.getOrCreate(mActivityAccountB, CLIENT_PACKAGE, ACCOUNT_B);
        RobolectricUtil.runAllBackgroundAndUi();
        assertFalse(activeFile.exists());

        // Switching back to Account A merges the retired metadata file into tab_state_app with
        // tab 301 intact.
        CustomTabAppTabModelOrchestrator.getOrCreate(recreatingActivity, CLIENT_PACKAGE, ACCOUNT_A);
        RobolectricUtil.runAllBackgroundAndUi();

        File appMetadataFile =
                new File(stateDir, CustomTabTabPersistencePolicy.APP_METADATA_FILE_NAME);
        assertTrue(appMetadataFile.exists());
        assertEquals(List.of(301), readTabIdsFromMetadataFile(appMetadataFile));
        factoryA.destroyTabModelOrchestrator();
    }

    @Test
    public void testMergeAbortsOnReadOrWriteFailureWithoutDroppingRetiredFiles() throws Exception {
        File stateDir =
                TabStateDirectory.getOrCreateCustomTabAppStateDirectory(CLIENT_PACKAGE, ACCOUNT_A);
        File appMetadataFile =
                new File(stateDir, CustomTabTabPersistencePolicy.APP_METADATA_FILE_NAME);
        File retiredFile = new File(stateDir, "tab_state_retired_77_uuidErr_1000");
        writeMetadataFile(retiredFile, new int[] {501}, new String[] {"https://example.com/501"});

        // 1. Corrupt appMetadataFile so reading it fails; merging should abort without deleting
        // retiredFile.
        try (FileOutputStream out = new FileOutputStream(appMetadataFile)) {
            out.write(new byte[] {1, 2});
        }
        CustomTabAppTabModelOrchestrator orch1 =
                CustomTabAppTabModelOrchestrator.getOrCreate(mActivity1, CLIENT_PACKAGE, ACCOUNT_A);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(retiredFile.exists());
        orch1.destroy();

        // 2. Remove corrupt appMetadataFile, then block AtomicFile.startWrite() by placing a
        // non-empty directory at "tab_state_app.new"; writing fails and retiredFile is preserved.
        assertTrue(appMetadataFile.delete());
        File writeBlockerDir = new File(stateDir, appMetadataFile.getName() + ".new");
        assertTrue(writeBlockerDir.mkdir());
        File writeBlockerChild = new File(writeBlockerDir, "blocker");
        assertTrue(writeBlockerChild.createNewFile());

        CustomTabAppTabModelOrchestrator orch2 =
                CustomTabAppTabModelOrchestrator.getOrCreate(mActivity1, CLIENT_PACKAGE, ACCOUNT_A);
        RobolectricUtil.runAllBackgroundAndUi();
        assertFalse(appMetadataFile.exists());
        assertTrue(retiredFile.exists());
        orch2.destroy();

        // 3. Remove the write blocker; merging now succeeds and cleans up retiredFile.
        assertTrue(writeBlockerChild.delete());
        assertTrue(writeBlockerDir.delete());
        CustomTabAppTabModelOrchestrator.getOrCreate(mActivity1, CLIENT_PACKAGE, ACCOUNT_A);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(appMetadataFile.exists());
        assertFalse(retiredFile.exists());
        assertEquals(List.of(501), readTabIdsFromMetadataFile(appMetadataFile));
    }

    @Test
    public void testCleanupTaskIgnoresAppMetadataFileAndItsTabs() throws Exception {
        when(mIntentDataProvider.isTabbedCctEnabled()).thenReturn(true);
        when(mIntentDataProvider.getClientPackageName()).thenReturn(CLIENT_PACKAGE);
        when(mIntentDataProvider.getTabbedCctAccountId()).thenReturn(ACCOUNT_A);

        CustomTabTabPersistencePolicy realPolicy =
                new CustomTabTabPersistencePolicy(mActivity1, null, mIntentDataProvider);
        File stateDir = realPolicy.getOrCreateStateDirectory();

        // Create an expired tab_state_app referencing tab 601.
        File appMetadataFile =
                new File(stateDir, CustomTabTabPersistencePolicy.APP_METADATA_FILE_NAME);
        writeMetadataFile(appMetadataFile, new int[] {601}, new String[] {"https://example.com/1"});
        assertTrue(appMetadataFile.setLastModified(1234));
        File tab601File =
                TabStateFileManager.getTabStateFile(
                        stateDir, 601, false, /* isFlatbuffer= */ false);
        assertTrue(tab601File.createNewFile());

        // Create an expired retired metadata file referencing both 601 (shared with tab_state_app)
        // and 602 (only in the expired retired file), plus an unreferenced tab 603.
        File expiredRetiredFile = new File(stateDir, "tab_state_retired_77_old_1000");
        writeMetadataFile(
                expiredRetiredFile,
                new int[] {601, 602},
                new String[] {"https://example.com/1", "https://example.com/2"});
        assertTrue(expiredRetiredFile.setLastModified(1234));
        File tab602File =
                TabStateFileManager.getTabStateFile(
                        stateDir, 602, false, /* isFlatbuffer= */ false);
        assertTrue(tab602File.createNewFile());
        File tab603File =
                TabStateFileManager.getTabStateFile(
                        stateDir, 603, false, /* isFlatbuffer= */ false);
        assertTrue(tab603File.createNewFile());

        AtomicReference<TabPersistenceFileInfo> cleanupResult = new AtomicReference<>();
        realPolicy.cleanupUnusedFiles(cleanupResult::set);
        RobolectricUtil.runAllBackgroundAndUi();

        assertNotNull(cleanupResult.get());
        assertEquals(List.of(expiredRetiredFile.getName()), cleanupResult.get().getMetadataFiles());
        assertFalse(cleanupResult.get().getMetadataFiles().contains(appMetadataFile.getName()));
        assertTrue(
                cleanupResult
                        .get()
                        .getTabStateFileInfos()
                        .containsAll(
                                List.of(
                                        new TabStateFileInfo(602, false),
                                        new TabStateFileInfo(603, false))));
        assertFalse(
                cleanupResult
                        .get()
                        .getTabStateFileInfos()
                        .contains(new TabStateFileInfo(601, false)));
    }
}
