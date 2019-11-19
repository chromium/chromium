// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.StreamUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.Callable;

/**
 * Test that migrating the old tab state folder structure to the new one works.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class RestoreMigrateTest {
    private Context mAppContext;

    private void writeStateFile(final TabModelSelector selector, int index) throws IOException {
        byte[] data =
                TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<byte[]>() {
                    @Override
                    public byte[] call() throws Exception {
                        return TabPersistentStore.serializeTabModelSelector(selector, null)
                                .listData;
                    }
                });
        File f = TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory();
        FileOutputStream fos = null;
        try {
            fos = new FileOutputStream(new File(
                    f, TabbedModeTabPersistencePolicy.getStateFileName(index)));
            fos.write(data);
        } finally {
            StreamUtil.closeQuietly(fos);
        }
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

    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    @Before
    public void setUp() {
        mAppContext = new AdvancedMockContext(InstrumentationRegistry.getInstrumentation()
                                                      .getTargetContext()
                                                      .getApplicationContext());
        ContextUtils.initApplicationContextForTests(mAppContext);
    }

    private TabPersistentStore buildTabPersistentStore(
            final TabModelSelector selector, final int selectorIndex) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<TabPersistentStore>() {
            @Override
            public TabPersistentStore call() {
                TabPersistencePolicy persistencePolicy = new TabbedModeTabPersistencePolicy(
                        selectorIndex, false);
                TabPersistentStore store = new TabPersistentStore(
                        persistencePolicy, selector, null, null);
                return store;
            }
        });
    }

    /**
     * Test that normal migration of state files works.
     * @throws IOException
     */
    @Test
    @SuppressWarnings("unused")
    @SmallTest
    @Feature({"TabPersistentStore"})
    @UiThreadTest
    public void testMigrateData() throws IOException {
        // Write old state files.
        File filesDir = mAppContext.getFilesDir();
        File stateFile = new File(filesDir, TabbedModeTabPersistencePolicy.LEGACY_SAVED_STATE_FILE);
        File tab0 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File tab1 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "1");
        File tab2 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "2");
        File tab3 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "3");

        Assert.assertTrue("Could not create state file", stateFile.createNewFile());
        Assert.assertTrue("Could not create tab 0 file", tab0.createNewFile());
        Assert.assertTrue("Could not create tab 1 file", tab1.createNewFile());
        Assert.assertTrue("Could not create tab 2 file", tab2.createNewFile());
        Assert.assertTrue("Could not create tab 3 file", tab3.createNewFile());

        // Build the TabPersistentStore which will try to move the files.
        MockTabModelSelector selector = new MockTabModelSelector(0, 0, null);
        TabPersistentStore store = buildTabPersistentStore(selector, 0);
        store.waitForMigrationToFinish();

        // Make sure we don't hit the migration path again.
        Assert.assertTrue(ContextUtils.getAppSharedPreferences().getBoolean(
                TabbedModeTabPersistencePolicy.PREF_HAS_RUN_FILE_MIGRATION, false));

        // Check that the files were moved.
        File newDir = TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory();
        File newStateFile = new File(newDir, TabbedModeTabPersistencePolicy.getStateFileName(0));
        File newTab0 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File newTab1 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "1");
        File newTab2 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "2");
        File newTab3 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "3");

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

    /**
     * Test that migration skips if it already has files in the new folder.
     * @throws IOException
     */
    @Test
    @SuppressWarnings("unused")
    @SmallTest
    @Feature({"TabPersistentStore"})
    @UiThreadTest
    public void testSkipMigrateData() throws IOException {
        // Write old state files.
        File filesDir = mAppContext.getFilesDir();
        File stateFile = new File(filesDir, TabbedModeTabPersistencePolicy.LEGACY_SAVED_STATE_FILE);
        File tab0 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File tab1 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "1");
        File tab2 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "2");
        File tab3 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "3");

        Assert.assertTrue("Could not create state file", stateFile.createNewFile());
        Assert.assertTrue("Could not create tab 0 file", tab0.createNewFile());
        Assert.assertTrue("Could not create tab 1 file", tab1.createNewFile());
        Assert.assertTrue("Could not create tab 2 file", tab2.createNewFile());
        Assert.assertTrue("Could not create tab 3 file", tab3.createNewFile());

        // Write new state files
        File newDir = TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory();
        File newStateFile = new File(newDir, TabbedModeTabPersistencePolicy.getStateFileName(0));
        File newTab4 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "4");

        Assert.assertTrue("Could not create new tab 4 file", newTab4.createNewFile());
        Assert.assertTrue("Could not create new state file", newStateFile.createNewFile());

        // Build the TabPersistentStore which will try to move the files.
        MockTabModelSelector selector = new MockTabModelSelector(0, 0, null);
        TabPersistentStore store = buildTabPersistentStore(selector, 0);
        store.waitForMigrationToFinish();

        Assert.assertTrue("Could not find new state file", newStateFile.exists());
        Assert.assertTrue("Could not find new tab 4 file", newTab4.exists());

        // Make sure the old files did not move
        File newTab0 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File newTab1 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "1");
        File newTab2 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "2");
        File newTab3 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX_INCOGNITO + "3");

        Assert.assertFalse("Could find new tab 0 file", newTab0.exists());
        Assert.assertFalse("Could find new tab 1 file", newTab1.exists());
        Assert.assertFalse("Could find new tab 2 file", newTab2.exists());
        Assert.assertFalse("Could find new tab 3 file", newTab3.exists());
    }

    /**
     * Test that the state file migration skips unrelated files.
     * @throws IOException
     */
    @Test
    @SuppressWarnings("unused")
    @SmallTest
    @Feature({"TabPersistentStore"})
    @UiThreadTest
    public void testMigrationLeavesOtherFilesAlone() throws IOException {
        // Write old state files.
        File filesDir = mAppContext.getFilesDir();
        File stateFile = new File(filesDir, TabbedModeTabPersistencePolicy.LEGACY_SAVED_STATE_FILE);
        File tab0 = new File(filesDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File otherFile = new File(filesDir, "other.file");

        Assert.assertTrue("Could not create state file", stateFile.createNewFile());
        Assert.assertTrue("Could not create tab 0 file", tab0.createNewFile());
        Assert.assertTrue("Could not create other file", otherFile.createNewFile());

        // Build the TabPersistentStore which will try to move the files.
        MockTabModelSelector selector = new MockTabModelSelector(0, 0, null);
        TabPersistentStore store = buildTabPersistentStore(selector, 0);
        store.waitForMigrationToFinish();

        Assert.assertFalse("Could still find old state file", stateFile.exists());
        Assert.assertFalse("Could still find old tab 0 file", tab0.exists());
        Assert.assertTrue("Could not find other file", otherFile.exists());

        // Check that the files were moved.
        File newDir = TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory();
        File newStateFile = new File(newDir, TabbedModeTabPersistencePolicy.getStateFileName(0));
        File newTab0 = new File(newDir, TabState.SAVED_TAB_STATE_FILE_PREFIX + "0");
        File newOtherFile = new File(newDir, "other.file");

        Assert.assertTrue("Could not find new state file", newStateFile.exists());
        Assert.assertTrue("Could not find new tab 0 file", newTab0.exists());
        Assert.assertFalse("Could find new other file", newOtherFile.exists());
    }

    /**
     * Tests that the max id returned is the max of all of the tab models.
     * @throws IOException
     */
    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @UiThreadTest
    @RetryOnFailure
    public void testFindsMaxIdProperly() throws IOException {
        TabModelSelector selector0 = new MockTabModelSelector(1, 1, null);
        TabModelSelector selector1 = new MockTabModelSelector(1, 1, null);

        writeStateFile(selector0, 0);
        writeStateFile(selector1, 1);

        TabModelSelector selectorIn = new MockTabModelSelector(0, 0, null);
        TabPersistentStore storeIn = buildTabPersistentStore(selectorIn, 0);

        int maxId = Math.max(getMaxId(selector0), getMaxId(selector1));
        try {
            RecordHistogram.setDisabledForTests(true);
            storeIn.loadState(false /* ignoreIncognitoFiles */);
            Assert.assertEquals("Invalid next id", maxId + 1,
                    TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID));
        } finally {
            RecordHistogram.setDisabledForTests(false);
        }
    }

    /**
     * Tests that each model loads the subset of tabs it is responsible for.  In this case, just
     * check that the model has the expected number of tabs to load.  Since each model is loading
     * a different number of tabs we can tell if they are each attempting to load their specific
     * set.
     * @throws IOException
     */
    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @UiThreadTest
    public void testOnlyLoadsSingleModel() throws IOException {
        TabModelSelector selector0 = new MockTabModelSelector(3, 3, null);
        TabModelSelector selector1 = new MockTabModelSelector(2, 1, null);

        writeStateFile(selector0, 0);
        writeStateFile(selector1, 1);

        TabModelSelector selectorIn0 = new MockTabModelSelector(0, 0, null);
        TabModelSelector selectorIn1 = new MockTabModelSelector(0, 0, null);

        TabPersistentStore storeIn0 = buildTabPersistentStore(selectorIn0, 0);

        TabPersistentStore storeIn1 = buildTabPersistentStore(selectorIn1, 1);

        try {
            RecordHistogram.setDisabledForTests(true);
            storeIn0.loadState(false /* ignoreIncognitoFiles */);
            storeIn1.loadState(false /* ignoreIncognitoFiles */);
        } finally {
            RecordHistogram.setDisabledForTests(false);
        }

        Assert.assertEquals("Unexpected number of tabs to load", 6, storeIn0.getRestoredTabCount());
        Assert.assertEquals("Unexpected number of tabs to load", 3, storeIn1.getRestoredTabCount());
    }
}
