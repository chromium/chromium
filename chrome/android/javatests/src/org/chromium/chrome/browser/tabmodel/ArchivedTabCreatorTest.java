// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
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
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests for ChromeTabCreator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH})
public class ArchivedTabCreatorTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;
    private Profile mProfile;
    private ArchivedTabModelOrchestrator mOrchestrator;
    private TabCreator mTabCreator;
    private WebPageStation mInitialPage;

    @Before
    public void setUp() throws Exception {
        mTestServer = mActivityTestRule.getTestServer();
        mInitialPage = mActivityTestRule.startOnBlankPage();
        runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            mInitialPage
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    mOrchestrator = ArchivedTabModelOrchestrator.getForProfile(mProfile);
                    mTabCreator = mOrchestrator.getArchivedTabCreatorForTesting();
                });
        // Wait for the native tab state to be initialized so that we are sure that native is ready.
        CriteriaHelper.pollUiThread(
                () -> mOrchestrator.getTabModelSelector().isTabStateInitialized());
    }

    @Test
    @MediumTest
    public void testCreateFrozenTab() throws Exception {
        WebPageStation testPage = mInitialPage.openFakeLinkToWebPage(mTestServer.getURL(TEST_PATH));
        Tab tab = testPage.loadedTabElement.value();
        Tab frozenTab =
                runOnUiThreadBlocking(
                        () -> {
                            TabState state = TabStateExtractor.from(tab);
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabModel()
                                    .getTabRemover()
                                    .closeTabs(
                                            TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                            /* allowDialog= */ false);
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
                    int count = mOrchestrator.getTabModel().getCount();
                    assertNotNull(
                            mTabCreator.createNewTab(
                                    new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                    TabLaunchType.FROM_RESTORE,
                                    null));
                    assertEquals(count + 1, mOrchestrator.getTabModel().getCount());
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
