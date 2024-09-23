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
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.android.util.concurrent.PausedExecutorService;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridge;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridgeJni;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.state.PersistedTabData;
import org.chromium.chrome.browser.tab.state.PersistedTabDataJni;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelJniBridge;
import org.chromium.chrome.browser.tabmodel.TabModelJniBridgeJni;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;

import java.io.File;
import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicInteger;

/** Tests for TabPersistentStore reacting to events from TabModel and Tab. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(Mode.PAUSED)
@DisableFeatures({
    ChromeFeatureList.ANDROID_TAB_DECLUTTER,
    ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH
})
public class TabPersistentStoreIntegrationTest {
    @Rule public JniMocker jniMocker = new JniMocker();

    private static final int TAB_ID = 42;
    private static final WebContentsState WEB_CONTENTS_STATE =
            new WebContentsState(ByteBuffer.allocateDirect(100));

    private TabbedModeTabModelOrchestrator mOrchestrator;
    private TabModelSelector mTabModelSelector;
    private TabPersistentStore mTabPersistentStore;

    @Mock private ChromeTabbedActivity mChromeActivity;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private ChromeTabCreator mChromeTabCreator;
    @Mock private NextTabPolicySupplier mNextTabPolicySupplier;
    @Mock private MismatchedIndicesHandler mMismatchedIndicesHandler;
    @Mock private TabContentManager mTabContentManager;
    @Mock private Profile mProfile;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private TabModelJniBridge.Natives mTabModelJniBridgeJni;
    @Mock private RecentlyClosedBridge.Natives mRecentlyClosedBridgeJni;
    @Mock private Resources mResources;
    @Mock private PersistedTabData.Natives mPersistedTabDataJni;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

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

        // Pretend native was loaded, creating TabModelImpls.
        OneshotSupplierImpl<ProfileProvider> profileProviderSupplier = new OneshotSupplierImpl<>();
        profileProviderSupplier.set(mProfileProvider);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);

        mOrchestrator =
                new TabbedModeTabModelOrchestrator(
                        /* tabMergingEnabled= */ true,
                        mActivityLifecycleDispatcher,
                        new CipherFactory());
        mOrchestrator.createTabModels(
                mChromeActivity,
                profileProviderSupplier,
                mTabCreatorManager,
                mNextTabPolicySupplier,
                mMismatchedIndicesHandler,
                0);
        mTabModelSelector = mOrchestrator.getTabModelSelector();
        mTabPersistentStore = mOrchestrator.getTabPersistentStore();

        jniMocker.mock(TabModelJniBridgeJni.TEST_HOOKS, mTabModelJniBridgeJni);
        jniMocker.mock(RecentlyClosedBridgeJni.TEST_HOOKS, mRecentlyClosedBridgeJni);
        jniMocker.mock(PersistedTabDataJni.TEST_HOOKS, mPersistedTabDataJni);
        TabTestUtils.mockTabJni(jniMocker);
        mOrchestrator.onNativeLibraryReady(mTabContentManager);
    }

    @After
    public void tearDown() {
        // TabbedModeTabModelOrchestrator gets a new TabModelSelector from TabWindowManagerSingleton
        // for every test case, so TabWindowManagerSingleton has to be reset to avoid running out of
        // assignment slots.
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();

        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testOpenAndCloseTabCreatesAndDeletesFile_tabState() {
        doTestOpenAndCloseTabCreatesAndDeletesFile();
    }

    private void doTestOpenAndCloseTabCreatesAndDeletesFile() {
        // Setup the test: Create a tab
        TabModel tabModel = mTabModelSelector.getModel(false);
        MockTab tab = MockTab.createAndInitialize(TAB_ID, mProfile, TabLaunchType.FROM_CHROME_UI);
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
        tabModel.closeTabs(TabClosureParams.closeTab(tab).build());
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
    public void testUndoTabClosurePersistsState_tabState() {
        doTestUndoTabClosurePersistsState();
    }

    private void doTestUndoTabClosurePersistsState() {
        AtomicInteger timesMetadataSaved = new AtomicInteger();
        observeOnMetadataSavedAsynchronously(timesMetadataSaved);

        // Setup the test: Create a tab and close it
        TabModel tabModel = mTabModelSelector.getModel(false);
        Tab tab = MockTab.createAndInitialize(TAB_ID, mProfile, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        tabModel.closeTabs(TabClosureParams.closeTab(tab).build());
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
    public void testCloseTabPersistsState() {
        AtomicInteger timesMetadataSaved = new AtomicInteger();
        observeOnMetadataSavedAsynchronously(timesMetadataSaved);

        // Setup the test: Create a tab and close it.
        TabModel tabModel = mTabModelSelector.getModel(false);
        Tab tab = MockTab.createAndInitialize(1, mProfile, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        int timesMetadataSavedBefore = timesMetadataSaved.intValue();
        // Step to test: Close tab.
        tabModel.closeTabs(TabClosureParams.closeTab(tab).build());
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
    public void testCloseAllTabsPersistsState() {
        HomepageManager homepageManager = Mockito.mock(HomepageManager.class);
        when(homepageManager.shouldCloseAppWithZeroTabs()).thenReturn(false);
        HomepageManager.setInstanceForTesting(homepageManager);

        AtomicInteger timesMetadataSaved = new AtomicInteger();
        observeOnMetadataSavedAsynchronously(timesMetadataSaved);

        // Setup the test: Create three tabs and close them all.
        TabModel tabModel = mTabModelSelector.getModel(false);
        Tab tab1 = MockTab.createAndInitialize(1, mProfile, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        Tab tab2 = MockTab.createAndInitialize(2, mProfile, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        Tab tab3 = MockTab.createAndInitialize(3, mProfile, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab3, 2, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        int timesMetadataSavedBefore = timesMetadataSaved.intValue();
        // Step to test: Close all tabs.
        tabModel.closeTabs(TabClosureParams.closeAllTabs().build());
        runAllAsyncTasks();

        // Step to test: Commit tabs closure.
        tabModel.commitAllTabClosures();
        runAllAsyncTasks();

        // Verify that metadata was saved.
        // 2 times because close the last tab will trigger selecting a null tab
        // which will trigger another metadata save.
        assertEquals(timesMetadataSavedBefore + 2, timesMetadataSaved.intValue());
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testSelectTabPersistsState() {
        AtomicInteger timesMetadataSaved = new AtomicInteger();
        observeOnMetadataSavedAsynchronously(timesMetadataSaved);

        // Setup the test: Create three tabs
        TabModel tabModel = mTabModelSelector.getModel(false);
        Tab tab1 = MockTab.createAndInitialize(1, mProfile, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        Tab tab2 = MockTab.createAndInitialize(2, mProfile, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        Tab tab3 = MockTab.createAndInitialize(3, mProfile, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab3, 2, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        int timesMetadataSavedBefore = timesMetadataSaved.intValue();
        // Step to test: Select the first tab.
        tabModel.setIndex(0, TabSelectionType.FROM_USER);
        runAllAsyncTasks();

        // Verify that metadata was saved.
        assertEquals(timesMetadataSavedBefore + 1, timesMetadataSaved.intValue());
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testMoveTabPersistsState() {
        AtomicInteger timesMetadataSaved = new AtomicInteger();
        observeOnMetadataSavedAsynchronously(timesMetadataSaved);

        // Setup the test: Create three tabs
        TabModel tabModel = mTabModelSelector.getModel(false);
        Tab tab1 = MockTab.createAndInitialize(1, mProfile, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        Tab tab2 = MockTab.createAndInitialize(2, mProfile, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        Tab tab3 = MockTab.createAndInitialize(3, mProfile, TabLaunchType.FROM_CHROME_UI);
        tabModel.addTab(tab3, 2, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        int timesMetadataSavedBefore = timesMetadataSaved.intValue();
        // Step to test: Move the tab3 to the index 0.
        tabModel.moveTab(3, 0);
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
        TabPersistentStoreObserver observer =
                new TabPersistentStoreObserver() {
                    @Override
                    public void onMetadataSavedAsynchronously(
                            TabModelSelectorMetadata modelSelectorMetadata) {
                        timesMetadataSaved.incrementAndGet();
                    }
                };
        mTabPersistentStore.addObserver(observer);
    }
}
