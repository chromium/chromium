// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.tab.Tab.INVALID_TAB_ID;
import static org.chromium.chrome.browser.tabmodel.TabList.INVALID_TAB_INDEX;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.ActivityFinisher;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.TabArchiverImpl.Clock;
import org.chromium.chrome.browser.tab.state.ArchivePersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.Arrays;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests for TabArchiver. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH})
@DisableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
public class TabArchiverTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";
    private static final String TEST_PATH_2 = "/chrome/test/data/android/google.html";

    private @Mock Clock mClock;
    private @Mock TabModelSelector mSelector;
    private @Mock TabWindowManager mTabWindowManager;
    private @Mock Tab mTab;
    private @Mock TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    private @Mock TabGroupModelFilter mTabGroupModelFilter;
    private @Mock TabGroupSyncService mTabGroupSyncService;

    private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private TabArchiverImpl mTabArchiver;
    private TabModelSelector mRegularTabModelSelector;
    private TabModel mArchivedTabModel;
    private TabModel mRegularTabModel;
    private TabCreator mArchivedTabCreator;
    private TabCreator mRegularTabCreator;
    private TabArchiveSettings mTabArchiveSettings;
    private SharedPreferencesManager mSharedPrefs;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() throws Exception {
        mArchivedTabModelOrchestrator =
                runOnUiThreadBlocking(
                        () ->
                                ArchivedTabModelOrchestrator.getForProfile(
                                        sActivityTestRule
                                                .getActivity()
                                                .getProfileProviderSupplier()
                                                .get()
                                                .getOriginalProfile()));
        TabGroupModelFilter archivedTabGroupModelFilter =
                mArchivedTabModelOrchestrator
                        .getTabModelSelector()
                        .getTabGroupModelFilterProvider()
                        .getCurrentTabGroupModelFilter();
        mArchivedTabModel = archivedTabGroupModelFilter.getTabModel();
        mArchivedTabCreator = mArchivedTabModelOrchestrator.getArchivedTabCreatorForTesting();

        mRegularTabModelSelector = sActivityTestRule.getActivity().getTabModelSelector();
        mRegularTabModel = sActivityTestRule.getActivity().getCurrentTabModel();
        mRegularTabCreator = sActivityTestRule.getActivity().getTabCreator(false);

        doReturn(1).when(mTabWindowManager).getMaxSimultaneousSelectors();
        doReturn(mSelector).when(mTabWindowManager).getTabModelSelectorById(anyInt());
        doReturn(mRegularTabModel).when(mSelector).getModel(anyBoolean());
        doReturn(true).when(mSelector).isTabStateInitialized();
        doReturn(mTabGroupModelFilterProvider).when(mSelector).getTabGroupModelFilterProvider();
        doReturn(mTabGroupModelFilter)
                .when(mTabGroupModelFilterProvider)
                .getCurrentTabGroupModelFilter();
        doReturn(mRegularTabModel).when(mTabGroupModelFilter).getTabModel();

        mSharedPrefs = ChromeSharedPreferences.getInstance();
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiveSettings = new TabArchiveSettings(mSharedPrefs);
                    mTabArchiveSettings.resetSettingsForTesting();
                    mTabArchiveSettings.setArchiveEnabled(true);
                });

        mTabArchiver =
                runOnUiThreadBlocking(
                        () ->
                                new TabArchiverImpl(
                                        archivedTabGroupModelFilter,
                                        mArchivedTabCreator,
                                        mTabArchiveSettings,
                                        mClock,
                                        mTabGroupSyncService));
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        runOnUiThreadBlocking(
                () -> {
                    // Clear out all archived tabs between tests.
                    mArchivedTabModel
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeAllTabs().build(),
                                    /* allowDialog= */ false);
                });
    }

    @AfterClass
    public static void tearDownTestSuite() {
        ActivityFinisher.finishAll();
    }

    @Test
    @MediumTest
    public void testArchiveThenUnarchiveTab() {
        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabArchived.TabCount", 1)
                        .build();
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.archiveAndRemoveTabs(
                                mRegularTabModelSelector
                                        .getTabGroupModelFilterProvider()
                                        .getTabGroupModelFilter(false),
                                Arrays.asList(tab)));
        watcher.assertExpected();

        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        runOnUiThreadBlocking(
                () ->
                        assertEquals(
                                Tab.INVALID_TAB_ID, mArchivedTabModel.getTabAt(0).getParentId()));
        runOnUiThreadBlocking(
                () ->
                        assertEquals(
                                mArchivedTabModel.getTabAt(0).getId(),
                                mArchivedTabModel.getTabAt(0).getRootId()));

        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.ArchivedTabRestored.TabCount", 1)
                        .build();

        long previousTimestampMillis =
                runOnUiThreadBlocking(() -> mArchivedTabModel.getTabAt(0).getTimestampMillis());
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.unarchiveAndRestoreTabs(
                                mRegularTabCreator,
                                Arrays.asList(mArchivedTabModel.getTabAt(0)),
                                /* updateTimestamp= */ true,
                                /* areTabsBeingOpened= */ false));
        watcher.assertExpected();

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        runOnUiThreadBlocking(
                () -> assertEquals(Tab.INVALID_TAB_ID, mRegularTabModel.getTabAt(1).getParentId()));
        runOnUiThreadBlocking(
                () ->
                        assertNotEquals(
                                previousTimestampMillis,
                                mRegularTabModel.getTabAt(1).getTimestampMillis()));
    }

    @Test
    @MediumTest
    public void testArchiveThenUnarchiveTab_NoTimestampUpdate() {
        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabArchived.TabCount", 1)
                        .build();
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.archiveAndRemoveTabs(
                                mRegularTabModelSelector
                                        .getTabGroupModelFilterProvider()
                                        .getTabGroupModelFilter(false),
                                Arrays.asList(tab)));
        watcher.assertExpected();

        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        runOnUiThreadBlocking(
                () ->
                        assertEquals(
                                Tab.INVALID_TAB_ID, mArchivedTabModel.getTabAt(0).getParentId()));
        runOnUiThreadBlocking(
                () ->
                        assertEquals(
                                mArchivedTabModel.getTabAt(0).getId(),
                                mArchivedTabModel.getTabAt(0).getRootId()));

        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.ArchivedTabRestored.TabCount", 1)
                        .build();
        long previousTimestampMillis =
                runOnUiThreadBlocking(() -> mArchivedTabModel.getTabAt(0).getTimestampMillis());
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.unarchiveAndRestoreTabs(
                                mRegularTabCreator,
                                Arrays.asList(mArchivedTabModel.getTabAt(0)),
                                /* updateTimestamp= */ false,
                                /* areTabsBeingOpened= */ false));
        watcher.assertExpected();

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        runOnUiThreadBlocking(
                () -> assertEquals(Tab.INVALID_TAB_ID, mRegularTabModel.getTabAt(1).getParentId()));
        runOnUiThreadBlocking(
                () ->
                        assertEquals(
                                previousTimestampMillis,
                                mRegularTabModel.getTabAt(0).getTimestampMillis()));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS)
    public void testArchiveTabGroups() {
        String syncId = "sync_id";
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.syncId = syncId;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);

        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false);

        // Simulate the first tab being added to a group.
        runOnUiThreadBlocking(
                () -> {
                    TabGroupModelFilter filter =
                            mRegularTabModelSelector
                                    .getTabGroupModelFilterProvider()
                                    .getTabGroupModelFilter(false);
                    filter.createSingleTabGroup(tab);
                });

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabArchived.TabCount", 1)
                        .build();
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.archiveAndRemoveTabs(
                                mRegularTabModelSelector
                                        .getTabGroupModelFilterProvider()
                                        .getTabGroupModelFilter(false),
                                Arrays.asList(tab)));
        watcher.assertExpected();
        verify(mTabGroupSyncService, times(1)).updateArchivalStatus(eq(syncId), eq(true));

        // Verify that the tab group was not added to the archived tab model but closed from the
        // regular tab model.
        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
    }

    @Test
    @MediumTest
    public void testGroupedTabsAreNotArchived() {
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        runOnUiThreadBlocking(
                () -> {
                    // Set the tab to expire after 1 hour to simplify testing.
                    mTabArchiveSettings.setArchiveTimeDeltaHours(1);
                });

        // Set the clock to 1 hour after 0.
        doReturn(TimeUnit.HOURS.toMillis(1)).when(mClock).currentTimeMillis();
        // Set the timestamp for both tabs at 0, they should be archived.
        TabImpl tab1 = (TabImpl) mRegularTabModel.getTabAt(0);
        tab1.setTimestampMillisForTesting(0);
        ((TabImpl) mRegularTabModel.getTabAt(1)).setTimestampMillisForTesting(0);

        // Simulate the first tab being added to a group.
        runOnUiThreadBlocking(
                () -> {
                    TabGroupModelFilter filter =
                            mRegularTabModelSelector
                                    .getTabGroupModelFilterProvider()
                                    .getTabGroupModelFilter(false);
                    filter.createSingleTabGroup(tab1);
                });

        assertEquals(3, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabArchived.TabCount", 1)
                        .build();
        // The grouped tab should be skipped.
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.doArchivePass(
                                sActivityTestRule
                                        .getActivity()
                                        .getTabModelSelectorSupplier()
                                        .get()));
        CriteriaHelper.pollUiThread(() -> 2 == mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS)
    public void testGroupedTabsAreArchived() {
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        runOnUiThreadBlocking(
                () -> {
                    // Set the tab to expire after 2 hour to simplify testing.
                    mTabArchiveSettings.setArchiveTimeDeltaHours(2);
                });

        // Set the clock to 2 hour after 0.
        doReturn(TimeUnit.HOURS.toMillis(2)).when(mClock).currentTimeMillis();
        // Set the timestamp for the tabs to 0, it should be archived.
        // Set the navigation timestamp for the tab to 1 to pass user active check.
        TabImpl tab1 = ((TabImpl) mRegularTabModel.getTabAt(0));
        tab1.setTimestampMillisForTesting(0);
        tab1.setLastNavigationCommittedTimestampMillis(TimeUnit.HOURS.toMillis(1));

        // Simulate the first tab being added to a group.
        runOnUiThreadBlocking(
                () -> {
                    TabGroupModelFilter filter =
                            mRegularTabModelSelector
                                    .getTabGroupModelFilterProvider()
                                    .getTabGroupModelFilter(false);
                    filter.createSingleTabGroup(tab1);
                });

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabArchived.TabCount", 1)
                        .build();
        // The grouped tab should not be added to the archived tab model and have been closed from
        // the regular tab model.
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.doArchivePass(
                                sActivityTestRule
                                        .getActivity()
                                        .getTabModelSelectorSupplier()
                                        .get()));
        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS)
    public void testTabsAreNotArchived_userNotActive() {
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        runOnUiThreadBlocking(
                () -> {
                    // Set the tab to expire after 1 hour to simplify testing.
                    mTabArchiveSettings.setArchiveTimeDeltaHours(1);
                });

        // Set the clock to 1 hour after 0.
        doReturn(TimeUnit.HOURS.toMillis(1)).when(mClock).currentTimeMillis();
        // Set the timestamp for both tabs at 0, they should be archived.
        // Set the navigation timestamp for both tabs at 0 to fail user active check.
        TabImpl tab1 = ((TabImpl) mRegularTabModel.getTabAt(0));
        tab1.setTimestampMillisForTesting(0);
        tab1.setLastNavigationCommittedTimestampMillis(0);
        ((TabImpl) mRegularTabModel.getTabAt(1)).setTimestampMillisForTesting(0);
        ((TabImpl) mRegularTabModel.getTabAt(1)).setLastNavigationCommittedTimestampMillis(0);

        // Simulate the first tab being added to a group.
        runOnUiThreadBlocking(
                () -> {
                    TabGroupModelFilter filter =
                            mRegularTabModelSelector
                                    .getTabGroupModelFilterProvider()
                                    .getTabGroupModelFilter(false);
                    filter.createSingleTabGroup(tab1);
                });

        assertEquals(3, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tabs.ArchivePass.DurationMs")
                        .build();
        // No remaining tabs should be archived and the declutter pass skipped.
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.doArchivePass(
                                sActivityTestRule
                                        .getActivity()
                                        .getTabModelSelectorSupplier()
                                        .get()));
        CriteriaHelper.pollUiThread(() -> 3 == mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testGroupedDuplicateTabsAreNotArchived() {
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        runOnUiThreadBlocking(
                () -> {
                    // Set the tab to expire after 1 hour to simplify testing.
                    mTabArchiveSettings.setArchiveTimeDeltaHours(1);
                });

        // Set the clock to 1 hour after 0.
        doReturn(TimeUnit.HOURS.toMillis(1)).when(mClock).currentTimeMillis();
        // Set the timestamp for both tabs at 0, they should be archived.
        TabImpl tab1 = ((TabImpl) mRegularTabModel.getTabAt(0));
        tab1.setTimestampMillisForTesting(0);
        TabImpl tab2 = ((TabImpl) mRegularTabModel.getTabAt(1));
        tab2.setTimestampMillisForTesting(0);
        ((TabImpl) mRegularTabModel.getTabAt(2)).setTimestampMillisForTesting(0);

        // Simulate the first and second tab being added to a group.
        runOnUiThreadBlocking(
                () -> {
                    TabGroupModelFilter filter =
                            mRegularTabModelSelector
                                    .getTabGroupModelFilterProvider()
                                    .getTabGroupModelFilter(false);
                    filter.mergeTabsToGroup(tab2.getId(), tab1.getId());
                });

        assertEquals(4, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabArchived.TabCount", 1)
                        .build();
        // The grouped tabs should be skipped. But the single tab, even though it is a duplicate
        // URL, should be archived since it is a standalone tab which passed the time threshold.
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.doArchivePass(
                                sActivityTestRule
                                        .getActivity()
                                        .getTabModelSelectorSupplier()
                                        .get()));
        CriteriaHelper.pollUiThread(() -> 3 == mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testDuplicateTabsAreArchived() {
        // Tab 2
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        // Tab 3
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        // Tab 4
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH_2), /* incognito= */ false);

        runOnUiThreadBlocking(
                () -> {
                    // Set the tab to expire after 2 hour to simplify testing.
                    mTabArchiveSettings.setArchiveTimeDeltaHours(2);
                });

        // Set the clock to 1 hour after 0. No tabs should be archived by timestamp eligibility.
        doReturn(TimeUnit.HOURS.toMillis(1)).when(mClock).currentTimeMillis();
        // Set the timestamp for the second and third tabs sharing the same URL (not fourth since it
        // will be the new active tab), tab 2 at 0 and tab 3 at 1.
        ((TabImpl) mRegularTabModel.getTabAt(1)).setTimestampMillisForTesting(0);
        ((TabImpl) mRegularTabModel.getTabAt(2)).setTimestampMillisForTesting(1);

        assertEquals(4, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabArchived.TabCount", 1)
                        .build();
        // Only one of the tabs with duplicate URLs should be archived.
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiveSettings.setArchiveDuplicateTabsEnabled(true);
                    mTabArchiver.doArchivePass(
                            sActivityTestRule.getActivity().getTabModelSelectorSupplier().get());
                });
        CriteriaHelper.pollUiThread(() -> 3 == mRegularTabModel.getCount());
        // Check that tab 3 (which is now tab 2) is the duplicate that remains as it is last active.
        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getTabAt(1).getTimestampMillis());
        assertEquals(1, mArchivedTabModel.getCount());
        // Check that tab 2 has been archived.
        assertEquals(0, mArchivedTabModel.getTabAt(0).getTimestampMillis());
        watcher.assertExpected();
        String action = "Tabs.ArchivedDuplicateTab";
        assertTrue(mUserActionTester.getActions().contains(action));
    }

    @Test
    @MediumTest
    public void testDuplicateTabsAreNotArchivedWithSwitchOff() {
        // Tab 2
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        // Tab 3
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        // Tab 4
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH_2), /* incognito= */ false);

        runOnUiThreadBlocking(
                () -> {
                    // Set the tab to expire after 2 hour to simplify testing.
                    mTabArchiveSettings.setArchiveTimeDeltaHours(2);
                });

        // Set the clock to 1 hour after 0. No tabs should be archived by timestamp eligibility.
        doReturn(TimeUnit.HOURS.toMillis(1)).when(mClock).currentTimeMillis();
        // Set the timestamp for the second and third tabs sharing the same URL (not fourth since it
        // will be the new active tab), tab 2 at 0 and tab 3 at 1.
        ((TabImpl) mRegularTabModel.getTabAt(1)).setTimestampMillisForTesting(0);
        ((TabImpl) mRegularTabModel.getTabAt(2)).setTimestampMillisForTesting(1);

        assertEquals(4, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder().expectNoRecords("Tabs.TabArchived.TabCount").build();
        // Duplicate tabs should not be archived.
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiveSettings.setArchiveDuplicateTabsEnabled(false);
                    mTabArchiver.doArchivePass(
                            sActivityTestRule.getActivity().getTabModelSelectorSupplier().get());
                });
        CriteriaHelper.pollUiThread(() -> 4 == mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testDuplicateTabInGroupIsNotArchived_BaseDuplicateOutOfGroup() {
        // Tab 2
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        // Tab 3
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        // Tab 4
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH_2), /* incognito= */ false);

        runOnUiThreadBlocking(
                () -> {
                    // Set the tab to expire after 2 hour to simplify testing.
                    mTabArchiveSettings.setArchiveTimeDeltaHours(2);
                });

        // Set the clock to 1 hour after 0. No tabs should be archived by timestamp eligibility.
        doReturn(TimeUnit.HOURS.toMillis(1)).when(mClock).currentTimeMillis();
        // Set the timestamp for the second and third tabs sharing the same URL (not fourth since it
        // will be the new active tab), tab 2 at 0 and tab 3 at 1.
        TabImpl tab1 = ((TabImpl) mRegularTabModel.getTabAt(0));
        tab1.setTimestampMillisForTesting(0);
        TabImpl tab2 = ((TabImpl) mRegularTabModel.getTabAt(1));
        tab2.setTimestampMillisForTesting(0);

        // Simulate the first and second tab being added to a group.
        runOnUiThreadBlocking(
                () -> {
                    TabGroupModelFilter filter =
                            mRegularTabModelSelector
                                    .getTabGroupModelFilterProvider()
                                    .getTabGroupModelFilter(false);
                    filter.mergeTabsToGroup(tab2.getId(), tab1.getId());
                });

        assertEquals(4, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder().expectNoRecords("Tabs.TabArchived.TabCount").build();
        // None of the tabs with duplicate URLs should be archived.
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.doArchivePass(
                                sActivityTestRule
                                        .getActivity()
                                        .getTabModelSelectorSupplier()
                                        .get()));
        CriteriaHelper.pollUiThread(() -> 4 == mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        watcher.assertExpected();
        String action = "Tabs.ArchivedDuplicateTab";
        assertFalse(mUserActionTester.getActions().contains(action));
    }

    @Test
    @MediumTest
    public void testTabModelSelectorInactiveTabsAreArchived() {
        runOnUiThreadBlocking(
                () -> {
                    // Set the tab to expire after 1 hour to simplify testing.
                    mTabArchiveSettings.setArchiveTimeDeltaHours(1);
                });

        // Set the clock to 1 hour after 0.
        doReturn(TimeUnit.HOURS.toMillis(1)).when(mClock).currentTimeMillis();
        // Leave the first two tabs at 0, it will be archived.
        ((TabImpl) mRegularTabModel.getTabAt(0)).setTimestampMillisForTesting(0);
        Tab tab1 =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false);
        ((TabImpl) tab1).setTimestampMillisForTesting(0);
        Tab tab2 =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false);
        // Setup the 3rd tab be kept in the regular TabModel
        ((TabImpl) tab2).setTimestampMillisForTesting(TimeUnit.HOURS.toMillis(1));

        assertEquals(3, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabArchiveEligibilityCheck.AfterNDays", 0, 0)
                        .build();

        // Send an event, similar to how TabWindowManager would.
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.doArchivePass(
                                sActivityTestRule
                                        .getActivity()
                                        .getTabModelSelectorSupplier()
                                        .get()));
        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(2, mArchivedTabModel.getCount());
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testEligibleTabsAreAutoDeleted() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiveSettings.setArchiveTimeDeltaHours(0);
                });

        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        CallbackHelper callbackHelper = new CallbackHelper();
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiver.archiveAndRemoveTabs(
                            mRegularTabModelSelector
                                    .getTabGroupModelFilterProvider()
                                    .getTabGroupModelFilter(false),
                            Arrays.asList(tab));
                    ArchivePersistedTabData.from(
                            mArchivedTabModel.getTabAt(0),
                            (archivedTabData) -> {
                                assertNotNull(archivedTabData);
                                callbackHelper.notifyCalled();
                            });
                });
        callbackHelper.waitForNext();

        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());

        Tab archivedTab = mArchivedTabModel.getTabAt(0);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabAutoDeleteEligibilityCheck.AfterNDays", 0)
                        .expectIntRecords("Tabs.TabAutoDeleted.AfterNDays", 0)
                        .build();
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiveSettings.setAutoDeleteEnabled(true);
                    mTabArchiveSettings.setAutoDeleteTimeDeltaHours(0);
                    mTabArchiver.doAutodeletePass();
                });

        CriteriaHelper.pollInstrumentationThread(() -> mArchivedTabModel.getCount() == 0);
        CriteriaHelper.pollInstrumentationThread(() -> archivedTab.isDestroyed());
        assertEquals(1, mRegularTabModel.getCount());

        runOnUiThreadBlocking(
                () -> {
                    ArchivePersistedTabData.from(
                            archivedTab,
                            (archivedTabData) -> {
                                assertNull(archivedTabData);
                                callbackHelper.notifyCalled();
                            });
                });
        callbackHelper.waitForNext();
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testTabModelSelectorUninitialized() {
        doReturn(false).when(mSelector).isTabStateInitialized();
        runOnUiThreadBlocking(() -> mTabArchiver.doArchivePass(mSelector));
        verify(mSelector, times(0)).getModel(anyBoolean());
    }

    @Test
    @MediumTest
    public void testArchivedTabParentRootIdsReset() {
        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabArchived.TabCount", 1)
                        .build();
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.archiveAndRemoveTabs(
                                mRegularTabModelSelector
                                        .getTabGroupModelFilterProvider()
                                        .getTabGroupModelFilter(false),
                                Arrays.asList(tab)));

        watcher.assertExpected();
        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        runOnUiThreadBlocking(
                () -> {
                    Tab archivedTab = mArchivedTabModel.getTabAt(0);
                    assertEquals(Tab.INVALID_TAB_ID, archivedTab.getParentId());
                    assertEquals(archivedTab.getId(), archivedTab.getRootId());

                    archivedTab.setRootId(7);
                    archivedTab.setParentId(7);

                    mTabArchiver.ensureArchivedTabsHaveCorrectFields();
                    assertEquals(Tab.INVALID_TAB_ID, archivedTab.getParentId());
                    assertEquals(archivedTab.getId(), archivedTab.getRootId());
                });
    }

    @Test
    @MediumTest
    public void testTabIdPresentInBothModelsDeletesRegularTab() {
        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false);

        TabState state = runOnUiThreadBlocking(() -> TabStateExtractor.from(tab));
        runOnUiThreadBlocking(
                () ->
                        mArchivedTabCreator.createFrozenTab(
                                state, tab.getId(), TabModel.INVALID_TAB_INDEX));

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabArchived.FoundDuplicateInRegularModel", 1)
                        .build();
        // Running the declutter code will de-dupe from the regular tab model, resulting in there be
        // 1 tab in each.
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.doArchivePass(
                                sActivityTestRule
                                        .getActivity()
                                        .getTabModelSelectorSupplier()
                                        .get()));

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testPersistedTabDataNull() throws Exception {
        doReturn(false).when(mTab).isInitialized();
        // This shouldn't NPE when the persisted tab data is null.
        runOnUiThreadBlocking(
                () -> mTabArchiver.initializePersistedTabDataAsync(Arrays.asList(mTab)));

        CallbackHelper callbackHelper = new CallbackHelper();
        runOnUiThreadBlocking(
                () -> {
                    ArchivePersistedTabData.from(
                            mTab,
                            (tabPersistedData) -> {
                                assertNull(tabPersistedData);
                                callbackHelper.notifyCalled();
                            });
                });
        callbackHelper.waitForNext();
    }

    @Test
    @MediumTest
    public void testArchivePassRecordsMetrics() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiveSettings.setArchiveEnabled(true);
                    mTabArchiveSettings.setArchiveTimeDeltaHours(0);
                });

        addRegularTabInBackgroundForArchive(TEST_PATH);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabArchived.TabCount", 1)
                        .expectAnyRecordTimes("Tabs.ArchivePass.DurationMs", 1)
                        .expectAnyRecordTimes("Tabs.InitializePTD.DurationMs", 1)
                        .build();
        CallbackHelper callbackHelper = new CallbackHelper();
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiver.addObserver(
                            new TabArchiver.Observer() {
                                @Override
                                public void onArchivePersistedTabDataCreated() {
                                    callbackHelper.notifyCalled();
                                }
                            });
                    mTabArchiver.doArchivePass(mRegularTabModelSelector);
                });

        // Wait for both observer methods to fire.
        callbackHelper.waitForNext();
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testAutodeletePassRecordsMetrics() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiveSettings.setArchiveEnabled(true);
                    mTabArchiveSettings.setArchiveTimeDeltaHours(0);
                    mTabArchiveSettings.setAutoDeleteEnabled(true);
                    mTabArchiveSettings.setAutoDeleteTimeDeltaHours(0);
                });

        addRegularTabInBackgroundForArchive(TEST_PATH);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        CallbackHelper callbackHelper = new CallbackHelper();
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiver.addObserver(
                            new TabArchiver.Observer() {
                                @Override
                                public void onArchivePersistedTabDataCreated() {
                                    callbackHelper.notifyCalled();
                                }
                            });
                    mTabArchiver.doArchivePass(mRegularTabModelSelector);
                });

        // Wait for both observer methods to fire.
        callbackHelper.waitForNext();

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.TabAutoDeleted.AfterNDays", 0)
                        .expectAnyRecordTimes("Tabs.DeleteWithPTD.DurationMs", 1)
                        .build();

        runOnUiThreadBlocking(
                () -> {
                    mTabArchiver.addObserver(
                            new TabArchiver.Observer() {
                                @Override
                                public void onAutodeletePassCompleted() {
                                    callbackHelper.notifyCalled();
                                }
                            });
                    mTabArchiver.doAutodeletePass();
                });
        callbackHelper.waitForNext();
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testTabArchiverDestroyedWhileCreatingPtd() throws TimeoutException {
        // Setup the clock to differentiate between PTD created by tab archiver versus the
        // verification code.
        long tabArchiverTimestamp = 99L;
        doReturn(tabArchiverTimestamp).when(mClock).currentTimeMillis();

        Tab archivedTab =
                runOnUiThreadBlocking(
                        () -> {
                            Tab tab =
                                    mArchivedTabCreator.createFrozenTab(
                                            null, INVALID_TAB_ID, INVALID_TAB_INDEX);
                            // This will call PTD#from which posts a task for the result.
                            mTabArchiver.initializePersistedTabDataAsyncImpl(
                                    Arrays.asList(tab), 0, 0);
                            // Immediately destroying the TabArchiver will verify that the task is
                            // correctly cancelled.
                            mTabArchiver.destroy();
                            return tab;
                        });

        CallbackHelper callbackHelper = new CallbackHelper();

        runOnUiThreadBlocking(
                () -> {
                    ArchivePersistedTabData.from(
                            archivedTab,
                            (ptd) -> {
                                assertNotEquals(tabArchiverTimestamp, ptd.getArchivedTimeMs());
                                callbackHelper.notifyCalled();
                            });
                });
        callbackHelper.waitForNext();
    }

    @Test
    @MediumTest
    public void testTabArchiverDestroyedWhileDestroyingPtd() throws TimeoutException {
        // Setup the clock to differentiate between PTD created by tab archiver versus the
        // verification code.
        long tabArchiverTimestamp = 99L;
        doReturn(tabArchiverTimestamp).when(mClock).currentTimeMillis();

        Tab archivedTab =
                runOnUiThreadBlocking(
                        () -> {
                            Tab tab =
                                    mArchivedTabCreator.createFrozenTab(
                                            null, INVALID_TAB_ID, INVALID_TAB_INDEX);
                            mTabArchiver.initializePersistedTabDataAsyncImpl(
                                    Arrays.asList(tab), 0, 0);
                            return tab;
                        });

        CallbackHelper callbackHelper = new CallbackHelper();
        runOnUiThreadBlocking(
                () -> {
                    ArchivePersistedTabData.from(
                            archivedTab,
                            (ptd) -> {
                                assertEquals(tabArchiverTimestamp, ptd.getArchivedTimeMs());
                                callbackHelper.notifyCalled();
                            });
                });
        callbackHelper.waitForNext();

        runOnUiThreadBlocking(
                () -> {
                    mTabArchiver.deleteArchivedTabsIfEligibleAsyncImpl(
                            Arrays.asList(archivedTab), 0, 0);
                    // This should cause the callback to be destroyed, and the ptd should still
                    // exist with the value set earlier in the test.
                    mTabArchiver.destroy();
                    ArchivePersistedTabData.from(
                            archivedTab,
                            (ptd) -> {
                                assertEquals(tabArchiverTimestamp, ptd.getArchivedTimeMs());
                                callbackHelper.notifyCalled();
                            });
                });
        callbackHelper.waitForNext();
    }

    private void addRegularTabInBackgroundForArchive(String path) {
        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false,
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND);
        runOnUiThreadBlocking(() -> tab.setTimestampMillis(0));
    }
}
