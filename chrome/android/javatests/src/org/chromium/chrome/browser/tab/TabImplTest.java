// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;

/** Tests for the {@link TabImpl} class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
public class TabImplTest {
    private static final String TEST_PATH = "/chrome/test/data/android/about.html";
    private static final long DEFAULT_MAX_TIME_TO_WAIT_IN_MS = 3000;

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private WebPageStation mInitialPage;

    @Before
    public void setUp() {
        mInitialPage = mActivityTestRule.startOnBlankPage();
    }

    private TabImpl createFrozenTab() {
        String url = mActivityTestRule.getTestServer().getURL(TEST_PATH);
        WebPageStation testPage = mInitialPage.openFakeLinkToWebPage(url);
        Tab tab = testPage.loadedTabElement.get();

        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabState state = TabStateExtractor.from(tab);
                    mActivityTestRule
                            .getActivity()
                            .getCurrentTabModel()
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                    /* allowDialog= */ false);
                    return (TabImpl)
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createFrozenTab(state, tab.getId(), /* index= */ 1);
                });
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabLoadIfNeededEnsuresBackingForMediaCapture() {
        TabImpl tab = createFrozenTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> tab.loadIfNeeded(TabLoadIfNeededCaller.MEDIA_CAPTURE_PICKER));

        ThreadUtils.runOnUiThreadBlocking(() -> assertTrue(tab.hasBacking()));
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabIsNotInPWA() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.getActivity().getActivityTab(),
                            Matchers.notNullValue());
                },
                DEFAULT_MAX_TIME_TO_WAIT_IN_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        assertFalse(mActivityTestRule.getActivity().getActivityTab().isTabInPWA());
        assertTrue(mActivityTestRule.getActivity().getActivityTab().isTabInBrowser());
    }
}
