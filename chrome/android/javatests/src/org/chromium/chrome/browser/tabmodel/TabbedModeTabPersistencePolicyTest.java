// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.util.Batch.UNIT_TESTS;

import android.app.Activity;
import android.content.Context;
import android.util.Pair;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.TabbedModeTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabPersistenceFileInfo.TabStateFileInfo;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabwindow.TabModelSelectorFactory;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;

/**
 * Tests for the tabbed-mode persistence policy. TODO: Consider turning this into a unit test after
 * resolving the task involving disk I/O.
 */
@Batch(UNIT_TESTS)
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabbedModeTabPersistencePolicyTest {
    private static final WebContentsState WEB_CONTENTS_STATE =
            new WebContentsState(
                    ByteBuffer.allocateDirect(100),
                    WebContentsState.CONTENTS_STATE_CURRENT_VERSION);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock ProfileProvider mProfileProvider;
    @Mock Profile mProfile;
    @Mock Profile mIncognitoProfile;
    @Mock ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock ModalDialogManager mModalDialogManager;
    @Mock MismatchedIndicesHandler mMismatchedIndicesHandler;

    private TestTabModelDirectory mMockDirectory;
    private CipherFactory mCipherFactory;

    @Before
    public void setUp() throws Exception {
        TabWindowManagerSingleton.setTabModelSelectorFactoryForTesting(
                new TabModelSelectorFactory() {
                    @Override
                    public TabModelSelector buildTabbedSelector(
                            Context context,
                            ModalDialogManager modalDialogManager,
                            OneshotSupplier<ProfileProvider> profileProviderSupplier,
                            TabCreatorManager tabCreatorManager,
                            NextTabPolicySupplier nextTabPolicySupplier,
                            MultiInstanceManager multiInstanceManager) {
                        return new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null);
                    }

                    @Override
                    public Pair<TabModelSelector, Destroyable> buildHeadlessSelector(
                            @WindowId int windowId, Profile profile) {
                        return Pair.create(null, null);
                    }
                });
        AdvancedMockContext appContext =
                new AdvancedMockContext(
                        InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getApplicationContext());
        ContextUtils.initApplicationContextForTests(appContext);

        mMockDirectory =
                new TestTabModelDirectory(
                        appContext,
                        "TabbedModeTabPersistencePolicyTest",
                        TabStateDirectory.TABBED_MODE_DIRECTORY);
        TabStateDirectory.setBaseStateDirectoryForTests(mMockDirectory.getBaseDirectory());

        mCipherFactory = new CipherFactory();

        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
    }

    @After
    public void tearDown() {
        mMockDirectory.tearDown();

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            activity.finishAndRemoveTask();
        }

        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }

    private TabbedModeTabModelOrchestrator buildTestTabModelSelector(
            int[] normalTabIds, int[] incognitoTabIds, boolean removeTabs) throws Exception {
        final CallbackHelper callbackSignal = new CallbackHelper();
        final int callCount = callbackSignal.getCallCount();

        MockTabModel.MockTabModelDelegate tabModelDelegate =
                new MockTabModel.MockTabModelDelegate() {
                    @Override
                    public MockTab createTab(int id, boolean incognito) {
                        Profile profile = incognito ? mIncognitoProfile : mProfile;
                        MockTab tab =
                                new MockTab(id, profile) {
                                    @Override
                                    public GURL getUrl() {
                                        return new GURL("https://www.google.com");
                                    }
                                };
                        tab.initialize(
                                null, null, null, null, null, null, false, null, false, false);
                        return tab;
                    }
                };

        final MockTabModel normalTabModel =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new MockTabModel(mProfile, tabModelDelegate));
        final MockTabModel incognitoTabModel =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new MockTabModel(mIncognitoProfile, tabModelDelegate));
        TabbedModeTabModelOrchestrator orchestrator =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            OneshotSupplierImpl<ProfileProvider> profileProviderSupplier =
                                    new OneshotSupplierImpl<>();
                            profileProviderSupplier.set(mProfileProvider);
                            TabbedModeTabModelOrchestrator tmpOrchestrator =
                                    new TabbedModeTabModelOrchestrator(
                                            false, mActivityLifecycleDispatcher, mCipherFactory);
                            tmpOrchestrator.createTabModels(
                                    new ChromeTabbedActivity(),
                                    mModalDialogManager,
                                    profileProviderSupplier,
                                    null,
                                    null,
                                    null,
                                    mMismatchedIndicesHandler,
                                    0);
                            TabModelSelector selector = tmpOrchestrator.getTabModelSelector();
                            ((MockTabModelSelector) selector)
                                    .initializeTabModels(normalTabModel, incognitoTabModel);
                            return tmpOrchestrator;
                        });
        TabPersistentStoreImpl store =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            TabPersistentStoreImpl tmpStore =
                                    orchestrator.getTabPersistentStoreForTesting();
                            tmpStore.addObserver(
                                    new TabPersistentStoreObserver() {
                                        @Override
                                        public void onMetadataSavedAsynchronously() {
                                            callbackSignal.notifyCalled();
                                        }
                                    });
                            return tmpStore;
                        });

        // Adding tabs results in writing to disk running on AsyncTasks. Run on the main thread
        // to turn the async operations + completion callback into a synchronous operation.
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            for (int tabId : normalTabIds) {
                                addTabToSaveQueue(
                                        store, normalTabModel, normalTabModel.addTab(tabId));
                            }
                            for (int tabId : incognitoTabIds) {
                                addTabToSaveQueue(
                                        store, incognitoTabModel, incognitoTabModel.addTab(tabId));
                            }
                            TabModelUtils.setIndex(normalTabModel, 0);
                            TabModelUtils.setIndex(incognitoTabModel, 0);
                        });
        callbackSignal.waitForCallback(callCount);
        if (removeTabs) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        removeAllTabs(normalTabModel);
                        removeAllTabs(incognitoTabModel);
                    });
        }
        return orchestrator;
    }

    private void removeAllTabs(MockTabModel tabModel) {
        while (tabModel.getCount() > 0) tabModel.removeTab(tabModel.getTabAt(0));
    }

    private void addTabToSaveQueue(TabPersistentStoreImpl store, TabModel tabModel, Tab tab) {
        TabState tabState = new TabState();
        tabState.contentsState = WEB_CONTENTS_STATE;
        TabStateExtractor.setTabStateForTesting(tab.getId(), tabState);
        store.addTabToSaveQueue(tab);
    }

    /**
     * Test the cleanup task path that deletes all the persistent state files for an instance.
     * Ensure tabs not used by other instances only are collected for deletion. This may not be a
     * real scenario likey to happen.
     */
    @Test
    @Feature("TabPersistentStore")
    @MediumTest
    @DisableFeatures({
        ChromeFeatureList.TAB_WINDOW_MANAGER_REPORT_INDICES_MISMATCH,
        ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH
    })
    public void testCleanupInstanceState() throws Throwable {
        assertNotNull(TabStateDirectory.getOrCreateBaseStateDirectory());

        // Delete instance 1. Among the tabs (4, 6, 7) (12, 14, 19), only (4, 12, 14)
        // are not used by any other instances, therefore will be the target for cleanup.
        //
        // We remove the tabs to simulate that they weren't cleaned up and the instance is not
        // running. A running instance would have had its tabs closed in
        // MultiInstanceManagerApi31#closeWindow already. Failing to do so will throw an
        // IllegalStateException.
        buildTestTabModelSelector(
                new int[] {3, 5, 7}, new int[] {11, 13, 17}, /* removeTabs= */ false);
        TabbedModeTabModelOrchestrator orchestrator1 =
                buildTestTabModelSelector(
                        new int[] {4, 6, 7}, new int[] {12, 14, 19}, /* removeTabs= */ true);
        buildTestTabModelSelector(
                new int[] {6, 8, 9}, new int[] {15, 18, 19}, /* removeTabs= */ false);

        final int id = 1;
        TabPersistencePolicy policy =
                orchestrator1.getTabPersistentStoreForTesting().getTabPersistencePolicyForTesting();
        final CallbackHelper callbackSignal = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    policy.cleanupInstanceState(
                            id,
                            (result) -> {
                                assertThat(
                                        result.getTabStateFileInfos(),
                                        Matchers.containsInAnyOrder(
                                                new TabStateFileInfo(4, false),
                                                new TabStateFileInfo(12, true),
                                                new TabStateFileInfo(14, true)));
                                callbackSignal.notifyCalled();
                            });
                });
        callbackSignal.waitForCallback(0);
    }
}
