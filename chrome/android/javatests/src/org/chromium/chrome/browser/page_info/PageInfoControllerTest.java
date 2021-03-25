// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.components.page_info.PageInfoFeatureList;
import org.chromium.components.page_info.PageInfoView;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

/**
 * Tests for PageInfoController.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
@Batch(ConnectionInfoViewTest.PAGE_INFO_BATCH_NAME)
@Batch.SplitByFeature
public class PageInfoControllerTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mTestServer = sActivityTestRule.getTestServer();
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Tests that PageInfoController can be instantiated and shown.
     */
    @Test
    @MediumTest
    @Feature({"PageInfoController"})
    public void testShow() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeActivity activity = sActivityTestRule.getActivity();
            Tab tab = activity.getActivityTab();
            new ChromePageInfo(
                    activity.getModalDialogManagerSupplier(), null, OpenedFromSource.MENU)
                    .show(tab, PageInfoController.NO_HIGHLIGHTED_PERMISSION);
        });
    }

    /**
     * Tests that PageInfoController converts safe URLs to Unicode.
     */
    @Test
    @MediumTest
    @Feature({"PageInfoController"})
    @DisableFeatures(PageInfoFeatureList.PAGE_INFO_V2)
    public void testPageInfoUrl() {
        String testUrl = mTestServer.getURLWithHostName("xn--allestrungen-9ib.ch", "/");
        sActivityTestRule.loadUrlInTab(
                testUrl, PageTransition.TYPED, sActivityTestRule.getActivity().getActivityTab());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeActivity activity = sActivityTestRule.getActivity();
            Tab tab = activity.getActivityTab();
            ChromePageInfoControllerDelegate chromePageInfoControllerDelegate =
                    new ChromePageInfoControllerDelegate(activity, tab.getWebContents(),
                            activity::getModalDialogManager,
                            /*offlinePageLoadUrlDelegate=*/
                            new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab));
            chromePageInfoControllerDelegate.setOfflinePageStateForTesting(
                    ChromePageInfoControllerDelegate.OfflinePageState.NOT_OFFLINE_PAGE);
            ChromePermissionParamsListBuilderDelegate chromePermissionParamsListBuilderDelegate =
                    new ChromePermissionParamsListBuilderDelegate();
            PageInfoController pageInfo =
                    new PageInfoController(tab.getWebContents(), ConnectionSecurityLevel.NONE,
                            /*publisher=*/null, chromePageInfoControllerDelegate,
                            chromePermissionParamsListBuilderDelegate,
                            PageInfoController.NO_HIGHLIGHTED_PERMISSION);
            PageInfoView pageInfoView = (PageInfoView) pageInfo.getPageInfoViewForTesting();
            // Test that the title contains the Unicode hostname rather than strict equality, as
            // the test server will be bound to a random port.
            Assert.assertTrue(
                    pageInfoView.getUrlTitleForTesting().contains("http://allestörungen.ch"));
        });
    }
}
