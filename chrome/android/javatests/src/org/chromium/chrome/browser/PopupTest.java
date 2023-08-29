// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests whether popup windows appear.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE)
public class PopupTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String POPUP_HTML_PATH = "/chrome/test/data/android/popup_test.html";

    private static final String METADATA_FOR_ABUSIVE_ENFORCEMENT =
            "{\"matches\":[{\"threat_type\":\"13\",\"sf_absv\":\"\"}]}";

    private String mPopupHtmlUrl;
    private EmbeddedTestServer mTestServer;

    private int getNumInfobarsShowing() {
        return mActivityTestRule.getInfoBars().size();
    }

    @Before
    public void setUp() throws Exception {
        SafeBrowsingApiBridge.setSafetyNetApiHandler(new MockSafetyNetApiHandler());
        mActivityTestRule.startMainActivityOnBlankPage();

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT, () -> Assert.assertTrue(getNumInfobarsShowing() == 0));

        mTestServer = EmbeddedTestServer.createAndStartServer(
                ApplicationProvider.getApplicationContext());
        mPopupHtmlUrl = mTestServer.getURL(POPUP_HTML_PATH);
    }

    @After
    public void tearDown() {
        MockSafetyNetApiHandler.clearMockResponses();
    }

    @Test
    @MediumTest
    @Feature({"Popup"})
    public void testPopupInfobarAppears() {
        mActivityTestRule.loadUrl(mPopupHtmlUrl);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getNumInfobarsShowing(), Matchers.is(1)));
    }

    @Test
    @MediumTest
    @Feature({"Popup"})
    public void testSafeGestureTabNotBlocked() throws Exception {
        final TabModelSelector selector = mActivityTestRule.getActivity().getTabModelSelector();

        String url = mTestServer.getURL("/chrome/test/data/android/popup_on_click.html");

        mActivityTestRule.loadUrl(url);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getNumInfobarsShowing(), Matchers.is(0)));
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getWebContents(), "link");
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getNumInfobarsShowing(), Matchers.is(0)));
    }

    @Test
    @MediumTest
    @Feature({"Popup"})
    public void testAbusiveGesturePopupBlocked() throws Exception {
        final TabModelSelector selector = mActivityTestRule.getActivity().getTabModelSelector();

        String url = mTestServer.getURL("/chrome/test/data/android/popup_on_click.html");
        MockSafetyNetApiHandler.addMockResponse(url, METADATA_FOR_ABUSIVE_ENFORCEMENT);

        mActivityTestRule.loadUrl(url);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getNumInfobarsShowing(), Matchers.is(0)));
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getWebContents(), "link");
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getNumInfobarsShowing(), Matchers.is(1)));
        Assert.assertEquals(1, selector.getTotalTabCount());
    }

    private void waitForForegroundInfoBar(@InfoBarIdentifier int id) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(getNumInfobarsShowing(), Matchers.greaterThan(0));
            InfoBar frontInfoBar = mActivityTestRule.getInfoBars().get(0);
            if (frontInfoBar.getInfoBarIdentifier() != id) frontInfoBar.onCloseButtonClicked();
            Criteria.checkThat("Invalid infobar type shown", frontInfoBar.getInfoBarIdentifier(),
                    Matchers.is(id));
        });
    }

    private void waitForNoInfoBarOfType(@InfoBarIdentifier int id) {
        CriteriaHelper.pollUiThread(() -> {
            List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
            for (InfoBar infoBar : infoBars) {
                Criteria.checkThat(infoBar.getInfoBarIdentifier(), Matchers.not(id));
            }
        });
    }

    @Test
    @MediumTest
    @Feature({"Popup"})
    public void testPopupWindowsAppearWhenAllowed() {
        final TabModelSelector selector = mActivityTestRule.getActivity().getTabModelSelector();

        mActivityTestRule.loadUrl(mPopupHtmlUrl);
        waitForForegroundInfoBar(InfoBarIdentifier.POPUP_BLOCKED_INFOBAR_DELEGATE_MOBILE);
        Assert.assertEquals(1, selector.getTotalTabCount());
        final InfoBarContainer container = mActivityTestRule.getInfoBarContainer();
        ArrayList<InfoBar> infobars = container.getInfoBarsForTesting();

        // Wait until the animations are done, then click the "open popups" button.
        final InfoBar infobar = infobars.get(0);
        Assert.assertEquals(InfoBarIdentifier.POPUP_BLOCKED_INFOBAR_DELEGATE_MOBILE,
                infobar.getInfoBarIdentifier());
        CriteriaHelper.pollUiThread(() -> !container.isAnimating());
        TouchCommon.singleClickView(infobar.getView().findViewById(R.id.button_primary));

        // Document mode popups appear slowly and sequentially to prevent Android from throwing them
        // away, so use a long timeout.  http://crbug.com/498920.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(selector.getCurrentTab().getTitle(), Matchers.is("Two"));
        }, 7500, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForNoInfoBarOfType(InfoBarIdentifier.POPUP_BLOCKED_INFOBAR_DELEGATE_MOBILE);

        Assert.assertEquals(3, selector.getTotalTabCount());
        int currentTabId = selector.getCurrentTab().getId();

        // Test that revisiting the original page makes popup windows immediately.
        mActivityTestRule.loadUrl(mPopupHtmlUrl);
        CriteriaHelper.pollUiThread(() -> {
            int tabCount = selector.getTotalTabCount();
            Criteria.checkThat(tabCount, Matchers.is(5));
            String tabTitle = selector.getCurrentTab().getTitle();
            Criteria.checkThat(tabTitle, Matchers.is("Two"));
        }, 7500, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForNoInfoBarOfType(InfoBarIdentifier.POPUP_BLOCKED_INFOBAR_DELEGATE_MOBILE);

        Assert.assertNotSame(currentTabId, selector.getCurrentTab().getId());
    }
}
