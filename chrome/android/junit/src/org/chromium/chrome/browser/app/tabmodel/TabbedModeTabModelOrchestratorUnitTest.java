// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.util.Pair;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridge;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelJniBridge;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelJniBridgeJni;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.io.DataInputStream;
import java.util.List;
import java.util.function.Supplier;

/** Tests for {@link TabbedModeTabModelOrchestrator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabbedModeTabModelOrchestratorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ChromeTabbedActivity mChromeActivity;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private Profile mProfile;
    @Mock private NextTabPolicySupplier mNextTabPolicySupplier;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private MismatchedIndicesHandler mMismatchedIndicesHandler;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    @Mock private TabContentManager mTabContentManager;
    @Mock private DeferredStartupHandler mDeferredStartupHandler;
    @Mock private TabModelJniBridge.Natives mTabModelJniBridgeJni;
    @Mock private RecentlyClosedBridge.Natives mRecentlyClosedBridgeJni;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabModelSelectorBase mTabModelSelector;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabModel mTabModel;
    @Mock private TabStateStorageService mTabStateStorageService;
    @Captor private ArgumentCaptor<Runnable> mRunnableCaptor;
    @Captor private ArgumentCaptor<Supplier<TabModel>> mSupplierCaptor;

    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();
    private CipherFactory mCipherFactory;

    // TabbedModeTabModelOrchestrator running on Android S where tab merging into other instance
    // is not performed.
    private class TabbedModeTabModelOrchestratorApi31 extends TabbedModeTabModelOrchestrator {
        public TabbedModeTabModelOrchestratorApi31() {
            super(/* tabMergingEnabled= */ false, mActivityLifecycleDispatcher, mCipherFactory);
        }

        @Override
        protected boolean isMultiInstanceApi31Enabled() {
            return true;
        }
    }

    @Before
    public void setUp() {
        mProfileProviderSupplier.set(mProfileProvider);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        mCipherFactory = new CipherFactory();
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        TabModelJniBridgeJni.setInstanceForTesting(mTabModelJniBridgeJni);
        RecentlyClosedBridgeJni.setInstanceForTesting(mRecentlyClosedBridgeJni);
        when(mRecentlyClosedBridgeJni.init(any(), any())).thenReturn(1L);
        TabStateStorageServiceFactory.setForTesting(mTabStateStorageService);
    }

    @After
    public void tearDown() {
        // TabbedModeTabModelOrchestrator gets a new TabModelSelector from TabWindowManagerSingleton
        // for every test case, so TabWindowManagerSingleton has to be reset to avoid running out of
        // assignment slots.
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        MultiWindowTestUtils.resetInstanceInfo();
    }

    @Test
    @Feature({"TabPersistentStore"})
    public void testMergeTabsOnStartupAfterUpgradeToMultiInstanceSupport() {
        // If there is no instance, this is the first startup since upgrading to multi-instance-
        // supported version. Any tab state file left in the previous version should be
        // taken into account so as not to lose tabs in it.
        assertEquals(0, MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY));
        TabbedModeTabModelOrchestrator orchestrator = new TabbedModeTabModelOrchestratorApi31();
        orchestrator.createTabModels(
                mChromeActivity,
                mModalDialogManager,
                mProfileProviderSupplier,
                mTabCreatorManager,
                mNextTabPolicySupplier,
                mMultiInstanceManager,
                mMismatchedIndicesHandler,
                0);
        List<Pair<AsyncTask<DataInputStream>, String>> tabStatesToMerge;
        TabPersistentStoreImpl tabPersistentStore =
                (TabPersistentStoreImpl) orchestrator.getTabPersistentStore();
        tabStatesToMerge = tabPersistentStore.getTabListToMergeTasksForTesting();
        assertFalse("Should have a tab state file to merge", tabStatesToMerge.isEmpty());

        MultiWindowTestUtils.createInstance(/* instanceId= */ 0, "https://url.com", 1, 57);
        assertEquals(1, MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY));

        // Once an instance is created, no more merging is allowed.
        orchestrator = new TabbedModeTabModelOrchestratorApi31();
        orchestrator.createTabModels(
                mChromeActivity,
                mModalDialogManager,
                mProfileProviderSupplier,
                mTabCreatorManager,
                mNextTabPolicySupplier,
                mMultiInstanceManager,
                mMismatchedIndicesHandler,
                1);
        tabPersistentStore = (TabPersistentStoreImpl) orchestrator.getTabPersistentStore();
        tabStatesToMerge = tabPersistentStore.getTabListToMergeTasksForTesting();
        assertTrue("Should not have any tab state file to merge", tabStatesToMerge.isEmpty());
    }

    @Test
    public void testDestroy() {
        when(mTabModelSelector.getModel(anyBoolean())).thenReturn(mTabModel);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabWindowManager.requestSelector(
                        any(), any(), any(), any(), any(), any(), any(), anyInt()))
                .thenReturn(new Pair<>(0, mTabModelSelector));
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        ArchivedTabModelOrchestrator.setInstanceForTesting(mArchivedTabModelOrchestrator);
        DeferredStartupHandler.setInstanceForTests(mDeferredStartupHandler);

        TabbedModeTabModelOrchestrator orchestrator = new TabbedModeTabModelOrchestratorApi31();
        orchestrator.createTabModels(
                mChromeActivity,
                mModalDialogManager,
                mProfileProviderSupplier,
                mTabCreatorManager,
                mNextTabPolicySupplier,
                mMultiInstanceManager,
                mMismatchedIndicesHandler,
                0);
        orchestrator.onNativeLibraryReady(mTabContentManager);
        verify(mDeferredStartupHandler).addDeferredTask(mRunnableCaptor.capture());

        mRunnableCaptor.getValue().run();
        verify(mArchivedTabModelOrchestrator)
                .initializeHistoricalTabModelObserver(mSupplierCaptor.capture());

        orchestrator.destroy();
        verify(mArchivedTabModelOrchestrator)
                .removeHistoricalTabModelObserver(mSupplierCaptor.getValue());
    }
}
