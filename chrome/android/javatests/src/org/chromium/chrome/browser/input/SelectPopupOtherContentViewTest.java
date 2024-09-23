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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.ui.base.ViewAndroidDelegate;

/** Test the select popup and how it interacts with another WebContents. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SelectPopupOtherContentViewTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

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

    private boolean isSelectPopupVisibleOnUiThread() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        WebContentsUtils.isSelectPopupVisible(
                                mActivityTestRule.getWebContents()));
    }

    /**
     * Tests that the showing select popup does not get closed because an unrelated ContentView gets
     * destroyed.
     */
    @Test
    @LargeTest
    @Feature({"Browser"})
    public void testPopupNotClosedByOtherContentView() throws Exception, Throwable {
        // Load the test page.
        mActivityTestRule.startMainActivityWithURL(SELECT_URL);

        // Once clicked, the popup should show up.
        DOMUtils.clickNode(mActivityTestRule.getWebContents(), "select");
        CriteriaHelper.pollInstrumentationThread(
                this::isSelectPopupVisibleOnUiThread, "The select popup did not show up on click.");

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
