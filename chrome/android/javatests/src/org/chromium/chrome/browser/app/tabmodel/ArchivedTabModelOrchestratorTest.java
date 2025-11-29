// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.tabmodel.TabList.INVALID_TAB_INDEX;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import android.os.Looper;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabArchiverImpl;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.hub.TabSwitcherSearchStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherSearchStation.SuggestionFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for ArchivedTabModelOrchestrator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Test interacts with activity shutdown and thus is incompatible with batching")
@EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH})
@DisableFeatures({
    ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_ALL_BUT_ACTIVE,
    ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS
})
public class ArchivedTabModelOrchestratorTest {
    private static final String TEST_PATH = "/chrome/test/data/android/about.html";
    private static final String TEST_PATH_2 = "/chrome/test/data/android/google.html";
    private static final String SYNC_GROUP_ID1 = "test_sync_group_id1";

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

    @Mock private ArchivedTabModelOrchestrator.Observer mObserver;
    @Mock private TabArchiverImpl.Clock mClock;
    @Mock private TabPersistentStore mArchivedTabPersistentStore;
    @Mock private TabPersistentStore mNormalTabPersistentStore;
    @Mock private TabModelSelectorBase mTabModelSelector;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private VersioningMessageController mVersioningMessageController;

    private Profile mProfile;
    private FakeDeferredStartupHandler mDeferredStartupHandler;
    private ArchivedTabModelOrchestrator mOrchestrator;
    private TabbedModeTabModelOrchestrator mTabbedModeOrchestrator;
    private TabModel mArchivedTabModel;
    private TabModel mRegularTabModel;
    private TabCreator mRegularTabCreator;
    private TabArchiveSettings mTabArchiveSettings;
    private WebPageStation mPage;
    private final TabPersistencePolicy mTabPersistencePolicy =
            new TabbedModeTabPersistencePolicy(0, false, false);

