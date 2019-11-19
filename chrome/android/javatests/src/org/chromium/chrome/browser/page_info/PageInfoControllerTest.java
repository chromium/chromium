// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
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
public class PageInfoControllerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
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
    @RetryOnFailure
    public void testShow() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PageInfoController.show(mActivityTestRule.getActivity(),
                    mActivityTestRule.getActivity().getActivityTab(), null,
                    PageInfoController.OpenedFromSource.MENU);
        });
    }

    /**
     * Tests that PageInfoController converts safe URLs to Unicode.
     */
    @Test
    @MediumTest
    @Feature({"PageInfoController"})
    @RetryOnFailure
    public void testPageInfoUrl() {
        String testUrl = mTestServer.getURLWithHostName("xn--allestrungen-9ib.ch", "/");
        mActivityTestRule.loadUrlInTab(
                testUrl, PageTransition.TYPED, mActivityTestRule.getActivity().getActivityTab());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PageInfoController pageInfo = new PageInfoController(mActivityTestRule.getActivity(),
                    mActivityTestRule.getActivity().getActivityTab(), ConnectionSecurityLevel.NONE,
                    null, null, PageInfoController.OfflinePageState.NOT_OFFLINE_PAGE,
                    PageInfoController.PreviewPageState.NOT_PREVIEW, null);
            PageInfoView pageInfoView = pageInfo.getPageInfoViewForTesting();
            // Test that the title contains the Unicode hostname rather than strict equality, as
            // the test server will be bound to a random port.
            Assert.assertTrue(
                    pageInfoView.getUrlTitleForTesting().contains("http://allestörungen.ch"));
        });
    }
}
