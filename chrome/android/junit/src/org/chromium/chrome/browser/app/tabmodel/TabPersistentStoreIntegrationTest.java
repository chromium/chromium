// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.res.Resources;
import android.os.Looper;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.android.util.concurrent.PausedExecutorService;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridge;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabImplJni;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelJniBridge;
import org.chromium.chrome.browser.tabmodel.TabModelJniBridgeJni;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.test.util.browser.Features;

import java.io.File;
import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicInteger;

/** Tests for TabPersistentStore reacting to events from TabModel and Tab. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(Mode.PAUSED)
public class TabPersistentStoreIntegrationTest {
    /** Shadow for {@link HomepageManager}. */
    @Implements(HomepageManager.class)
    static class ShadowHomepageManager {
        @Implementation
        public static boolean shouldCloseAppWithZeroTabs() {
            return false;
        }
    }
    @Rule
    public JniMocker jniMocker = new JniMocker();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final int TAB_ID = 42;
    private static final WebContentsState WEB_CONTENTS_STATE =
            new WebContentsState(ByteBuffer.allocateDirect(100));

    private TabbedModeTabModelOrchestrator mOrchestrator;
    private TabModelSelector mTabModelSelector;
    private TabPersistentStore mTabPersistentStore;

    @Mock
    private ChromeTabbedActivity mChromeActivity;
    @Mock
    private TabCreatorManager mTabCreatorManager;
    @Mock
    private ChromeTabCreator mChromeTabCreator;
    @Mock
    private NextTabPolicySupplier mNextTabPolicySupplier;
    @Mock
    private TabContentManager mTabContentManager;
    @Mock
    private Profile mProfile;
    @Mock
    private TabModelJniBridge.Natives mTabModelJniBridgeJni;
    @Mock
    private RecentlyClosedBridge.Natives mRecentlyClosedBridgeJni;
    @Mock
    private Resources mResources;
    @Mock
    private TabImpl.Natives mTabImplJni;

    private PausedExecutorService mExecutor = new PausedExecutorService();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PostTask.setPrenativeThreadPoolExecutorForTesting(mExecutor);

        // Create TabPersistentStore and TabModelSelectorImpl through orchestrator like
        // ChromeActivity does.
        when(mChromeActivity.isInMultiWindowMode()).thenReturn(false);
        when(mChromeActivity.getResources()).thenReturn(mResources);
        when(mResources.getInteger(org.chromium.ui.R.integer.min_screen_width_bucket))
                .thenReturn(1);
        when(mTabCreatorManager.getTabCreator(anyBoolean())).thenReturn(mChromeTabCreator);
        mOrchestrator = new TabbedModeTabModelOrchestrator(/*tabMergingEnabled=*/true);
        mOrchestrator.createTabModels(
                mChromeActivity, mTabCreatorManager, mNextTabPolicySupplier, 0);
        mTabModelSelector = mOrchestrator.getTabModelSelector();
        mTabPersistentStore = mOrchestrator.getTabPersistentStore();

        // Pretend native was loaded, creating TabModelImpls.
        Profile.setLastUsedProfileForTesting(mProfile);
        jniMocker.mock(TabModelJniBridgeJni.TEST_HOOKS, mTabModelJniBridgeJni);
        jniMocker.mock(RecentlyClosedBridgeJni.TEST_HOOKS, mRecentlyClosedBridgeJni);
        jniMocker.mock(TabImplJni.TEST_HOOKS, mTabImplJni);
        mOrchestrator.onNativeLibraryReady(mTabContentManager);
    }

    @After
    public void tearDown() {
        PostTask.resetPrenativeThreadPoolExecutorForTesting();

        // TabbedModeTabModelOrchestrator gets a new TabModelSelector from TabWindowManagerSingleton
        // for every test case, so TabWindowManagerSingleton has to be reset to avoid running out of
        // assignment slots.
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();

        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @Features.DisableFeatures(ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA)
    public void testOpenAndCloseTabCreatesAndDeletesFile_tabState() {
        doTestOpenAndCloseTabCreatesAndDeletesFile();
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @Features.EnableFeatures(ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA)
    public void testOpenAndCloseTabCreatesAndDeletesFile_persistedTabData() {
        doTestOpenAndCloseTabCreatesAndDeletesFile();
    }

    private void doTestOpenAndCloseTabCreatesAndDeletesFile() {
        // Setup the test: Create a tab
        TabModel tabModel = mTabModelSelector.getModel(false);
        MockTab tab =
                (MockTab) MockTab.createAndInitialize(TAB_ID, false, TabLaunchType.FROM_CHROME_UI);
        // Ordinarily, TabState comes from native, so setup a stub in TabStateExtractor.
        TabState tabState = new TabState();
        tabState.contentsState = WEB_CONTENTS_STATE;
        TabStateExtractor.setTabStateForTesting(TAB_ID, tabState);
        tabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        File tabStateFile = mTabPersistentStore.getTabStateFile(TAB_ID, false);
        assertFalse(tabStateFile.exists());

        // Step to test: Load stops
        tab.broadcastOnLoadStopped(false);
        runAllAsyncTasks();

        // Verify tab state was created
        assertTrue(tabStateFile.exists());

        // Close the tab
        tabModel.closeTab(tab, false, false, true);
        runAllAsyncTasks();

        // Step to test: Commit tab closure
        tabModel.commitTabClosure(TAB_ID);
        runAllAsyncTasks();

        // Verify the file was deleted
        assertFalse(tabStateFile.exists());
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @Features.DisableFeatures(ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA)
    public void testUndoTabClosurePersistsState_tabState() {
        doTestUndoTabClosurePersistsState();
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @Features.EnableFeatures(ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA)
    public void testUndoTabClosurePersistsState_persistedTabData() {
        doTestUndoTabClosurePersistsState();
    }

    private void doTestUndoTabClosurePersistsState() {
        AtomicInteger timesMetadataSaved = new AtomicInteger();
        observeOnMetadataSavedAsynchronously(timesMetadataSaved);

        // Setup the test: Create a tab and close it
        TabModel tabModel = mTabModelSelector.getModel(false);
        Tab tab = MockTab.createAndInitialize(TAB_ID, false, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        tabModel.closeTab(tab, false, false, true);
        runAllAsyncTasks();
        int timesMetadataSavedBefore = timesMetadataSaved.intValue();

        // Step to test: Cancel tab closure
        tabModel.cancelTabClosure(TAB_ID);
        runAllAsyncTasks();

        // Verify that metadata was saved
        assertEquals(timesMetadataSavedBefore + 1, timesMetadataSaved.intValue());
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @Features.EnableFeatures(ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA)
    public void testCloseTabPersistsState() {
        AtomicInteger timesMetadataSaved = new AtomicInteger();
        observeOnMetadataSavedAsynchronously(timesMetadataSaved);

        // Setup the test: Create a tab and close it.
        TabModel tabModel = mTabModelSelector.getModel(false);
        Tab tab = MockTab.createAndInitialize(1, false, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        int timesMetadataSavedBefore = timesMetadataSaved.intValue();
        // Step to test: Close tab.
        tabModel.closeTab(tab, false, false, true);
        runAllAsyncTasks();

        // Step to test: Commit tab closure.
        tabModel.commitTabClosure(1);
        runAllAsyncTasks();

        // Verify that metadata was saved.
        assertEquals(timesMetadataSavedBefore + 1, timesMetadataSaved.intValue());
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    @Config(manifest = Config.NONE, shadows = {ShadowHomepageManager.class})
    @Features.EnableFeatures(ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA)
    public void testCloseAllTabsPersistsState() {
        AtomicInteger timesMetadataSaved = new AtomicInteger();
        observeOnMetadataSavedAsynchronously(timesMetadataSaved);

        // Setup the test: Create three tabs and close them all.
        TabModel tabModel = mTabModelSelector.getModel(false);
        Tab tab1 = MockTab.createAndInitialize(1, false, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        Tab tab2 = MockTab.createAndInitialize(2, false, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        Tab tab3 = MockTab.createAndInitialize(3, false, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab3, 2, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        int timesMetadataSavedBefore = timesMetadataSaved.intValue();
        // Step to test: Close all tabs.
        tabModel.closeAllTabs(false);
        runAllAsyncTasks();

        // Step to test: Commit tabs closure.
        tabModel.commitAllTabClosures();
        runAllAsyncTasks();

        // Verify that metadata was saved.
        assertEquals(timesMetadataSavedBefore + 1, timesMetadataSaved.intValue());
    }

    private void runAllAsyncTasks() {
        // Run AsyncTasks
        mExecutor.runAll();

        // Wait for onPostExecute() of the AsyncTasks to run on the UI Thread.
        shadowOf(Looper.getMainLooper()).idle();
    }

    private void observeOnMetadataSavedAsynchronously(AtomicInteger timesMetadataSaved) {
        TabPersistentStoreObserver observer = new TabPersistentStoreObserver() {
            @Override
            public void onMetadataSavedAsynchronously(
                    TabModelSelectorMetadata modelSelectorMetadata) {
                timesMetadataSaved.incrementAndGet();
            }
        };
        mTabPersistentStore.addObserver(observer);
    }
}
