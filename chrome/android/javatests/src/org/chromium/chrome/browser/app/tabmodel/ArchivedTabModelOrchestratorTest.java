// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;

/** Tests for ArchivedTabModelOrchestrator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER)
public class ArchivedTabModelOrchestratorTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private Profile mProfile;
    private ArchivedTabModelOrchestrator mOrchestrator;
    private TabModel mArchivedTabModel;
    private TabModel mRegularTabModel;
    private TabCreator mRegularTabCreator;

    @Before
    public void setUp() throws Exception {
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.sAndroidTabDeclutterArchiveTimeDeltaHours, "0");
        FeatureList.setTestValues(testValues);

        runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            sActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    mOrchestrator = ArchivedTabModelOrchestrator.getForProfile(mProfile);
                    mOrchestrator.resetArchiveSettingsForTesting();
                    mArchivedTabModel = mOrchestrator.getTabModelSelector().getModel(false);
                    mRegularTabModel = sActivityTestRule.getActivity().getCurrentTabModel();
                    mRegularTabCreator = sActivityTestRule.getActivity().getTabCreator(false);
                });
    }

    @After
    public void tearDown() throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    // Clear out all tabs between tests.
                    mArchivedTabModel.closeAllTabs();
                    mRegularTabModel.closeAllTabs();
                });
    }

    @Test
    @MediumTest
    public void testBeginDeclutter() {
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        runOnUiThreadBlocking(() -> mOrchestrator.resetBeginDeclutterForTesting());
        runOnUiThreadBlocking(() -> mOrchestrator.maybeBeginDeclutter());

        assertEquals(0, mRegularTabModel.getCount());
        assertEquals(2, mArchivedTabModel.getCount());
    }

    @Test
    @MediumTest
    public void testRescueTabs() {
        sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        runOnUiThreadBlocking(() -> mOrchestrator.resetBeginDeclutterForTesting());
        runOnUiThreadBlocking(() -> mOrchestrator.maybeBeginDeclutter());

        assertEquals(0, mRegularTabModel.getCount());
        assertEquals(2, mArchivedTabModel.getCount());

        runOnUiThreadBlocking(() -> mOrchestrator.maybeRescueArchivedTabs(mRegularTabCreator));

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
    }
}
