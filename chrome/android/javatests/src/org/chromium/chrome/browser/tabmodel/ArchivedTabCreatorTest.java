// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;

/** Tests for ChromeTabCreator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    ChromeFeatureList.ANDROID_TAB_DECLUTTER,
    ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH
})
public class ArchivedTabCreatorTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @ClassRule public static EmbeddedTestServerRule sTestServerRule = new EmbeddedTestServerRule();

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;
    private Profile mProfile;
    private ArchivedTabModelOrchestrator mOrchestrator;
    private TabCreator mTabCreator;

    @Before
    public void setUp() throws Exception {
        mTestServer = sTestServerRule.getServer();
        runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            sActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    mOrchestrator = ArchivedTabModelOrchestrator.getForProfile(mProfile);
                    mTabCreator = mOrchestrator.getArchivedTabCreatorForTesting();
                });
    }

    @After
    public void tearDown() {
        runOnUiThreadBlocking(() -> mOrchestrator.destroy());
    }

    @Test
    @MediumTest
    public void testCreateFrozenTab() throws Exception {
        Tab tab =
                sActivityTestRule.loadUrlInNewTab(
                        mTestServer.getURL(TEST_PATH), /* incognito= */ false);
        Tab frozenTab =
                runOnUiThreadBlocking(
                        () -> {
                            TabState state = TabStateExtractor.from(tab);
                            sActivityTestRule
                                    .getActivity()
                                    .getCurrentTabModel()
                                    .closeTabs(
                                            TabClosureParams.closeTab(tab)
                                                    .allowUndo(false)
                                                    .build());
                            return mTabCreator.createFrozenTab(state, tab.getId(), /* index= */ 0);
                        });
        assertNotNull(frozenTab);
        assertNull(frozenTab.getWebContents());
    }

    @Test
    @MediumTest
    public void testRestoreFallback() {
        runOnUiThreadBlocking(
                () -> {
                    assertNotNull(
                            mTabCreator.createNewTab(
                                    new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                    TabLaunchType.FROM_RESTORE,
                                    null));
                });
    }

    @Test
    @MediumTest
    public void testRestoreFallback_AssertionErrorWhenTabLaunchTypeIncorrect() throws Exception {
        // Test is a no-op when asserts are disabled.
        if (!BuildConfig.ENABLE_ASSERTS) return;

        runOnUiThreadBlocking(
                () -> {
                    try {
                        mTabCreator.createNewTab(
                                new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                TabLaunchType.FROM_CHROME_UI,
                                null);
                    } catch (AssertionError e) {
                        return;
                    }
                    fail(
                            "Creating a non-frozen tab should fail with an assert when the"
                                    + " launch type is incorrect.");
                });
    }
}