    @Before
    public void setUp() throws Exception {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        when(mTabGroupSyncService.getVersioningMessageController())
                .thenReturn(mVersioningMessageController);

        mDeferredStartupHandler = new FakeDeferredStartupHandler();
        DeferredStartupHandler.setInstanceForTests(mDeferredStartupHandler);
        mPage = mActivityTestRule.startOnBlankPage();

        runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            mActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();

                    mTabbedModeOrchestrator =
                            (TabbedModeTabModelOrchestrator)
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelOrchestratorSupplier()
                                            .get();
                    mOrchestrator = ArchivedTabModelOrchestrator.getForProfile(mProfile);
                });
    }

    private void finishLoading() {
        runOnUiThreadBlocking(
                () -> {
                    mDeferredStartupHandler.runAllTasks();
                    assertThat(mOrchestrator.areTabModelsInitialized()).isTrue();
                    mOrchestrator.getTabArchiveSettings().resetSettingsForTesting();
                    mArchivedTabModel = mOrchestrator.getTabModelSelector().getModel(false);
                    mRegularTabModel = mActivityTestRule.getActivity().getCurrentTabModel();
                    mRegularTabCreator = mActivityTestRule.getActivity().getTabCreator(false);
                    mTabArchiveSettings = mOrchestrator.getTabArchiveSettings();
                    // Sets archive to disabled during startup to prevent the initial scheduled
                    // declutter pass to no-op.
                    mTabArchiveSettings.setArchiveEnabled(false);
                });
    }

    private void setupDeclutterSettingsForTest() {
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiveSettings.setArchiveEnabled(true);
                    mTabArchiveSettings.setArchiveTimeDeltaHours(0);
                });
    }

    @Test
    @MediumTest
    public void testDeferredInitialization() {
        assertFalse(mOrchestrator.areTabModelsInitialized());
        runOnUiThreadBlocking(() -> mOrchestrator.addObserver(mObserver));
        finishLoading();
        assertTrue(mOrchestrator.areTabModelsInitialized());
        verify(mObserver).onTabModelCreated(any());
    }

    @Test
    @MediumTest
    public void testBeginDeclutter_DisablesAndEnablesArchivedSaveTabListTask() {
        finishLoading();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        setupDeclutterSettingsForTest();
        runOnUiThreadBlocking(
                () -> {
                    TabbedModeTabModelOrchestrator normalOrchestrator =
                            (TabbedModeTabModelOrchestrator)
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelOrchestratorSupplier()
                                            .get();
                    normalOrchestrator.initForTesting(
                            normalOrchestrator.getTabModelSelector(),
                            mNormalTabPersistentStore,
                            mTabPersistencePolicy);
                    mOrchestrator.initForTesting(
                            mOrchestrator.getTabModelSelector(),
                            mArchivedTabPersistentStore,
                            mTabPersistencePolicy);
                    mOrchestrator.doDeclutterPass(normalOrchestrator);
                });

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));

        verify(mArchivedTabPersistentStore).pauseSaveTabList();
        verify(mArchivedTabPersistentStore).resumeSaveTabList();
        verify(mNormalTabPersistentStore).pauseSaveTabList();
        verify(mNormalTabPersistentStore).resumeSaveTabList();
    }

    @Test
    @MediumTest
    public void testDeclutterInactiveTabs() {
        finishLoading();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));

        setupDeclutterSettingsForTest();
        runOnUiThreadBlocking(
                () -> {
                    doReturn(TimeUnit.HOURS.toMillis(2)).when(mClock).currentTimeMillis();
                    ((TabArchiverImpl) mOrchestrator.getTabArchiver()).setClockForTesting(mClock);
                    mRegularTabModel.getTabAt(0).setTimestampMillis(0L);
                    mRegularTabModel.getTabAt(1).setTimestampMillis(0L);
                    mTabArchiveSettings.setArchiveTimeDeltaHours(1);

                    mOrchestrator.doDeclutterPass(
                            (TabbedModeTabModelOrchestrator)
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelOrchestratorSupplier()
                                            .get());
                });

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));
        runOnUiThreadBlocking(
                () ->
                        mOrchestrator
                                .getTabArchiver()
                                .unarchiveAndRestoreTabs(
                                        mRegularTabCreator,
                                        Arrays.asList(mArchivedTabModel.getTabAt(0)),
                                        /* updateTimestamp= */ true,
                                        /* areTabsBeingOpened= */ false));

        // Now the timestamp has been updated, no tabs should get archived.
        runOnUiThreadBlocking(
                () -> {
                    mOrchestrator.doDeclutterPass(
                            (TabbedModeTabModelOrchestrator)
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelOrchestratorSupplier()
                                            .get());
                });
        CriteriaHelper.pollUiThread(() -> 2 == mRegularTabModel.getCount());
        CriteriaHelper.pollUiThread(() -> 0 == mArchivedTabModel.getCount());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_ALL_BUT_ACTIVE})
    public void testArchiveAllButActive() {
        finishLoading();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        setupDeclutterSettingsForTest();
        runOnUiThreadBlocking(
                () ->
                        mOrchestrator.doDeclutterPass(
                                (TabbedModeTabModelOrchestrator)
                                        mActivityTestRule
                                                .getActivity()
                                                .getTabModelOrchestratorSupplier()
                                                .get()));
        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));
    }

    @Test
    @MediumTest
    public void testRescueTabs_FeatureFlag() {
        setupSavedTabGroup();
        finishLoading();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));

        setupDeclutterSettingsForTest();
        runOnUiThreadBlocking(
                () ->
                        mOrchestrator.doDeclutterPass(
                                (TabbedModeTabModelOrchestrator)
                                        mActivityTestRule
                                                .getActivity()
                                                .getTabModelOrchestratorSupplier()
                                                .get()));
        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));

        runOnUiThreadBlocking(
                () -> {
                    TabbedModeTabModelOrchestrator normalOrchestrator =
                            (TabbedModeTabModelOrchestrator)
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelOrchestratorSupplier()
                                            .get();
                    normalOrchestrator.initForTesting(
                            normalOrchestrator.getTabModelSelector(),
                            mNormalTabPersistentStore,
                            mTabPersistencePolicy);
                    mOrchestrator.initForTesting(
                            mOrchestrator.getTabModelSelector(),
                            mArchivedTabPersistentStore,
                            mTabPersistencePolicy);

                    mOrchestrator.resetRescueArchivedTabsForTesting();
                    mOrchestrator.resetRescueArchivedTabGroupsForTesting();
                    mOrchestrator.rescueArchivedTabs(
                            (TabbedModeTabModelOrchestrator)
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelOrchestratorSupplier()
                                            .get());
                });

        CriteriaHelper.pollUiThread(() -> 2 == mRegularTabModel.getCount());
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        verify(mTabGroupSyncService, times(2)).updateArchivalStatus(eq(SYNC_GROUP_ID1), eq(false));
        verify(mArchivedTabPersistentStore).pauseSaveTabList();
        verify(mArchivedTabPersistentStore).resumeSaveTabList();
        verify(mNormalTabPersistentStore).pauseSaveTabList();
        verify(mNormalTabPersistentStore).resumeSaveTabList();
    }

    @Test
    @MediumTest
    public void testRescueTabs_ArchiveDisabled() {
        setupSavedTabGroup();
        finishLoading();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));

        setupDeclutterSettingsForTest();
        runOnUiThreadBlocking(
                () ->
                        mOrchestrator.doDeclutterPass(
                                (TabbedModeTabModelOrchestrator)
                                        mActivityTestRule
                                                .getActivity()
                                                .getTabModelOrchestratorSupplier()
                                                .get()));
        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));

        runOnUiThreadBlocking(
                () -> {
                    mOrchestrator.resetRescueArchivedTabsForTesting();
                    mOrchestrator.resetRescueArchivedTabGroupsForTesting();
                    mTabArchiveSettings.setArchiveEnabled(false);
                });

        CriteriaHelper.pollUiThread(() -> mRegularTabModel.getCount() == 2);
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        verify(mTabGroupSyncService, times(2)).updateArchivalStatus(eq(SYNC_GROUP_ID1), eq(false));
    }

    @Test
    @MediumTest
    public void testRescueTabs_ArchiveDisabledWhileNoOrchestatorsRegistered() {
        setupSavedTabGroup();
        finishLoading();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));

        setupDeclutterSettingsForTest();
        runOnUiThreadBlocking(
                () ->
                        mOrchestrator.doDeclutterPass(
                                (TabbedModeTabModelOrchestrator)
                                        mActivityTestRule
                                                .getActivity()
                                                .getTabModelOrchestratorSupplier()
                                                .get()));
        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));

        // Unregistering the tab model orchestrator should skip rescue (there's nothing to rescue
        // to).
        runOnUiThreadBlocking(
                () -> {
                    mOrchestrator.unregisterTabModelOrchestrator(mTabbedModeOrchestrator);
                    mOrchestrator.resetRescueArchivedTabsForTesting();
                    mOrchestrator.resetRescueArchivedTabGroupsForTesting();
                    mTabArchiveSettings.setArchiveEnabled(false);
                });
        CriteriaHelper.pollUiThread(() -> mArchivedTabModel.getCount() == 1);

        // Registering the tab model orchestrator will rescue tabs if the archive is disabled.
        runOnUiThreadBlocking(
                () -> {
                    mOrchestrator.registerTabModelOrchestrator(mTabbedModeOrchestrator);
                });
        CriteriaHelper.pollUiThread(() -> mRegularTabModel.getCount() == 2);
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        verify(mTabGroupSyncService, times(2)).updateArchivalStatus(eq(SYNC_GROUP_ID1), eq(false));
    }

    @Test
    @MediumTest
    public void testGetModelIndex() {
        finishLoading();
        int index = ThreadUtils.runOnUiThreadBlocking(() -> mArchivedTabModel.index());
        assertEquals(INVALID_TAB_INDEX, index);
    }

    @Test
    @MediumTest
    public void testDestroyBeforeActivityDestroyed() {
        finishLoading();
        runOnUiThreadBlocking(() -> ArchivedTabModelOrchestrator.destroyProfileKeyedMap());
        // The PKM is already destroyed, but the ATMO shouldn't crash when it
        // receives an activity destroyed event.
    }

    @Test
    @MediumTest
    public void testDeclutterAfterDestroy() {
        finishLoading();
        runOnUiThreadBlocking(() -> mOrchestrator.getTabArchiveSettings().setArchiveEnabled(false));
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, getTabCountOnUiThread(mRegularTabModel));

        setupDeclutterSettingsForTest();
        runOnUiThreadBlocking(
                () -> {
                    when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
                    ArgumentCaptor<TabModelSelectorObserver> observerCaptor =
                            ArgumentCaptor.forClass(TabModelSelectorObserver.class);
                    mOrchestrator.setTabModelSelectorForTesting(mTabModelSelector);
                    mOrchestrator.doDeclutterPass(
                            (TabbedModeTabModelOrchestrator)
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelOrchestratorSupplier()
                                            .get());
                    // Destroying this after a task has been queued should destroy the callback
                    // controller and skip the declutter process.
                    ArchivedTabModelOrchestrator.destroyProfileKeyedMap();
                    verify(mTabModelSelector).addObserver(observerCaptor.capture());
                    observerCaptor.getValue().onTabStateInitialized();
                });

        assertEquals(2, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
    }

    @Test
    @MediumTest
    public void testOpenArchivedTabFromHubSearch() {
        finishLoading();
        String declutterUrl = mActivityTestRule.getTestServer().getURL(TEST_PATH);
        mPage =
                mPage.loadWebPageProgrammatically(declutterUrl)
                        .openFakeLink(
                                mActivityTestRule.getTestServer().getURL(TEST_PATH_2),
                                WebPageStation.newBuilder());

        assertEquals(2, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        setupDeclutterSettingsForTest();
        runOnUiThreadBlocking(
                () -> {
                    doReturn(TimeUnit.HOURS.toMillis(2)).when(mClock).currentTimeMillis();
                    ((TabArchiverImpl) mOrchestrator.getTabArchiver()).setClockForTesting(mClock);
                    mRegularTabModel.getTabAt(0).setTimestampMillis(0L);
                    mRegularTabModel.getTabAt(1).setTimestampMillis(0L);
                    mTabArchiveSettings.setArchiveTimeDeltaHours(1);
                    mOrchestrator.doDeclutterPass(
                            (TabbedModeTabModelOrchestrator)
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelOrchestratorSupplier()
                                            .get());
                });

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));

        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown();
        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ null, /* title= */ "About", /* text= */ null);
        mPage = suggestion.openPage();
        assertEquals(declutterUrl, mPage.loadedTabElement.value().getUrl().getSpec());
        CriteriaHelper.pollUiThread(() -> 2 == mRegularTabModel.getCount());
    }

    @Test
    @MediumTest
    public void testOpenArchivedTabFromHubSearch_Incognito() {
        finishLoading();
        String declutterUrl = mActivityTestRule.getTestServer().getURL(TEST_PATH);
        mPage =
                mPage.loadWebPageProgrammatically(declutterUrl)
                        .openFakeLink(
                                mActivityTestRule.getTestServer().getURL(TEST_PATH_2),
                                WebPageStation.newBuilder());

        assertEquals(2, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        setupDeclutterSettingsForTest();
        runOnUiThreadBlocking(
                () -> {
                    doReturn(TimeUnit.HOURS.toMillis(2)).when(mClock).currentTimeMillis();
                    ((TabArchiverImpl) mOrchestrator.getTabArchiver()).setClockForTesting(mClock);
                    mRegularTabModel.getTabAt(0).setTimestampMillis(0L);
                    mRegularTabModel.getTabAt(1).setTimestampMillis(0L);
                    mTabArchiveSettings.setArchiveTimeDeltaHours(1);
                    mOrchestrator.doDeclutterPass(
                            (TabbedModeTabModelOrchestrator)
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelOrchestratorSupplier()
                                            .get());
                });

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));

        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openNewIncognitoTabOrWindowFast()
                        .openIncognitoTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("About");
        tabSwitcherSearchStation.checkSuggestionsShown();

        onView(allOf(withId(R.id.line_2), withText(containsString("About")))).check(doesNotExist());
    }

    private void setupSavedTabGroup() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.syncId = SYNC_GROUP_ID1;
        savedTabGroup.archivalTimeMs = System.currentTimeMillis();

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(savedTabGroup);
    }
}
