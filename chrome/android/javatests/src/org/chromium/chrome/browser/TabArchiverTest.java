// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.ThreadUtils.runOnUiThreadBlockingNoException;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;

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

    @ClassRule public static EmbeddedTestServerRule sTestServerRule = new EmbeddedTestServerRule();

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;
    private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private TabArchiver mTabArchiver;
    private TabModel mArchivedTabModel;
    private TabModel mRegularTabModel;
    private TabCreator mArchivedTabCreator;
    private TabCreator mRegularTabCreator;

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
        mRegularTabModel = sActivityTestRule.getActivity().getCurrentTabModel();
        mArchivedTabCreator = mArchivedTabModelOrchestrator.getArchivedTabCreator();
        mRegularTabCreator = sActivityTestRule.getActivity().getTabCreator(false);

        mTabArchiver =
                new TabArchiver(
                        mArchivedTabCreator,
                        mArchivedTabModel,
                        AsyncTabParamsManagerSingleton.getInstance());
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
}
