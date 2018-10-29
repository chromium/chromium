// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.infobar.AdsBlockedInfoBar;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.subresource_filter.TestSubresourceFilterPublisher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.List;

/**
 * End to end tests of SubresourceFilter ad filtering on Android.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class SubresourceFilterTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);
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
        TestSubresourceFilterPublisher publisher = new TestSubresourceFilterPublisher();
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) ()
                        -> publisher.createAndPublishRulesetDisallowingSuffixForTesting(suffix));
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return publisher.isPublished();
            }
        });
    }

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        // Create a new temporary instance to ensure the Class is loaded. Otherwise we will get a
        // ClassNotFoundException when trying to instantiate during startup.
        SafeBrowsingApiBridge.setSafeBrowsingHandlerType(
                new MockSafeBrowsingApiHandler().getClass());
        mActivityTestRule.startMainActivityOnBlankPage();

        // Disallow all jpgs.
        createAndPublishRulesetDisallowingSuffix(".jpg");
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        MockSafeBrowsingApiHandler.clearMockResponses();
    }

    @Test
    @MediumTest
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
    @MediumTest
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
        ThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);

        // Check the checkbox and press the button to reload.
        ThreadUtils.runOnUiThreadBlocking(() -> infobar.onCheckedChanged(null, true));

        // Think better of it and just close the infobar.
        ThreadUtils.runOnUiThreadBlocking(infobar::onCloseButtonClicked);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        CriteriaHelper.pollUiThread(() -> !InfoBarContainer.get(tab).hasInfoBars());
    }

    @Test
    @MediumTest
    public void resourceFilteredClickLearnMore() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        MockSafeBrowsingApiHandler.addMockResponse(url, METADATA_FOR_ENFORCEMENT);
        mActivityTestRule.loadUrl(url);

        String loaded = mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded");
        Assert.assertEquals("false", loaded);

        Tab originalTab = mActivityTestRule.getActivity().getActivityTab();
        CallbackHelper tabCreatedCallback = new CallbackHelper();
        TabModel tabModel = mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        ThreadUtils.runOnUiThreadBlocking(() -> tabModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, @TabModel.TabLaunchType int type) {
                if (tab.getUrl().equals(LEARN_MORE_PAGE)) tabCreatedCallback.notifyCalled();
            }
        }));

        // Check that the infobar is showing.
        List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
        CriteriaHelper.pollUiThread(() -> infoBars.size() == 1);
        AdsBlockedInfoBar infobar = (AdsBlockedInfoBar) infoBars.get(0);

        // Click the link once to expand it.
        ThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);

        // Click again to navigate, which should spawn a new tab.
        ThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);

        // Wait for the tab to be added with the correct URL. Note, do not wait for this URL to be
        // loaded since it is not controlled by the test instrumentation. Just waiting for the
        // navigation to start should be OK though.
        tabCreatedCallback.waitForCallback("Never received tab created event", 0);

        // The infobar should not be removed on the original tab.
        CriteriaHelper.pollUiThread(() -> InfoBarContainer.get(originalTab).hasInfoBars());
    }

    @Test
    @MediumTest
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
        ThreadUtils.runOnUiThreadBlocking(infobar::onLinkClicked);

        // Check the checkbox and press the button to reload.
        ThreadUtils.runOnUiThreadBlocking(() -> infobar.onCheckedChanged(null, true));
        ThreadUtils.runOnUiThreadBlocking(() -> infobar.onButtonClicked(true));

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, url);

        CriteriaHelper.pollUiThread(() -> !InfoBarContainer.get(tab).hasInfoBars());

        // Reloading should whitelist the site, so resources should no longer be filtered.
        loaded = mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded");
        Assert.assertEquals("true", loaded);
    }

    @Test
    @MediumTest
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
