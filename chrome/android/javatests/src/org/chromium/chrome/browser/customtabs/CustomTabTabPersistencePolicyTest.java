// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import androidx.annotation.Nullable;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.StreamUtil;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for the Custom Tab persistence logic.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class CustomTabTabPersistencePolicyTest {
    private TestTabModelDirectory mMockDirectory;
    private AdvancedMockContext mAppContext;
    private SequencedTaskRunner mSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE);
    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    @Before
    public void setUp() {
        // CustomTabsConnection needs a true context, not the mock context set below.
        CustomTabsConnection.getInstance();

        mAppContext = new AdvancedMockContext(InstrumentationRegistry.getInstrumentation()
                                                      .getTargetContext()
                                                      .getApplicationContext());
        ContextUtils.initApplicationContextForTests(mAppContext);

        mMockDirectory = new TestTabModelDirectory(
                mAppContext, "CustomTabTabPersistencePolicyTest",
                CustomTabTabPersistencePolicy.SAVED_STATE_DIRECTORY);
        TabPersistentStore.setBaseStateDirectoryForTests(mMockDirectory.getBaseDirectory());
    }

    @After
    public void tearDown() {
        mMockDirectory.tearDown();

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> ApplicationStatus.onStateChangeForTesting(
                            activity, ActivityState.DESTROYED));
        }
    }

    @Test
    @Feature("TabPersistentStore")
    @SmallTest
    public void testDeletableMetadataSelection_NoFiles() {
        List<File> deletableFiles = CustomTabTabPersistencePolicy.getMetadataFilesForDeletion(
                System.currentTimeMillis(), new ArrayList<File>());
        Assert.assertThat(deletableFiles, Matchers.emptyIterableOf(File.class));
    }

    @Test
    @Feature("TabPersistentStore")
    @SmallTest
    public void testDeletableMetadataSelection_MaximumValidFiles() {
        long currentTime = System.currentTimeMillis();

        // Test the maximum allowed number of state files where they are all valid in terms of age.
        List<File> filesToTest = new ArrayList<>();
        filesToTest.addAll(generateMaximumStateFiles(currentTime));
        List<File> deletableFiles = CustomTabTabPersistencePolicy.getMetadataFilesForDeletion(
                currentTime, filesToTest);
        Assert.assertThat(deletableFiles, Matchers.emptyIterableOf(File.class));
    }

    @Test
    @Feature("TabPersistentStore")
    @SmallTest
    public void testDeletableMetadataSelection_ExceedsMaximumValidFiles() {
        long currentTime = System.currentTimeMillis();

        // Test where we exceed the maximum number of allowed state files and ensure it chooses the
        // older file to delete.
        List<File> filesToTest = new ArrayList<>();
        filesToTest.addAll(generateMaximumStateFiles(currentTime));
        File slightlyOlderFile = buildTestFile("slightlyolderfile", currentTime - 1L);
        // Insert it into the middle just to ensure it is not picking the last file.
        filesToTest.add(filesToTest.size() / 2, slightlyOlderFile);
        List<File> deletableFiles = CustomTabTabPersistencePolicy.getMetadataFilesForDeletion(
                currentTime, filesToTest);
        Assert.assertThat(deletableFiles, Matchers.containsInAnyOrder(slightlyOlderFile));
    }

    @Test
    @Feature("TabPersistentStore")
    @SmallTest
    public void testDeletableMetadataSelection_ExceedExpiryThreshold() {
        long currentTime = System.currentTimeMillis();

        // Ensure that files that exceed the allowed time threshold are removed regardless of the
        // number of possible files.
        List<File> filesToTest = new ArrayList<>();
        File expiredFile = buildTestFile("expired_file",
                currentTime - CustomTabTabPersistencePolicy.STATE_EXPIRY_THRESHOLD);
        filesToTest.add(expiredFile);
        List<File> deletableFiles = CustomTabTabPersistencePolicy.getMetadataFilesForDeletion(
                currentTime, filesToTest);
        Assert.assertThat(deletableFiles, Matchers.containsInAnyOrder(expiredFile));
    }

    /**
     * Test to ensure that an existing metadata files are deleted if no restore is requested.
     */
    @Test
    @Feature("TabPersistentStore")
    @MediumTest
    public void testExistingMetadataFileDeletedIfNoRestore() throws Exception {
        File baseStateDirectory = TabPersistentStore.getOrCreateBaseStateDirectory();
        Assert.assertNotNull(baseStateDirectory);

        CustomTabTabPersistencePolicy policy = new CustomTabTabPersistencePolicy(7, false);
        File stateDirectory = policy.getOrCreateStateDirectory();
        Assert.assertNotNull(stateDirectory);

        String stateFileName = policy.getStateFileName();
        File existingStateFile = new File(stateDirectory, stateFileName);
        Assert.assertTrue(existingStateFile.createNewFile());

        Assert.assertTrue(existingStateFile.exists());
        policy.performInitialization(mSequencedTaskRunner);
        policy.waitForInitializationToFinish();
        Assert.assertFalse(existingStateFile.exists());
    }

    /**
     * Test the logic that gets all the live tab and task IDs.
     */
    @Test
    @Feature("TabPersistentStore")
    @SmallTest
    public void testGettingTabAndTaskIds() throws Throwable {
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Set<Integer> tabIds = new HashSet<>();
                Set<Integer> taskIds = new HashSet<>();
                CustomTabTabPersistencePolicy.getAllLiveTabAndTaskIds(tabIds, taskIds);
                Assert.assertThat(tabIds, Matchers.emptyIterable());
                Assert.assertThat(taskIds, Matchers.emptyIterable());

                tabIds.clear();
                taskIds.clear();

                CustomTabActivity cct1 = buildTestCustomTabActivity(1, new int[] {4, 8, 9}, null);
                ApplicationStatus.onStateChangeForTesting(cct1, ActivityState.CREATED);

                CustomTabActivity cct2 =
                        buildTestCustomTabActivity(5, new int[] {458}, new int[] {9878});
                ApplicationStatus.onStateChangeForTesting(cct2, ActivityState.CREATED);

                // Add a tabbed mode activity to ensure that its IDs are not included in the
                // returned CCT ID sets.
                final TabModelSelectorImpl tabbedSelector =
                        buildTestTabModelSelector(new int[] {12121212}, new int[] {1515151515});
                ChromeTabbedActivity tabbedActivity = new ChromeTabbedActivity() {
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
                Assert.assertThat(tabIds, Matchers.containsInAnyOrder(4, 8, 9, 458, 9878));
                Assert.assertThat(taskIds, Matchers.containsInAnyOrder(1, 5));
            }
        });
    }

    /**
     * Test the full cleanup task path that determines what files are eligible for deletion.
     */
    @Test
    @Feature("TabPersistentStore")
    @MediumTest
    public void testCleanupTask() throws Throwable {
        File baseStateDirectory = TabPersistentStore.getOrCreateBaseStateDirectory();
        Assert.assertNotNull(baseStateDirectory);

        CustomTabTabPersistencePolicy policy = new CustomTabTabPersistencePolicy(2, false);
        File stateDirectory = policy.getOrCreateStateDirectory();
        Assert.assertNotNull(stateDirectory);

        final AtomicReference<List<String>> filesToDelete = new AtomicReference<>();
        final CallbackHelper callbackSignal = new CallbackHelper();
        Callback<List<String>> filesToDeleteCallback = new Callback<List<String>>() {
            @Override
            public void onResult(List<String> fileNames) {
                filesToDelete.set(fileNames);
                callbackSignal.notifyCalled();
            }
        };

        // Test when no files have been created.
        policy.cleanupUnusedFiles(filesToDeleteCallback);
        callbackSignal.waitForCallback(0);
        Assert.assertThat(filesToDelete.get(), Matchers.emptyIterable());

        // Create an unreferenced tab state file and ensure it is marked for deletion.
        File tab999File = TabState.getTabStateFile(stateDirectory, 999, false);
        Assert.assertTrue(tab999File.createNewFile());
        policy.cleanupUnusedFiles(filesToDeleteCallback);
        callbackSignal.waitForCallback(1);
        Assert.assertThat(filesToDelete.get(), Matchers.containsInAnyOrder(tab999File.getName()));

        // Reference the tab state file and ensure it is no longer marked for deletion.
        mRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                CustomTabActivity cct1 = buildTestCustomTabActivity(1, new int[] {999}, null);
                ApplicationStatus.onStateChangeForTesting(cct1, ActivityState.CREATED);
            }
        });
        policy.cleanupUnusedFiles(filesToDeleteCallback);
        callbackSignal.waitForCallback(2);
        Assert.assertThat(filesToDelete.get(), Matchers.emptyIterable());

        // Create a tab model and associated tabs. Ensure it is not marked for deletion as it is
        // new enough.
        byte[] data = TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<byte[]>() {
            @Override
            public byte[] call() throws Exception {
                TabModelSelectorImpl selectorImpl =
                        buildTestTabModelSelector(new int[] {111, 222, 333}, null);
                return TabPersistentStore.serializeTabModelSelector(selectorImpl, null).listData;
            }
        });
        FileOutputStream fos = null;
        File metadataFile = new File(
                stateDirectory, TabPersistentStore.getStateFileName("3"));
        try {
            fos = new FileOutputStream(metadataFile);
            fos.write(data);
        } finally {
            StreamUtil.closeQuietly(fos);
        }
        File tab111File = TabState.getTabStateFile(stateDirectory, 111, false);
        Assert.assertTrue(tab111File.createNewFile());
        File tab222File = TabState.getTabStateFile(stateDirectory, 222, false);
        Assert.assertTrue(tab222File.createNewFile());
        File tab333File = TabState.getTabStateFile(stateDirectory, 333, false);
        Assert.assertTrue(tab333File.createNewFile());
        policy.cleanupUnusedFiles(filesToDeleteCallback);
        callbackSignal.waitForCallback(3);
        Assert.assertThat(filesToDelete.get(), Matchers.emptyIterable());

        // Set the age of the metadata file to be past the expiration threshold and ensure it along
        // with the associated tab files are marked for deletion.
        Assert.assertTrue(metadataFile.setLastModified(1234));
        policy.cleanupUnusedFiles(filesToDeleteCallback);
        callbackSignal.waitForCallback(4);
        Assert.assertThat(filesToDelete.get(),
                Matchers.containsInAnyOrder(tab111File.getName(), tab222File.getName(),
                        tab333File.getName(), metadataFile.getName()));
    }

    /**
     * Ensure that the metadata file's last modified timestamp is updated on initialization.
     */
    @Test
    @Feature("TabPersistentStore")
    @MediumTest
    public void testMetadataTimestampRefreshed() throws Exception {
        File baseStateDirectory = TabPersistentStore.getOrCreateBaseStateDirectory();
        Assert.assertNotNull(baseStateDirectory);

        CustomTabTabPersistencePolicy policy = new CustomTabTabPersistencePolicy(2, true);
        File stateDirectory = policy.getOrCreateStateDirectory();
        Assert.assertNotNull(stateDirectory);

        File metadataFile = new File(stateDirectory, policy.getStateFileName());
        Assert.assertTrue(metadataFile.createNewFile());

        long previousTimestamp =
                System.currentTimeMillis() - CustomTabTabPersistencePolicy.STATE_EXPIRY_THRESHOLD;
        Assert.assertTrue(metadataFile.setLastModified(previousTimestamp));

        policy.performInitialization(mSequencedTaskRunner);
        policy.waitForInitializationToFinish();

        Assert.assertTrue(metadataFile.lastModified() > previousTimestamp);
    }

    private static List<File> generateMaximumStateFiles(long currentTime) {
        List<File> validFiles = new ArrayList<>();
        for (int i = 0; i < CustomTabTabPersistencePolicy.MAXIMUM_STATE_FILES; i++) {
            validFiles.add(buildTestFile("testfile" + i, currentTime));
        }
        return validFiles;
    }

    private static File buildTestFile(String filename, final long lastModifiedTime) {
        return new File(filename) {
            @Override
            public long lastModified() {
                return lastModifiedTime;
            }
        };
    }

    private static CustomTabActivity buildTestCustomTabActivity(
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
            public void waitForInitializationToFinish() {
            }

            @Override
            public void setTabContentManager(TabContentManager cache) {
            }

            @Override
            public void setMergeInProgress(boolean isStarted) {
            }

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
            @Nullable
            public List<String> getStateToBeMergedFileNames() {
                return null;
            }

            @Override
            public String getStateFileName() {
                return TabPersistentStore.getStateFileName("cct_testing0");
            }

            @Override
            public File getOrCreateStateDirectory() {
                return new File(
                        TabPersistentStore.getOrCreateBaseStateDirectory(), "cct_tests_zor");
            }

            @Override
            public void notifyStateLoaded(int tabCountAtStartup) {
            }

            @Override
            public void destroy() {
            }

            @Override
            public void cleanupUnusedFiles(Callback<List<String>> filesToDelete) {
            }

            @Override
            public void cancelCleanupInProgress() {
            }
        };
    }

    private static TabModelSelectorImpl buildTestTabModelSelector(
            int[] normalTabIds, int[] incognitoTabIds) {
        MockTabModel.MockTabModelDelegate tabModelDelegate =
                new MockTabModel.MockTabModelDelegate() {
                    @Override
                    public Tab createTab(int id, boolean incognito) {
                        return new MockTab(id, incognito) {
                            @Override
                            public String getUrl() {
                                return "https://www.google.com";
                            }
                        };
                    }
                };
        final MockTabModel normalTabModel = new MockTabModel(false, tabModelDelegate);
        if (normalTabIds != null) {
            for (int tabId : normalTabIds) normalTabModel.addTab(tabId);
        }
        final MockTabModel incognitoTabModel = new MockTabModel(true, tabModelDelegate);
        if (incognitoTabIds != null) {
            for (int tabId : incognitoTabIds) incognitoTabModel.addTab(tabId);
        }

        CustomTabActivity activity = new CustomTabActivity();
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        TabModelSelectorImpl selector = new TabModelSelectorImpl(
                activity, activity, buildTestPersistencePolicy(), false, false, false);
        selector.initializeForTesting(normalTabModel, incognitoTabModel);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.DESTROYED);
        return selector;
    }
}
