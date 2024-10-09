// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import android.app.Activity;
import android.util.SparseBooleanArray;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.CustomTabsTabModelOrchestrator;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabPersistenceFileInfo;
import org.chromium.chrome.browser.tabmodel.TabPersistenceFileInfo.TabStateFileInfo;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.url.GURL;

import java.io.File;
import java.io.FileOutputStream;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for the Custom Tab persistence logic. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class CustomTabTabPersistencePolicyTest {
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;

    private TestTabModelDirectory mMockDirectory;
    private AdvancedMockContext mAppContext;
    private SequencedTaskRunner mSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE);

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        Mockito.when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);

        // CustomTabsConnection needs a true context, not the mock context set below.
        ThreadUtils.runOnUiThreadBlocking(() -> CustomTabsConnection.getInstance());

        mAppContext =
                new AdvancedMockContext(
                        InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getApplicationContext());
        ContextUtils.initApplicationContextForTests(mAppContext);

        mMockDirectory =
                new TestTabModelDirectory(
                        mAppContext,
                        "CustomTabTabPersistencePolicyTest",
                        TabStateDirectory.CUSTOM_TABS_DIRECTORY);
        TabStateDirectory.setBaseStateDirectoryForTests(mMockDirectory.getBaseDirectory());
    }

    @After
    public void tearDown() {
        mMockDirectory.tearDown();

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            ApplicationStatus.onStateChangeForTesting(
                                    activity, ActivityState.DESTROYED));
        }
    }

    /** Test to ensure that an existing metadata files are deleted if no restore is requested. */
    @Test
    @Feature("TabPersistentStore")
    @MediumTest
    public void testExistingMetadataFileDeletedIfNoRestore() throws Exception {
        File baseStateDirectory = TabStateDirectory.getOrCreateBaseStateDirectory();
        Assert.assertNotNull(baseStateDirectory);

        CustomTabTabPersistencePolicy policy = new CustomTabTabPersistencePolicy(7, false);
        File stateDirectory = policy.getOrCreateStateDirectory();
        Assert.assertNotNull(stateDirectory);

        String stateFileName = policy.getMetadataFileName();
        File existingStateFile = new File(stateDirectory, stateFileName);
        Assert.assertTrue(existingStateFile.createNewFile());

        Assert.assertTrue(existingStateFile.exists());
        policy.performInitialization(mSequencedTaskRunner);
        policy.waitForInitializationToFinish();
        Assert.assertFalse(existingStateFile.exists());
    }

    /** Test the logic that gets all the live tab and task IDs. */
    @Test
    @Feature("TabPersistentStore")
    @SmallTest
    @UiThreadTest
    public void testGettingTabAndTaskIds() throws Throwable {
        Set<Integer> tabIds = new HashSet<>();
        Set<Integer> taskIds = new HashSet<>();
        CustomTabTabPersistencePolicy.getAllLiveTabAndTaskIds(tabIds, taskIds);
        assertThat(tabIds, Matchers.emptyIterable());
        assertThat(taskIds, Matchers.emptyIterable());

        tabIds.clear();
        taskIds.clear();

        CustomTabActivity cct1 = buildTestCustomTabActivity(1, new int[] {4, 8, 9}, null);
        ApplicationStatus.onStateChangeForTesting(cct1, ActivityState.CREATED);

        CustomTabActivity cct2 = buildTestCustomTabActivity(5, new int[] {458}, new int[] {9878});
        ApplicationStatus.onStateChangeForTesting(cct2, ActivityState.CREATED);

        // Add a tabbed mode activity to ensure that its IDs are not included in the
        // returned CCT ID sets.
        final TabModelSelectorImpl tabbedSelector =
                buildTestTabModelSelector(new int[] {12121212}, new int[] {1515151515});
        ChromeTabbedActivity tabbedActivity =
                new ChromeTabbedActivity() {
                    @Override
                    public int getTaskId() {
                        return 888;
                    }

                    @Override
                    public TabModelSelector getTabModelSelector() {
                        return tabbedSelector;
                    }
                };
        ApplicationStatus.onStateChangeForTesting(tabbedActivity, ActivityState.CREATED);

        CustomTabTabPersistencePolicy.getAllLiveTabAndTaskIds(tabIds, taskIds);
        assertThat(tabIds, Matchers.containsInAnyOrder(4, 8, 9, 458, 9878));
        assertThat(taskIds, Matchers.containsInAnyOrder(1, 5));
    }

    /** Test the full cleanup task path that determines what files are eligible for deletion. */
    @Test
    @Feature("TabPersistentStore")
    @MediumTest
    public void testCleanupTask() throws Throwable {
        File baseStateDirectory = TabStateDirectory.getOrCreateBaseStateDirectory();
        Assert.assertNotNull(baseStateDirectory);

        CustomTabTabPersistencePolicy policy = new CustomTabTabPersistencePolicy(2, false);
        File stateDirectory = policy.getOrCreateStateDirectory();
        Assert.assertNotNull(stateDirectory);

        final AtomicReference<TabPersistenceFileInfo> tabDataToDelete = new AtomicReference<>();
        final CallbackHelper callbackSignal = new CallbackHelper();
        Callback<TabPersistenceFileInfo> tabDataToDeleteCallback =
                new Callback<TabPersistenceFileInfo>() {
                    @Override
                    public void onResult(TabPersistenceFileInfo tabData) {
                        tabDataToDelete.set(tabData);
                        callbackSignal.notifyCalled();
                    }
                };

        // Test when no files have been created.
        policy.cleanupUnusedFiles(tabDataToDeleteCallback);
        callbackSignal.waitForCallback(0);
        assertThat(tabDataToDelete.get().getMetadataFiles(), Matchers.emptyIterable());

        // Create an unreferenced tab state file and ensure it is marked for deletion.
        File tab999File =
                TabStateFileManager.getTabStateFile(
                        stateDirectory, 999, false, /* isFlatBuffer= */ false);
        Assert.assertTrue(tab999File.createNewFile());
        policy.cleanupUnusedFiles(tabDataToDeleteCallback);
        callbackSignal.waitForCallback(1);
        assertThat(
                tabDataToDelete.get().getTabStateFileInfos().get(0).tabId, Matchers.equalTo(999));
        assertThat(
                tabDataToDelete.get().getTabStateFileInfos().get(0).isEncrypted,
                Matchers.equalTo(false));

        // Reference the tab state file and ensure it is no longer marked for deletion.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    CustomTabActivity cct1 = buildTestCustomTabActivity(1, new int[] {999}, null);
                    ApplicationStatus.onStateChangeForTesting(cct1, ActivityState.CREATED);
                });
        policy.cleanupUnusedFiles(tabDataToDeleteCallback);
        callbackSignal.waitForCallback(2);
        assertThat(tabDataToDelete.get().getMetadataFiles(), Matchers.emptyIterable());

        // Create a tab model and associated tabs. Ensure it is not marked for deletion as it is
        // new enough.
        TabModelSelectorMetadata data =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            TabModelSelectorImpl selectorImpl =
                                    buildTestTabModelSelector(new int[] {111, 222, 333}, null);
                            return TabPersistentStore.saveTabModelSelectorMetadata(
                                    selectorImpl, null);
                        });
        FileOutputStream fos = null;
        File metadataFile = new File(stateDirectory, TabPersistentStore.getMetadataFileName("3"));
        try {
            TabPersistentStore.saveListToFile(metadataFile, data);
        } finally {
            StreamUtil.closeQuietly(fos);
        }
        File tab111File =
                TabStateFileManager.getTabStateFile(
                        stateDirectory, 111, false, /* isFlatBuffer= */ false);
        Assert.assertTrue(tab111File.createNewFile());
        File tab222File =
                TabStateFileManager.getTabStateFile(
                        stateDirectory, 222, false, /* isFlatBuffer= */ false);
        Assert.assertTrue(tab222File.createNewFile());
        File tab333File =
                TabStateFileManager.getTabStateFile(
                        stateDirectory, 333, false, /* isFlatBuffer= */ false);
        Assert.assertTrue(tab333File.createNewFile());
        policy.cleanupUnusedFiles(tabDataToDeleteCallback);
        callbackSignal.waitForCallback(3);
        assertThat(tabDataToDelete.get().getMetadataFiles(), Matchers.emptyIterable());

        // Set the age of the metadata file to be past the expiration threshold and ensure it along
        // with the associated tab files are marked for deletion.
        Assert.assertTrue(metadataFile.setLastModified(1234));
        policy.cleanupUnusedFiles(tabDataToDeleteCallback);
        callbackSignal.waitForCallback(4);
        assertThat(
                tabDataToDelete.get().getTabStateFileInfos(),
                Matchers.containsInAnyOrder(
                        new TabStateFileInfo(111, false),
                        new TabStateFileInfo(222, false),
                        new TabStateFileInfo(333, false)));
        assertThat(
                tabDataToDelete.get().getMetadataFiles(),
                Matchers.containsInAnyOrder(metadataFile.getName()));
    }

    /** Ensure that the metadata file's last modified timestamp is updated on initialization. */
    @Test
    @Feature("TabPersistentStore")
    @MediumTest
    public void testMetadataTimestampRefreshed() throws Exception {
        File baseStateDirectory = TabStateDirectory.getOrCreateBaseStateDirectory();
        Assert.assertNotNull(baseStateDirectory);

        CustomTabTabPersistencePolicy policy = new CustomTabTabPersistencePolicy(2, true);
        File stateDirectory = policy.getOrCreateStateDirectory();
        Assert.assertNotNull(stateDirectory);

        File metadataFile = new File(stateDirectory, policy.getMetadataFileName());
        Assert.assertTrue(metadataFile.createNewFile());

        long previousTimestamp =
                System.currentTimeMillis() - CustomTabFileUtils.STATE_EXPIRY_THRESHOLD;
        Assert.assertTrue(metadataFile.setLastModified(previousTimestamp));

        policy.performInitialization(mSequencedTaskRunner);
        policy.waitForInitializationToFinish();

        Assert.assertTrue(metadataFile.lastModified() > previousTimestamp);
    }

    private CustomTabActivity buildTestCustomTabActivity(
            final int taskId, int[] normalTabIds, int[] incognitoTabIds) {
        final TabModelSelectorImpl selectorImpl =
                buildTestTabModelSelector(normalTabIds, incognitoTabIds);
        return new CustomTabActivity() {
            @Override
            public int getTaskId() {
                return taskId;
            }

            @Override
            public TabModelSelectorImpl getTabModelSelector() {
                return selectorImpl;
            }
        };
    }

    private static TabPersistencePolicy buildTestPersistencePolicy() {
        return new TabPersistencePolicy() {
            @Override
            public void waitForInitializationToFinish() {}

            @Override
            public void setTabContentManager(TabContentManager cache) {}

            @Override
            public void setMergeInProgress(boolean isStarted) {}

            @Override
            public boolean performInitialization(TaskRunner taskRunner) {
                return false;
            }

            @Override
            public boolean shouldMergeOnStartup() {
                return false;
            }

            @Override
            public boolean isMergeInProgress() {
                return false;
            }

            @Override
            public @Nullable String getMetadataFileNameToBeMerged() {
                return null;
            }

            @Override
            public @NonNull String getMetadataFileName() {
                return TabPersistentStore.getMetadataFileName("cct_testing0");
            }

            @Override
            public File getOrCreateStateDirectory() {
                return new File(TabStateDirectory.getOrCreateBaseStateDirectory(), "cct_tests_zor");
            }

            @Override
            public void notifyStateLoaded(int tabCountAtStartup) {}

            @Override
            public void destroy() {}

            @Override
            public void cleanupUnusedFiles(Callback<TabPersistenceFileInfo> filesToDelete) {}

            @Override
            public void cancelCleanupInProgress() {}

            @Override
            public void getAllTabIds(Callback<SparseBooleanArray> tabIdsCallback) {}
        };
    }

    private TabModelSelectorImpl buildTestTabModelSelector(
            int[] normalTabIds, int[] incognitoTabIds) {
        MockTabModel.MockTabModelDelegate tabModelDelegate =
                new MockTabModel.MockTabModelDelegate() {
                    @Override
                    public MockTab createTab(int id, boolean incognito) {
                        Profile profile = incognito ? mIncognitoProfile : mProfile;
                        return new MockTab(id, profile) {
                            @Override
                            public GURL getUrl() {
                                return new GURL("https://www.google.com");
                            }
                        };
                    }
                };
        final MockTabModel normalTabModel = new MockTabModel(mProfile, tabModelDelegate);
        if (normalTabIds != null) {
            for (int tabId : normalTabIds) normalTabModel.addTab(tabId);
        }
        final MockTabModel incognitoTabModel =
                new MockTabModel(mIncognitoProfile, tabModelDelegate);
        if (incognitoTabIds != null) {
            for (int tabId : incognitoTabIds) incognitoTabModel.addTab(tabId);
        }

        CustomTabActivity customTabActivity =
                new CustomTabActivity() {
                    // This is intended to pretend we've started the activity, so we can attach a
                    // base context to the activity.
                    @Override
                    public void onStart() {
                        attachBaseContext(mAppContext);
                    }
                };
        ApplicationStatus.onStateChangeForTesting(customTabActivity, ActivityState.CREATED);
        ActivityStateListener stateListener =
                (activity, state) -> {
                    if (state == ActivityState.STARTED) {
                        customTabActivity.onStart();
                    }
                };
        ApplicationStatus.registerStateListenerForActivity(stateListener, customTabActivity);
        ApplicationStatus.onStateChangeForTesting(customTabActivity, ActivityState.STARTED);

        OneshotSupplierImpl<ProfileProvider> profileProviderSupplier = new OneshotSupplierImpl<>();
        profileProviderSupplier.set(mProfileProvider);
        Mockito.when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);

        CustomTabsTabModelOrchestrator orchestrator = new CustomTabsTabModelOrchestrator();
        orchestrator.createTabModels(
                profileProviderSupplier,
                customTabActivity,
                buildTestPersistencePolicy(),
                ActivityType.CUSTOM_TAB,
                AsyncTabParamsManagerSingleton.getInstance(),
                new CipherFactory());
        TabModelSelectorImpl selector = (TabModelSelectorImpl) orchestrator.getTabModelSelector();
        selector.initializeForTesting(normalTabModel, incognitoTabModel);
        ApplicationStatus.onStateChangeForTesting(customTabActivity, ActivityState.DESTROYED);
        ApplicationStatus.unregisterActivityStateListener(stateListener);
        return selector;
    }
}
