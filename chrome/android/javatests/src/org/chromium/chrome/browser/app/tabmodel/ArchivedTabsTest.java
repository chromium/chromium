// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.os.Looper;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator.LeaseReason;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.close_all_tabs.CloseAllTabsHelper;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.ArrayList;
import java.util.List;

/** Tests for ArchivedTabModelOrchestrator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Test interacts with activity shutdown and thus is incompatible with batching")
public class ArchivedTabsTest {
    private static class FakeDeferredStartupHandler extends DeferredStartupHandler {
        private final List<Runnable> mTasks = new ArrayList<>();

        FakeDeferredStartupHandler() {
            super(Looper.getMainLooper().getQueue());
        }

        @Override
        public void addDeferredTask(Runnable task) {
            mTasks.add(task);
        }

        public void runAllTasks() {
            for (Runnable task : mTasks) {
                task.run();
            }
            mTasks.clear();
        }
    }

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    private Profile mProfile;
    private FakeDeferredStartupHandler mDeferredStartupHandler;
    private ArchivedTabModelOrchestrator mOrchestrator;
    private TabModel mArchivedTabModel;
    private TabModel mRegularTabModel;
    private TabCreator mRegularTabCreator;
    private TabArchiveSettings mTabArchiveSettings;

    @Before
    public void setUp() throws Exception {
        mDeferredStartupHandler = new FakeDeferredStartupHandler();
        DeferredStartupHandler.setInstanceForTests(mDeferredStartupHandler);
        mActivityTestRule.startOnBlankPage();

        runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            mActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    mOrchestrator = ArchivedTabModelOrchestrator.getForProfile(mProfile);
                });
        finishLoading();
    }

    private void finishLoading() {
        runOnUiThreadBlocking(
                () -> {
                    mDeferredStartupHandler.runAllTasks();
                    assertThat(mOrchestrator.areTabModelsInitialized()).isTrue();
                    mOrchestrator.getTabArchiveSettings().resetSettingsForTesting();
                    mArchivedTabModel =
                            mOrchestrator.getTabModelSelector().getModel(/* incognito= */ false);
                    mRegularTabModel = mActivityTestRule.getActivity().getCurrentTabModel();
                    mRegularTabCreator =
                            mActivityTestRule.getActivity().getTabCreator(/* incognito= */ false);
                    mTabArchiveSettings = mOrchestrator.getTabArchiveSettings();
                    mTabArchiveSettings.setArchiveEnabled(true);
                });
        CriteriaHelper.pollUiThread(() -> mOrchestrator.isTabStateInitialized());
    }

    private void createArchivedTabAndCloseAllTabs() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        runOnUiThreadBlocking(
                () -> {
                    TabCreator archivedTabCreator = mOrchestrator.getArchivedTabCreatorForTesting();
                    archivedTabCreator.createNewTab(
                            new LoadUrlParams("https://google.com"),
                            TabLaunchType.FROM_RESTORE,
                            /* parent= */ null);
                });
        CriteriaHelper.pollUiThread(() -> 1 == mArchivedTabModel.getCount());
        CriteriaHelper.pollUiThread(() -> 1 == mTabArchiveSettings.getArchivedTabCount());
        CriteriaHelper.pollUiThread(
                () -> 1 == mTabArchiveSettings.getArchivedTabCountSupplier().get());
        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());

        TabUiTestHelper.enterTabSwitcher(cta);
        runOnUiThreadBlocking(
                () -> {
                    TabModelSelector selector = cta.getTabModelSelectorSupplier().get();
                    CloseAllTabsHelper.closeAllTabsHidingTabGroups(
                            selector,
                            () ->
                                    ArchivedTabModelOrchestrator.unarchiveTabsAndGetUndoRunnable(
                                            selector),
                            /* allowUndo= */ true,
                            TabClosingSource.GRID_TAB_SWITCHER,
                            /* closePinnedTabs= */ false);
                });
        CriteriaHelper.pollUiThread(() -> 0 == mArchivedTabModel.getCount());
        CriteriaHelper.pollUiThread(() -> 0 == mTabArchiveSettings.getArchivedTabCount());
        CriteriaHelper.pollUiThread(
                () -> 0 == mTabArchiveSettings.getArchivedTabCountSupplier().get());
        CriteriaHelper.pollUiThread(() -> 0 == mRegularTabModel.getCount());
    }

    @Test
    @MediumTest
    public void testCloseAllTabsAndClickUndo() {
        createArchivedTabAndCloseAllTabs();

        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        CriteriaHelper.pollUiThread(() -> 1 == mArchivedTabModel.getCount());
        CriteriaHelper.pollUiThread(() -> 1 == mTabArchiveSettings.getArchivedTabCount());
        CriteriaHelper.pollUiThread(
                () -> 1 == mTabArchiveSettings.getArchivedTabCountSupplier().get());
        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testCloseAllTabsAndClickUndo_AfterTeardown() {
        createArchivedTabAndCloseAllTabs();

        runOnUiThreadBlocking(() -> mOrchestrator.performTeardownForTesting());
        assertFalse(
                runOnUiThreadBlocking(
                        () -> ArchivedTabModelOrchestrator.isInstantiatedForProfile(mProfile)));

        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        CriteriaHelper.pollUiThread(
                () -> ArchivedTabModelOrchestrator.isInstantiatedForProfile(mProfile));
        CriteriaHelper.pollUiThread(
                () -> {
                    ArchivedTabModelOrchestrator newOrchestrator =
                            ArchivedTabModelOrchestrator.getIfInstantiatedForProfile(mProfile);
                    if (newOrchestrator == null) return false;
                    TabModel newArchivedTabModel = newOrchestrator.getTabModel();
                    return newArchivedTabModel != null && 1 == newArchivedTabModel.getCount();
                });
        CriteriaHelper.pollUiThread(() -> 1 == mTabArchiveSettings.getArchivedTabCount());
        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testOrchestratorTeardownAndEviction() {
        assertTrue(
                runOnUiThreadBlocking(
                        () -> ArchivedTabModelOrchestrator.isInstantiatedForProfile(mProfile)));

        // Acquire lease and then release it.
        Destroyable lease =
                runOnUiThreadBlocking(
                        () ->
                                ArchivedTabModelOrchestrator.acquireLease(
                                        mProfile, LeaseReason.FOR_TESTING));
        assertNotNull(lease);
        runOnUiThreadBlocking(lease::destroy);

        // Perform teardown.
        runOnUiThreadBlocking(() -> mOrchestrator.performTeardownForTesting());

        // Verify orchestrator is evicted from ProfileKeyedMap and no longer instantiated.
        assertFalse(
                runOnUiThreadBlocking(
                        () -> ArchivedTabModelOrchestrator.isInstantiatedForProfile(mProfile)));
        assertNull(
                runOnUiThreadBlocking(
                        () ->
                                TabWindowManagerSingleton.getInstance()
                                        .getArchivedTabModelSelector()));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testOrchestratorReinstantiationOnDemand() {
        // Teardown initial orchestrator.
        runOnUiThreadBlocking(() -> mOrchestrator.performTeardownForTesting());
        assertFalse(
                runOnUiThreadBlocking(
                        () -> ArchivedTabModelOrchestrator.isInstantiatedForProfile(mProfile)));

        // Acquire lease to trigger re-instantiation.
        Destroyable lease =
                runOnUiThreadBlocking(
                        () ->
                                ArchivedTabModelOrchestrator.acquireLease(
                                        mProfile, LeaseReason.FOR_TESTING));
        assertNotNull(lease);

        // Verify orchestrator is re-instantiated.
        assertTrue(
                runOnUiThreadBlocking(
                        () -> ArchivedTabModelOrchestrator.isInstantiatedForProfile(mProfile)));

        // Clean up lease.
        runOnUiThreadBlocking(lease::destroy);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testArchivedTabsPreservedAcrossTeardownAndReinstantiation() {
        // Create an archived tab.
        runOnUiThreadBlocking(
                () -> {
                    TabCreator archivedTabCreator = mOrchestrator.getArchivedTabCreatorForTesting();
                    archivedTabCreator.createNewTab(
                            new LoadUrlParams("https://google.com"),
                            TabLaunchType.FROM_RESTORE,
                            /* parent= */ null);
                });
        CriteriaHelper.pollUiThread(() -> 1 == mArchivedTabModel.getCount());
        assertEquals(1, mTabArchiveSettings.getArchivedTabCount());

        // Teardown the orchestrator.
        runOnUiThreadBlocking(() -> mOrchestrator.performTeardownForTesting());
        assertFalse(
                runOnUiThreadBlocking(
                        () -> ArchivedTabModelOrchestrator.isInstantiatedForProfile(mProfile)));

        // Cached count in SharedPreferences is preserved even when orchestrator is evicted.
        assertEquals(1, mTabArchiveSettings.getArchivedTabCount());

        // Re-instantiate orchestrator on demand and initialize it.
        ArchivedTabModelOrchestrator newOrchestrator =
                runOnUiThreadBlocking(
                        () -> {
                            TabContentManager tabContentManager =
                                    mActivityTestRule.getActivity().getTabContentManager();
                            ArchivedTabModelOrchestrator orchestrator =
                                    ArchivedTabModelOrchestrator.getForProfile(mProfile);
                            orchestrator.maybeCreateAndInitTabModels(
                                    tabContentManager, new CipherFactory());
                            return orchestrator;
                        });

        assertTrue(
                runOnUiThreadBlocking(
                        () -> ArchivedTabModelOrchestrator.isInstantiatedForProfile(mProfile)));

        CriteriaHelper.pollUiThread(() -> newOrchestrator.areTabModelsInitialized());
        TabModel newArchivedModel =
                runOnUiThreadBlocking(
                        () ->
                                newOrchestrator
                                        .getTabModelSelector()
                                        .getModel(/* incognito= */ false));
        CriteriaHelper.pollUiThread(() -> 1 == newArchivedModel.getCount());
    }
}
