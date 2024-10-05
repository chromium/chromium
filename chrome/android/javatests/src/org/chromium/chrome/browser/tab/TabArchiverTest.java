// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Token;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.TabArchiver.Clock;
import org.chromium.chrome.browser.tab.state.ArchivePersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;

import java.util.concurrent.TimeUnit;

/** Tests for TabArchiver. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    ChromeFeatureList.ANDROID_TAB_DECLUTTER,
    ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH
})
public class TabArchiverTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private @Mock Clock mClock;
    private @Mock TabModelSelector mSelector;
    private @Mock TabWindowManager mTabWindowManager;

    private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private TabArchiver mTabArchiver;
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
        mArchivedTabModel = mArchivedTabModelOrchestrator.getTabModelSelector().getModel(false);
        mArchivedTabCreator = mArchivedTabModelOrchestrator.getArchivedTabCreatorForTesting();

        mRegularTabModel = sActivityTestRule.getActivity().getCurrentTabModel();
        mRegularTabCreator = sActivityTestRule.getActivity().getTabCreator(false);

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
                                new TabArchiver(
                                        mArchivedTabModel,
                                        mArchivedTabCreator,
                                        AsyncTabParamsManagerSingleton.getInstance(),
                                        mTabWindowManager,
                                        mTabArchiveSettings,
                                        mClock));
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        runOnUiThreadBlocking(
                () -> {
                    // Clear out all archived tabs between tests.
                    mArchivedTabModel.closeTabs(TabClosureParams.closeAllTabs().build());
                });
    }

    @Test
    @MediumTest
    public void testDestroy() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiver.initDeclutter();
                    mTabArchiver.destroy();
                    verify(mTabWindowManager).removeObserver(mTabArchiver);
                });
    }

    @Test
    @MediumTest
    public void testArchiveThenUnarchiveTab() throws Exception {
        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        runOnUiThreadBlocking(() -> mTabArchiver.archiveAndRemoveTab(mRegularTabModel, tab));
        assertEquals(1, mUserActionTester.getActionCount("Tabs.TabArchived"));

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

        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.unarchiveAndRestoreTab(
                                mRegularTabCreator, mArchivedTabModel.getTabAt(0)));
        assertEquals(1, mUserActionTester.getActionCount("Tabs.ArchivedTabRestored"));

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        runOnUiThreadBlocking(
                () -> assertEquals(Tab.INVALID_TAB_ID, mRegularTabModel.getTabAt(1).getParentId()));
    }

    @Test
    @MediumTest
    public void testGroupedTabsAreNotArchived() throws Exception {
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
        // Set the timestamp for both tabs at 0, they should will be archived.
        ((TabImpl) mRegularTabModel.getTabAt(0)).setTimestampMillisForTesting(0);
        ((TabImpl) mRegularTabModel.getTabAt(1)).setTimestampMillisForTesting(0);

        // Simulate the first tab being added to a group.
        runOnUiThreadBlocking(
                () -> mRegularTabModel.getTabAt(0).setTabGroupId(Token.createRandom()));

        assertEquals(3, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        // The grouped tab should be skipped.
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.onTabModelSelectorAdded(
                                sActivityTestRule
                                        .getActivity()
                                        .getTabModelSelectorSupplier()
                                        .get()));
        CriteriaHelper.pollUiThread(() -> 2 == mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        assertEquals(1, mUserActionTester.getActionCount("Tabs.TabArchived"));
    }

    @Test
    @MediumTest
    public void testTabModelSelectorInactiveTabsAreArchived() throws Exception {
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
                        .expectIntRecords("Tabs.TabArchived.AfterNDays", 0, 0)
                        .build();

        // Send an event, similar to how TabWindowManager would.
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.onTabModelSelectorAdded(
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
                    Tab archivedTab = mTabArchiver.archiveAndRemoveTab(mRegularTabModel, tab);
                    ArchivePersistedTabData.from(
                            archivedTab,
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
                    mTabArchiver.deleteEligibleArchivedTabs();
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
    public void testTabModelSelectorUninitialized() throws Exception {
        doReturn(false).when(mSelector).isTabStateInitialized();
        runOnUiThreadBlocking(() -> mTabArchiver.onTabModelSelectorAdded(mSelector));
        verify(mSelector, times(0)).getModel(anyBoolean());
    }

    @Test
    @MediumTest
    public void testTabModelSelectorInactiveTabsAreArchived_NoActionTakenWhenDisabled()
            throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mTabArchiveSettings.setArchiveEnabled(false);
                    // Set the tab to expire after 1 hour to simplify testing.
                    mTabArchiveSettings.setArchiveTimeDeltaHours(1);
                });

        // Set the clock to 1 hour after 0.
        doReturn(TimeUnit.HOURS.toMillis(1)).when(mClock).currentTimeMillis();
        // Leave the first tab at 0, it will be archived.
        ((TabImpl) mRegularTabModel.getTabAt(0)).setTimestampMillisForTesting(0);
        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false);
        // Setup the 2nd tab to expire.
        ((TabImpl) tab).setTimestampMillisForTesting(TimeUnit.HOURS.toMillis(1));

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        // Send an event, similar to how TabWindowManager would.
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.onTabModelSelectorAdded(
                                sActivityTestRule
                                        .getActivity()
                                        .getTabModelSelectorSupplier()
                                        .get()));
        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
    }

    @Test
    @MediumTest
    public void testArchivedTabParentRootIdsReset() throws Exception {
        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        runOnUiThreadBlocking(() -> mTabArchiver.archiveAndRemoveTab(mRegularTabModel, tab));
        assertEquals(1, mUserActionTester.getActionCount("Tabs.TabArchived"));

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
    public void testTabIdPresentInBothModelsDeletesRegularTab() throws Exception {
        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        sActivityTestRule.getTestServer().getURL(TEST_PATH),
                        /* incognito= */ false);

        TabState state = runOnUiThreadBlocking(() -> TabStateExtractor.from(tab));
        Tab archivedTab =
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
                        mTabArchiver.onTabModelSelectorAdded(
                                sActivityTestRule
                                        .getActivity()
                                        .getTabModelSelectorSupplier()
                                        .get()));

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        watcher.assertExpected();
    }
}
