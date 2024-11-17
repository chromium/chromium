// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.Callable;

/** Test that migrating the old tab state folder structure to the new one works. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class RestoreMigrateTest {
    private static final String TEST_DIR = "test";

    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;

    private Context mAppContext;
    private CipherFactory mCipherFactory;

    private void writeStateFile(final TabModelSelector selector, int index) throws IOException {
        TabModelSelectorMetadata data =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<TabModelSelectorMetadata>() {
                            @Override
                            public TabModelSelectorMetadata call() throws Exception {
                                return TabPersistentStore.saveTabModelSelectorMetadata(
                                        selector, null);
                            }
                        });

        File f = TabStateDirectory.getOrCreateTabbedModeStateDirectory();
        TabPersistentStore.saveListToFile(
                new File(f, TabbedModeTabPersistencePolicy.getMetadataFileNameForIndex(index)),
                data);
    }

    private int getMaxId(TabModelSelector selector) {
        int maxId = 0;
        for (TabList list : selector.getModels()) {
            for (int i = 0; i < list.getCount(); i++) {
                maxId = Math.max(maxId, list.getTabAt(i).getId());
            }
        }
        return maxId;
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Mockito.when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);

        mAppContext =
                new AdvancedMockContextWithTestDir(
                        InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getApplicationContext());
        ContextUtils.initApplicationContextForTests(mAppContext);
        TabIdManager.resetInstanceForTesting();

        mCipherFactory = new CipherFactory();
    }

    static class AdvancedMockContextWithTestDir extends AdvancedMockContext {
        private File mFileTestDir;

        AdvancedMockContextWithTestDir(Context base) {
            super(base);
            mFileTestDir = new File(super.getFilesDir(), TEST_DIR);
            mFileTestDir.mkdir();
        }

        @Override
        public File getFilesDir() {
            return mFileTestDir;
        }
    }

    @After
    public void tearDown() {
        FileUtils.recursivelyDeleteFile(mAppContext.getFilesDir(), null);
        FileUtils.recursivelyDeleteFile(
                TabStateDirectory.getOrCreateTabbedModeStateDirectory(), null);
        TabStateDirectory.resetTabbedModeStateDirectoryForTesting();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TABMODEL_HAS_RUN_FILE_MIGRATION, false);
        TabbedModeTabPersistencePolicy.resetMigrationTaskForTesting();
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }

    private TabPersistentStore buildTabPersistentStore(
            final TabModelSelector selector, final int selectorIndex) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabPersistencePolicy persistencePolicy =
                            new TabbedModeTabPersistencePolicy(selectorIndex, false, true);
                    TabPersistentStore store =
                            new TabPersistentStore(
                                    TabPersistentStore.CLIENT_TAG_REGULAR,
                                    persistencePolicy,
                                    selector,
                                    null,
                                    TabWindowManagerSingleton.getInstance(),
                                    mCipherFactory);
                    return store;
                });
    }

    /** Test that normal migration of state files works. */
    @Test
    @SuppressWarnings("unused")
    @SmallTest
    @Feature({"TabPersistentStore"})
    @UiThreadTest
    public void testMigrateData() throws IOException {
        // Write old state files.
        File filesDir = mAppContext.getFilesDir();
        File stateFile = new File(filesDir, TabbedModeTabPersistencePolicy.LEGACY_SAVED_STATE_FILE);
        File tab0 = new File(filesDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File tab1 = new File(filesDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX + "1");
        File tab2 =
                new File(filesDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "2");
        File tab3 =
                new File(filesDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "3");

        Assert.assertTrue("Could not create state file", stateFile.createNewFile());
        Assert.assertTrue("Could not create tab 0 file", tab0.createNewFile());
        Assert.assertTrue("Could not create tab 1 file", tab1.createNewFile());
        Assert.assertTrue("Could not create tab 2 file", tab2.createNewFile());
        Assert.assertTrue("Could not create tab 3 file", tab3.createNewFile());

        // Build the TabPersistentStore which will try to move the files.
        MockTabModelSelector selector =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null);
        TabPersistentStore store = buildTabPersistentStore(selector, 0);
        store.waitForMigrationToFinish();

        // Make sure we don't hit the migration path again.
        Assert.assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.TABMODEL_HAS_RUN_FILE_MIGRATION, false));

        // Check that the files were moved.
        File newDir = TabStateDirectory.getOrCreateTabbedModeStateDirectory();
        File newStateFile =
                new File(newDir, TabbedModeTabPersistencePolicy.getMetadataFileNameForIndex(0));
        File newTab0 = new File(newDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File newTab1 = new File(newDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX + "1");
        File newTab2 =
                new File(newDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "2");
        File newTab3 =
                new File(newDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "3");

        Assert.assertTrue("Could not find new state file", newStateFile.exists());
        Assert.assertTrue("Could not find new tab 0 file", newTab0.exists());
        Assert.assertTrue("Could not find new tab 1 file", newTab1.exists());
        Assert.assertTrue("Could not find new tab 2 file", newTab2.exists());
        Assert.assertTrue("Could not find new tab 3 file", newTab3.exists());

        Assert.assertFalse("Could still find old state file", stateFile.exists());
        Assert.assertFalse("Could still find old tab 0 file", tab0.exists());
        Assert.assertFalse("Could still find old tab 1 file", tab1.exists());
        Assert.assertFalse("Could still find old tab 2 file", tab2.exists());
        Assert.assertFalse("Could still find old tab 3 file", tab3.exists());
    }

    /** Test that migration skips if it already has files in the new folder. */
    @Test
    @SuppressWarnings("unused")
    @SmallTest
    @Feature({"TabPersistentStore"})
    @UiThreadTest
    public void testSkipMigrateData() throws IOException {
        // Write old state files.
        File filesDir = mAppContext.getFilesDir();
        File stateFile = new File(filesDir, TabbedModeTabPersistencePolicy.LEGACY_SAVED_STATE_FILE);
        File tab0 = new File(filesDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File tab1 = new File(filesDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX + "1");
        File tab2 =
                new File(filesDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "2");
        File tab3 =
                new File(filesDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "3");

        Assert.assertTrue("Could not create state file", stateFile.createNewFile());
        Assert.assertTrue("Could not create tab 0 file", tab0.createNewFile());
        Assert.assertTrue("Could not create tab 1 file", tab1.createNewFile());
        Assert.assertTrue("Could not create tab 2 file", tab2.createNewFile());
        Assert.assertTrue("Could not create tab 3 file", tab3.createNewFile());

        // Write new state files
        File newDir = TabStateDirectory.getOrCreateTabbedModeStateDirectory();
        File newStateFile =
                new File(newDir, TabbedModeTabPersistencePolicy.getMetadataFileNameForIndex(0));
        File newTab4 = new File(newDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX + "4");

        Assert.assertTrue("Could not create new tab 4 file", newTab4.createNewFile());
        Assert.assertTrue("Could not create new state file", newStateFile.createNewFile());

        // Build the TabPersistentStore which will try to move the files.
        MockTabModelSelector selector =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null);
        TabPersistentStore store = buildTabPersistentStore(selector, 0);
        store.waitForMigrationToFinish();

        Assert.assertTrue("Could not find new state file", newStateFile.exists());
        Assert.assertTrue("Could not find new tab 4 file", newTab4.exists());

        // Make sure the old files did not move
        File newTab0 = new File(newDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File newTab1 = new File(newDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX + "1");
        File newTab2 =
                new File(newDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "2");
        File newTab3 =
                new File(newDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "3");

        Assert.assertFalse("Could find new tab 0 file", newTab0.exists());
        Assert.assertFalse("Could find new tab 1 file", newTab1.exists());
        Assert.assertFalse("Could find new tab 2 file", newTab2.exists());
        Assert.assertFalse("Could find new tab 3 file", newTab3.exists());
    }

    /** Test that the state file migration skips unrelated files. */
    @Test
    @SuppressWarnings("unused")
    @SmallTest
    @Feature({"TabPersistentStore"})
    @UiThreadTest
    public void testMigrationLeavesOtherFilesAlone() throws IOException {
        // Write old state files.
        File filesDir = mAppContext.getFilesDir();
        File stateFile = new File(filesDir, TabbedModeTabPersistencePolicy.LEGACY_SAVED_STATE_FILE);
        File tab0 = new File(filesDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File otherFile = new File(filesDir, "other.file");

        Assert.assertTrue("Could not create state file", stateFile.createNewFile());
        Assert.assertTrue("Could not create tab 0 file", tab0.createNewFile());
        Assert.assertTrue("Could not create other file", otherFile.createNewFile());

        // Build the TabPersistentStore which will try to move the files.
        MockTabModelSelector selector =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null);
        TabPersistentStore store = buildTabPersistentStore(selector, 0);
        store.waitForMigrationToFinish();

        Assert.assertFalse("Could still find old state file", stateFile.exists());
        Assert.assertFalse("Could still find old tab 0 file", tab0.exists());
        Assert.assertTrue("Could not find other file", otherFile.exists());

        // Check that the files were moved.
        File newDir = TabStateDirectory.getOrCreateTabbedModeStateDirectory();
        File newStateFile =
                new File(newDir, TabbedModeTabPersistencePolicy.getMetadataFileNameForIndex(0));
        File newTab0 = new File(newDir, TabStateFileManager.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File newOtherFile = new File(newDir, "other.file");

        Assert.assertTrue("Could not find new state file", newStateFile.exists());
        Assert.assertTrue("Could not find new tab 0 file", newTab0.exists());
        Assert.assertFalse("Could find new other file", newOtherFile.exists());
    }

    /** Tests that the max id returned is the max of all of the tab models. */
    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @UiThreadTest
    public void testFindsMaxIdProperly() throws IOException {
        TabModelSelector selector0 =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 1, 1, null);
        TabModelSelector selector1 =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 1, 1, null);

        writeStateFile(selector0, 0);
        writeStateFile(selector1, 1);

        TabModelSelector selectorIn =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null);
        TabPersistentStore storeIn = buildTabPersistentStore(selectorIn, 0);

        int maxId = Math.max(getMaxId(selector0), getMaxId(selector1));
        storeIn.loadState(/* ignoreIncognitoFiles= */ false);
        Assert.assertEquals(
                "Invalid next id",
                maxId + 1,
                TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID));
    }

    /**
     * Tests that each model loads the subset of tabs it is responsible for. In this case, just
     * check that the model has the expected number of tabs to load. Since each model is loading a
     * different number of tabs we can tell if they are each attempting to load their specific set.
     */
    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @UiThreadTest
    public void testOnlyLoadsSingleModel() throws IOException {
        TabModelSelector selector0 =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 3, 3, null);
        TabModelSelector selector1 =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 2, 1, null);

        writeStateFile(selector0, 0);
        writeStateFile(selector1, 1);

        TabModelSelector selectorIn0 =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null);
        TabModelSelector selectorIn1 =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null);

        TabPersistentStore storeIn0 = buildTabPersistentStore(selectorIn0, 0);

        TabPersistentStore storeIn1 = buildTabPersistentStore(selectorIn1, 1);

        storeIn0.loadState(/* ignoreIncognitoFiles= */ false);
        storeIn1.loadState(/* ignoreIncognitoFiles= */ false);

        Assert.assertEquals("Unexpected number of tabs to load", 6, storeIn0.getRestoredTabCount());
        Assert.assertEquals("Unexpected number of tabs to load", 3, storeIn1.getRestoredTabCount());
    }
}
