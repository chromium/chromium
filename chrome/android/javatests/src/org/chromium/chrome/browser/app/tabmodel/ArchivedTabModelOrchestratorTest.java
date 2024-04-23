// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** Tests for ArchivedTabModelOrchestrator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Test interacts with activity shutdown and thus is incompatible with batching")
@EnableFeatures({
    ChromeFeatureList.ANDROID_TAB_DECLUTTER,
    ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH
})
public class ArchivedTabModelOrchestratorTest {
    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Profile mProfile;
    private ArchivedTabModelOrchestrator mOrchestrator;
    private TabModel mArchivedTabModel;
    private TabModel mRegularTabModel;
    private TabCreator mRegularTabCreator;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.sAndroidTabDeclutterArchiveTimeDeltaHours, "0");
        FeatureList.setTestValues(testValues);

        runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            mActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    mOrchestrator = ArchivedTabModelOrchestrator.getForProfile(mProfile);
                    mOrchestrator.resetArchiveSettingsForTesting();
                    mArchivedTabModel = mOrchestrator.getTabModelSelector().getModel(false);
                    mRegularTabModel = mActivityTestRule.getActivity().getCurrentTabModel();
                    mRegularTabCreator = mActivityTestRule.getActivity().getTabCreator(false);
                });
    }

    @Test
    @MediumTest
    public void testBeginDeclutter() {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

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
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

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

    @Test
    @MediumTest
    public void testDestroyBeforeActivityDestroyed() {
        runOnUiThreadBlocking(
                () -> ArchivedTabModelOrchestrator.destroyProfileKeyedMapForTesting());
        // The PKM is already destroyed, but the ATMO shouldn't crash when it
        // receives an activity destroyed event.
    }
}
