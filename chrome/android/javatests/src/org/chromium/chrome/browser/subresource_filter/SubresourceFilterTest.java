// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subresource_filter;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.MockSafeBrowsingApiHandler;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.components.subresource_filter.AdsBlockedInfoBar;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.List;

/**
 * End to end tests of SubresourceFilter ad filtering on Android.
 *
 * Since these tests take a while to set up (averaging 12 seconds between activity startup and
 * ruleset publishing), prefer to limit the number of test cases where possible.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class SubresourceFilterTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private EmbeddedTestServer mTestServer;

    private static final String PAGE_WITH_JPG =
            "/chrome/test/data/android/subresource_filter/page-with-img.html";
    private static final String LEARN_MORE_PAGE =
            "https://support.google.com/chrome/?p=blocked_ads";
    private static final String METADATA_FOR_ENFORCEMENT =
            "{\"matches\":[{\"threat_type\":\"13\",\"sf_bas\":\"\"}]}";
    private static final String METADATA_FOR_WARNING =
            "{\"matches\":[{\"threat_type\":\"13\",\"sf_bas\":\"warn\"}]}";

    private void createAndPublishRulesetDisallowingSuffix(String suffix) {
        TestRulesetPublisher publisher = new TestRulesetPublisher();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> publisher.createAndPublishRulesetDisallowingSuffixForTesting(suffix));

        // This takes an average of 6 seconds but can range anywhere from 2-10 seconds on occasion.
        // Since we are testing startup events, ensuring that they fire, this delay is unavoidable.
        // Increase the timeout to 15 seconds to remove flakiness.
        CriteriaHelper.pollUiThread(
                publisher::isPublished, 15000L, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Before
    public void setUp() throws Exception {
        mTestServer = mTestServerRule.getServer();

        // Create a new temporary instance to ensure the Class is loaded. Otherwise we will get a
        // ClassNotFoundException when trying to instantiate during startup.
        SafeBrowsingApiBridge.setSafeBrowsingHandlerType(
                new MockSafeBrowsingApiHandler().getClass());
        mActivityTestRule.startMainActivityOnBlankPage();

        // Disallow all jpgs.
        createAndPublishRulesetDisallowingSuffix(".jpg");
    }

    @After
    public void tearDown() {
        MockSafeBrowsingApiHandler.clearMockResponses();
    }

    @Test
    @LargeTest
    public void resourceNotFiltered() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        mActivityTestRule.loadUrl(url);

        String loaded = mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded");
        Assert.assertEquals("true", loaded);

        // Check that the infobar is not showing.
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        CriteriaHelper.pollUiThread(() -> infoBars.isEmpty());
    }

    @Test
    @LargeTest
    public void resourceFilteredClose() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        MockSafeBrowsingApiHandler.addMockResponse(url, METADATA_FOR_ENFORCEMENT);
        mActivityTestRule.loadUrl(url);

        String loaded = mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded");
        Assert.assertEquals("false", loaded);

        // Check that the infobar is showing.
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        CriteriaHelper.pollUiThread(() -> infoBars.size() == 1);
        AdsBlockedInfoBar infobar = (AdsBlockedInfoBar) infoBars.get(0);

        // Click the link once to expand it.
        TestThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);

        // Check the checkbox and press the button to reload.
        TestThreadUtils.runOnUiThreadBlocking(() -> infobar.onCheckedChanged(null, true));

        // Think better of it and just close the infobar.
        TestThreadUtils.runOnUiThreadBlocking(infobar::onCloseButtonClicked);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        CriteriaHelper.pollUiThread(() -> !InfoBarContainer.get(tab).hasInfoBars());
    }

    @Test
    @LargeTest
    public void resourceFilteredClickLearnMore() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        MockSafeBrowsingApiHandler.addMockResponse(url, METADATA_FOR_ENFORCEMENT);
        mActivityTestRule.loadUrl(url);

        String loaded = mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded");
        Assert.assertEquals("false", loaded);

        Tab originalTab = mActivityTestRule.getActivity().getActivityTab();
        CallbackHelper tabCreatedCallback = new CallbackHelper();
        TabModel tabModel = mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tabModel.addObserver(new TabModelObserver() {
                    @Override
                    public void didAddTab(
                            Tab tab, @TabLaunchType int type, @TabCreationState int creationState) {
                        if (tab.getUrlString().equals(LEARN_MORE_PAGE)) {
                            tabCreatedCallback.notifyCalled();
                        }
                    }
                }));

        // Check that the infobar is showing.
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        CriteriaHelper.pollUiThread(() -> infoBars.size() == 1);
        AdsBlockedInfoBar infobar = (AdsBlockedInfoBar) infoBars.get(0);

        // Click the link once to expand it.
        TestThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);

        // Click again to navigate, which should spawn a new tab.
        TestThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);

        // Wait for the tab to be added with the correct URL. Note, do not wait for this URL to be
        // loaded since it is not controlled by the test instrumentation. Just waiting for the
        // navigation to start should be OK though.
        tabCreatedCallback.waitForCallback("Never received tab created event", 0);

        // The infobar should not be removed on the original tab.
        CriteriaHelper.pollUiThread(() -> InfoBarContainer.get(originalTab).hasInfoBars());
    }

    @Test
    @LargeTest
    public void resourceFilteredReload() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        MockSafeBrowsingApiHandler.addMockResponse(url, METADATA_FOR_ENFORCEMENT);
        mActivityTestRule.loadUrl(url);

        String loaded = mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded");
        Assert.assertEquals("false", loaded);

        // Check that the infobar is showing.
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        CriteriaHelper.pollUiThread(() -> infoBars.size() == 1);
        AdsBlockedInfoBar infobar = (AdsBlockedInfoBar) infoBars.get(0);

        // Click the link once to expand it.
        TestThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);

        // Check the checkbox and press the button to reload.
        TestThreadUtils.runOnUiThreadBlocking(() -> infobar.onCheckedChanged(null, true));
        TestThreadUtils.runOnUiThreadBlocking(() -> infobar.onButtonClicked(true));

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, url);

        CriteriaHelper.pollUiThread(() -> !InfoBarContainer.get(tab).hasInfoBars());

        // Reloading should allowlist the site, so resources should no longer be filtered.
        loaded = mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded");
        Assert.assertEquals("true", loaded);
    }

    @Test
    @LargeTest
    public void resourceNotFilteredWithWarning() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        MockSafeBrowsingApiHandler.addMockResponse(url, METADATA_FOR_WARNING);
        mActivityTestRule.loadUrl(url);

        String loaded = mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded");
        Assert.assertEquals("true", loaded);

        // Check that the infobar is not showing.
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        CriteriaHelper.pollUiThread(() -> infoBars.isEmpty());
    }
}
