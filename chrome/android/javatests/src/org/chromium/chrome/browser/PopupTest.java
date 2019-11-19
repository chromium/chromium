// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Tests whether popup windows appear.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PopupTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

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
        // Create a new temporary instance to ensure the Class is loaded. Otherwise we will get a
        // ClassNotFoundException when trying to instantiate during startup.
        SafeBrowsingApiBridge.setSafeBrowsingHandlerType(
                new MockSafeBrowsingApiHandler().getClass());
        mActivityTestRule.startMainActivityOnBlankPage();

        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> Assert.assertTrue(getNumInfobarsShowing() == 0));

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mPopupHtmlUrl = mTestServer.getURL(POPUP_HTML_PATH);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
        MockSafeBrowsingApiHandler.clearMockResponses();
    }

    @Test
    @MediumTest
    @Feature({"Popup"})
    public void testPopupInfobarAppears() {
        mActivityTestRule.loadUrl(mPopupHtmlUrl);
        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> getNumInfobarsShowing()));
    }

    @Test
    @MediumTest
    @Feature({"Popup"})
    public void testSafeGestureTabNotBlocked() throws Exception {
        final TabModelSelector selector = mActivityTestRule.getActivity().getTabModelSelector();

        String url = mTestServer.getURL("/chrome/test/data/android/popup_on_click.html");

        mActivityTestRule.loadUrl(url);
        CriteriaHelper.pollUiThread(Criteria.equals(0, () -> getNumInfobarsShowing()));
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getWebContents(), "link");
        CriteriaHelper.pollUiThread(Criteria.equals(0, () -> getNumInfobarsShowing()));
    }

    @Test
    @MediumTest
    @Feature({"Popup"})
    public void testAbusiveGesturePopupBlocked() throws Exception {
        final TabModelSelector selector = mActivityTestRule.getActivity().getTabModelSelector();

        String url = mTestServer.getURL("/chrome/test/data/android/popup_on_click.html");
        MockSafeBrowsingApiHandler.addMockResponse(url, METADATA_FOR_ABUSIVE_ENFORCEMENT);

        mActivityTestRule.loadUrl(url);
        CriteriaHelper.pollUiThread(Criteria.equals(0, () -> getNumInfobarsShowing()));
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getWebContents(), "link");
        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> getNumInfobarsShowing()));
        Assert.assertEquals(1, selector.getTotalTabCount());
    }

    private void waitForForegroundInfoBar(@InfoBarIdentifier int id) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (getNumInfobarsShowing() == 0) {
                    updateFailureReason("No infobars present");
                    return false;
                }
                InfoBar frontInfoBar = mActivityTestRule.getInfoBars().get(0);
                if (frontInfoBar.getInfoBarIdentifier() != id) {
                    updateFailureReason(String.format(Locale.ENGLISH,
                            "Invalid infobar type shown: %d", frontInfoBar.getInfoBarIdentifier()));
                    frontInfoBar.onCloseButtonClicked();
                    return false;
                }
                return true;
            }
        });
    }

    private void waitForNoInfoBarOfType(@InfoBarIdentifier int id) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                List<InfoBar> infoBars = mActivityTestRule.getInfoBars();
                if (infoBars.isEmpty()) return true;
                for (InfoBar infoBar : infoBars) {
                    if (infoBar.getInfoBarIdentifier() == id) return false;
                }
                return true;
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
        CriteriaHelper.pollUiThread(Criteria.equals(false, () -> container.isAnimating()));
        TouchCommon.singleClickView(infobar.getView().findViewById(R.id.button_primary));

        // Document mode popups appear slowly and sequentially to prevent Android from throwing them
        // away, so use a long timeout.  http://crbug.com/498920.
        CriteriaHelper.pollUiThread(
                Criteria.equals("Two", () -> selector.getCurrentTab().getTitle()), 7500,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForNoInfoBarOfType(InfoBarIdentifier.POPUP_BLOCKED_INFOBAR_DELEGATE_MOBILE);

        Assert.assertEquals(3, selector.getTotalTabCount());
        int currentTabId = selector.getCurrentTab().getId();

        // Test that revisiting the original page makes popup windows immediately.
        mActivityTestRule.loadUrl(mPopupHtmlUrl);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                int tabCount = selector.getTotalTabCount();
                if (tabCount != 5) {
                    updateFailureReason(String.format(
                            Locale.ENGLISH, "Expected 5 tabs, but found: %d", tabCount));
                    return false;
                }

                String tabTitle = selector.getCurrentTab().getTitle();
                updateFailureReason(String.format(
                        Locale.ENGLISH, "Exepcted title 'Two', but found: %s", tabTitle));
                return TextUtils.equals("Two", tabTitle);
            }
        }, 7500, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForNoInfoBarOfType(InfoBarIdentifier.POPUP_BLOCKED_INFOBAR_DELEGATE_MOBILE);

        Assert.assertNotSame(currentTabId, selector.getCurrentTab().getId());
    }
}
