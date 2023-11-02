// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabbedModeTabModelOrchestrator;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelSelectorMetadata;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;

/**
 * Tests for the tabbed-mode persisitence policy.
 * TODO: Consider turning this into a unit test after resolving the task involving disk I/O.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabbedModeTabPersistencePolicyTest {
    private static final WebContentsState WEB_CONTENTS_STATE =
            new WebContentsState(ByteBuffer.allocateDirect(100));
    private TestTabModelDirectory mMockDirectory;
    private AdvancedMockContext mAppContext;

    private static final TabModelSelectorFactory sMockTabModelSelectorFactory =
            new TabModelSelectorFactory() {
                @Override
                public TabModelSelector buildSelector(Activity activity,
                        TabCreatorManager tabCreatorManager,
                        NextTabPolicySupplier nextTabPolicySupplier, int selectorIndex) {
                    return new MockTabModelSelector(0, 0, null);
                }
            };

    @Before
    public void setUp() throws Exception {
        TabWindowManagerSingleton.setTabModelSelectorFactoryForTesting(
                sMockTabModelSelectorFactory);
        mAppContext = new AdvancedMockContext(InstrumentationRegistry.getInstrumentation()
                                                      .getTargetContext()
                                                      .getApplicationContext());
        ContextUtils.initApplicationContextForTests(mAppContext);

        mMockDirectory = new TestTabModelDirectory(mAppContext,
                "TabbedModeTabPersistencePolicyTest", TabStateDirectory.TABBED_MODE_DIRECTORY);
        TabStateDirectory.setBaseStateDirectoryForTests(mMockDirectory.getBaseDirectory());
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
            int[] normalTabIds, int[] incognitoTabIds) throws Exception {
        final CallbackHelper callbackSignal = new CallbackHelper();
        final int callCount = callbackSignal.getCallCount();

        MockTabModel.MockTabModelDelegate tabModelDelegate =
                new MockTabModel.MockTabModelDelegate() {
                    @Override
                    public Tab createTab(int id, boolean incognito) {
                        Tab tab = new MockTab(id, incognito) {
                            @Override
                            public GURL getUrl() {
                                return new GURL("https://www.google.com");
                            }
                        };
                        return tab;
                    }
                };

        final MockTabModel normalTabModel = TestThreadUtils.runOnUiThreadBlocking(
                () -> new MockTabModel(false, tabModelDelegate));
        final MockTabModel incognitoTabModel = TestThreadUtils.runOnUiThreadBlocking(
                () -> new MockTabModel(true, tabModelDelegate));
        TabbedModeTabModelOrchestrator orchestrator = TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabbedModeTabModelOrchestrator tmpOrchestrator =
                    new TabbedModeTabModelOrchestrator(false);
            tmpOrchestrator.createTabModels(new ChromeTabbedActivity(), null, null, 0);
            TabModelSelector selector = tmpOrchestrator.getTabModelSelector();
            ((MockTabModelSelector) selector)
                    .initializeTabModels(normalTabModel, incognitoTabModel);
            return tmpOrchestrator;
        });
        TabPersistentStore store = TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabPersistentStore tmpStore = orchestrator.getTabPersistentStoreForTesting();
            tmpStore.addObserver(new TabPersistentStoreObserver() {
                @Override
                public void onMetadataSavedAsynchronously(TabModelSelectorMetadata metadata) {
                    callbackSignal.notifyCalled();
                }
            });
            return tmpStore;
        });

        // Adding tabs results in writing to disk running on AsyncTasks. Run on the main thread
        // to turn the async operations + completion callback into a synchronous operation.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            for (int tabId : normalTabIds) {
                addTabToSaveQueue(store, normalTabModel, normalTabModel.addTab(tabId));
            }
            for (int tabId : incognitoTabIds) {
                addTabToSaveQueue(store, incognitoTabModel, incognitoTabModel.addTab(tabId));
            }
            TabModelUtils.setIndex(normalTabModel, 0, false);
            TabModelUtils.setIndex(incognitoTabModel, 0, false);
        });
        callbackSignal.waitForCallback(callCount);
        return orchestrator;
    }

    private void addTabToSaveQueue(TabPersistentStore store, TabModel tabModel, Tab tab) {
        TabState tabState = new TabState();
        tabState.contentsState = WEB_CONTENTS_STATE;
        TabStateExtractor.setTabStateForTesting(tab.getId(), tabState);
        store.addTabToSaveQueue(tab);
    }

    /**
     * Test the cleanup task path that deletes all the persistent state files for an instance.
     * Ensure tabs not used by other instances only are collected for deletion. This may not
     * be a real scenario likey to happen.
     */
    @Test
    @Feature("TabPersistentStore")
    @MediumTest
    public void testCleanupInstanceState() throws Throwable {
        Assert.assertNotNull(TabStateDirectory.getOrCreateBaseStateDirectory());

        // Delete instance 1. Among the tabs (4, 6, 7) (12, 14, 19), only (4, 12, 14)
        // are not used by any other instances, therefore will be the target for cleanup.
        buildTestTabModelSelector(new int[] {3, 5, 7}, new int[] {11, 13, 17});
        TabbedModeTabModelOrchestrator orchestrator1 =
                buildTestTabModelSelector(new int[] {4, 6, 7}, new int[] {12, 14, 19});
        buildTestTabModelSelector(new int[] {6, 8, 9}, new int[] {15, 18, 19});

        final int id = 1;
        TabPersistencePolicy policy =
                orchestrator1.getTabPersistentStoreForTesting().getTabPersistencePolicyForTesting();
        final CallbackHelper callbackSignal = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            policy.cleanupInstanceState(id, (result) -> {
                Assert.assertThat(result,
                        Matchers.containsInAnyOrder(
                                TabStateFileManager.getTabStateFilename(4, false),
                                TabStateFileManager.getTabStateFilename(12, true),
                                TabStateFileManager.getTabStateFilename(14, true)));
                callbackSignal.notifyCalled();
            });
        });
        callbackSignal.waitForCallback(0);
    }
}
