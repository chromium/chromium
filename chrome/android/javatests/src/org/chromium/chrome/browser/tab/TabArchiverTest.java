// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.ThreadUtils.runOnUiThreadBlockingNoException;

import androidx.test.filters.MediumTest;

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
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.TabArchiver.Clock;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeUnit;

/** Tests for TabArchiver. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER)
public class TabArchiverTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @ClassRule public static EmbeddedTestServerRule sTestServerRule = new EmbeddedTestServerRule();

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private @Mock Clock mClock;
    private @Mock TabModelSelector mSelector;

    private EmbeddedTestServer mTestServer;
    private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private TabArchiver mTabArchiver;
    private TabModel mArchivedTabModel;
    private TabModel mRegularTabModel;
    private TabCreator mArchivedTabCreator;
    private TabCreator mRegularTabCreator;
    private TabArchiveSettings mTabArchiveSettings;
    private SharedPreferencesManager mSharedPrefs;

    @Before
    public void setUp() throws Exception {
        mTestServer = sTestServerRule.getServer();

        mArchivedTabModelOrchestrator =
                runOnUiThreadBlockingNoException(
                        () ->
                                ArchivedTabModelOrchestrator.getForProfile(
                                        sActivityTestRule
                                                .getActivity()
                                                .getProfileProviderSupplier()
                                                .get()
                                                .getOriginalProfile()));
        mArchivedTabModel = mArchivedTabModelOrchestrator.getTabModelSelector().getModel(false);
        mArchivedTabCreator = mArchivedTabModelOrchestrator.getArchivedTabCreator();
        runOnUiThreadBlocking(
                () ->
                        // Clear out all archived tabs between tests.
                        mArchivedTabModel.closeAllTabs());

        mRegularTabModel = sActivityTestRule.getActivity().getCurrentTabModel();
        mRegularTabCreator = sActivityTestRule.getActivity().getTabCreator(false);

        mSharedPrefs = ChromeSharedPreferences.getInstance();
        mTabArchiveSettings = new TabArchiveSettings(mSharedPrefs);
        // Clear prefs set by tests.
        mTabArchiveSettings.resetSettingsForTesting();

        mTabArchiver =
                runOnUiThreadBlockingNoException(
                        () ->
                                new TabArchiver(
                                        mArchivedTabModel,
                                        mArchivedTabCreator,
                                        AsyncTabParamsManagerSingleton.getInstance(),
                                        TabWindowManagerSingleton.getInstance(),
                                        mTabArchiveSettings,
                                        mClock));
    }

    @Test
    @MediumTest
    public void testArchiveThenUnarchiveTab() throws Exception {
        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        mTestServer.getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        runOnUiThreadBlocking(() -> mTabArchiver.archiveAndRemoveTab(mRegularTabModel, tab));

        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());

        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.unarchiveAndRestoreTab(
                                mRegularTabCreator, mArchivedTabModel.getTabAt(0)));

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
    }

    @Test
    @MediumTest
    public void testTabModelSelectorInactiveTabsAreArchived() throws Exception {
        // Set the tab to expire after 1 hour to simplify testing.
        mTabArchiveSettings.setArchiveTimeDeltaHours(1);

        // Set the clock to 1 hour after 0.
        doReturn(TimeUnit.HOURS.toMillis(1)).when(mClock).currentTimeMillis();
        // Leave the first two tabs at 0, it will be archived.
        ((TabImpl) mRegularTabModel.getTabAt(0)).setTimestampMillisForTesting(0);
        Tab tab1 =
                sActivityTestRule.loadUrlInNewTab(
                        mTestServer.getURL(TEST_PATH), /* incognito= */ false);
        ((TabImpl) tab1).setTimestampMillisForTesting(0);
        Tab tab2 =
                sActivityTestRule.loadUrlInNewTab(
                        mTestServer.getURL(TEST_PATH), /* incognito= */ false);
        // Setup the 3rd tab be kept in the regular TabModel
        ((TabImpl) tab2).setTimestampMillisForTesting(TimeUnit.HOURS.toMillis(1));

        assertEquals(3, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        // Send an event, similar to how TabWindowManager would.
        runOnUiThreadBlocking(
                () ->
                        mTabArchiver.onTabModelSelectorAdded(
                                sActivityTestRule
                                        .getActivity()
                                        .getTabModelSelectorSupplier()
                                        .get()));
        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(2, mArchivedTabModel.getCount());
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
        mTabArchiveSettings.setArchiveEnabled(false);
        // Set the tab to expire after 1 hour to simplify testing.
        mTabArchiveSettings.setArchiveTimeDeltaHours(1);

        // Set the clock to 1 hour after 0.
        doReturn(TimeUnit.HOURS.toMillis(1)).when(mClock).currentTimeMillis();
        // Leave the first tab at 0, it will be archived.
        ((TabImpl) mRegularTabModel.getTabAt(0)).setTimestampMillisForTesting(0);
        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        mTestServer.getURL(TEST_PATH), /* incognito= */ false);
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
}
