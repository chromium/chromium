// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests the functionality of return to chrome features that open overview mode if the timeout
 * has passed.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study",
        "force-fieldtrials=Study/Group"})
public class ReturnToChromeTest {
    private static final String BASE_PARAMS =
            "force-fieldtrial-params=Study.Group:" + TAB_SWITCHER_ON_RETURN_MS + "/0";
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private String mUrl;

    @Rule
    public RenderTestRule mRenderTestRule = new RenderTestRule();

    @Before
    public void setUp() {
        FeatureUtilities.setGridTabSwitcherEnabledForTesting(true);

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mUrl = testServer.getURL("/chrome/test/data/android/navigate/simple.html");

        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Test that overview mode is not triggered if the delay is longer than the interval between
     * stop and start.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS + "/100000"})
    public void testTabSwitcherModeNotTriggeredWithinThreshold() throws Exception {
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, 2, 0, mUrl);
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());

        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertFalse(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
    }

    /**
     * Test that overview mode is triggered if the delay is shorter than the interval between
     * stop and start.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS + "/0"})
    public void testTabSwitcherModeTriggeredBeyondThreshold() throws Exception {
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, 2, 0, mUrl);
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());

        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        CriteriaHelper.pollUiThread(Criteria.equals(true,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()::isTabModelRestored));

        assertEquals(2, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
    }

    @Test
    @SmallTest
    @Feature({"ReturnToChrome", "RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/" + TAB_SWITCHER_ON_RETURN_MS + "/0"})
    @DisableIf.Build(hardware_is = "bullhead", message = "https://crbug.com/1025241")
    public void testInitialScrollIndex() throws Exception {
        // clang-format on
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, 10, 0, mUrl);

        // Trigger thumbnail capturing for the last tab.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());

        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());

        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        CriteriaHelper.pollUiThread(Criteria.equals(true,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()::isTabModelRestored));

        assertEquals(10, mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
        assertEquals(9, mActivityTestRule.getActivity().getCurrentTabModel().index());
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(
                                       org.chromium.chrome.tab_ui.R.id.tab_list_view),
                "10_web_tabs-select_last");
    }
}
