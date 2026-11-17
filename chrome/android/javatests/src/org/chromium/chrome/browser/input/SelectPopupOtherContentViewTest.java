// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.input;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.WebPageFacility;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewAndroidDelegate;

/** Test the select popup and how it interacts with another WebContents. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SelectPopupOtherContentViewTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String SELECT_URL =
            UrlUtils.encodeHtmlDataUri(
                    "<html><body>"
                            + "Which animal is the strongest:<br/>"
                            + "<select id=\"select\">"
                            + "<option>Black bear</option>"
                            + "<option>Polar bear</option>"
                            + "<option>Grizzly</option>"
                            + "<option>Tiger</option>"
                            + "<option>Lion</option>"
                            + "<option>Gorilla</option>"
                            + "<option>Chipmunk</option>"
                            + "</select>"
                            + "</body></html>");

    private static class SelectPageFacility extends WebPageFacility {
        public final HtmlElement selectElement = declareHtmlElement("#select");
    }

    private boolean isSelectPopupVisibleOnUiThread() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> WebContentsUtils.isSelectPopupVisible(mActivityTestRule.getWebContents()));
    }

    /**
     * Tests that the showing select popup does not get closed because an unrelated ContentView gets
     * destroyed.
     */
    @Test
    @LargeTest
    @Feature({"Browser"})
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/481445014
    public void testPopupNotClosedByOtherContentView() throws Exception, Throwable {
        // Load the test page.
        SelectPageFacility selectFacility = new SelectPageFacility();
        WebPageStation page =
                mActivityTestRule.startOnPage(
                        SELECT_URL, WebPageStation.newBuilder().withFacility(selectFacility));

        // Once clicked, the popup should show up.
        selectFacility
                .selectElement
                .clickTo()
                .waitFor(
                        SimpleConditions.uiThreadCondition(
                                "Select popup is visible",
                                page.webContentsElement,
                                wc ->
                                        Condition.whether(
                                                WebContentsUtils.isSelectPopupVisible(wc))));

        // Now create and destroy a different WebContents.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebContents webContents =
                            WebContentsFactory.createWebContents(
                                    ProfileManager.getLastUsedRegularProfile(), false, false);
                    ChromeActivity activity = mActivityTestRule.getActivity();

                    ContentView cv = ContentView.createContentView(activity, webContents);
                    webContents.setDelegates(
                            "",
                            ViewAndroidDelegate.createBasicDelegate(cv),
                            cv,
                            activity.getWindowAndroid(),
                            WebContents.createDefaultInternalsHolder());
                    webContents.destroy();
                });

        // Process some more events to give a chance to the dialog to hide if it were to.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // The popup should still be shown.
        Assert.assertTrue(
                "The select popup got hidden by destroying of unrelated ContentViewCore.",
                isSelectPopupVisibleOnUiThread());
    }
}
