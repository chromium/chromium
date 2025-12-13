// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Color;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Test for various Display Modes of Web Apps. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappDisplayModeTest {
    @Rule public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testStandalone() {
        WebappActivity activity = startActivity(DisplayMode.STANDALONE, "");

        Assert.assertEquals(
                DisplayMode.STANDALONE,
                activity.getIntentDataProvider().getWebappExtras().displayMode);
        Assert.assertFalse(activity.getToolbarManager().getToolbarLayoutForTesting().isShown());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testFullScreen() {
        WebappActivity activity = startActivity(DisplayMode.FULLSCREEN, "");

        Assert.assertEquals(
                DisplayMode.FULLSCREEN,
                activity.getIntentDataProvider().getWebappExtras().displayMode);
        Assert.assertFalse(activity.getToolbarManager().getToolbarLayoutForTesting().isShown());
    }

    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testFullScreenInFullscreen() {
        WebappActivity activity = startActivity(DisplayMode.FULLSCREEN, "fullscreen_on_click");

        Assert.assertEquals(
                DisplayMode.FULLSCREEN,
                activity.getIntentDataProvider().getWebappExtras().displayMode);
        Assert.assertFalse(activity.getToolbarManager().getToolbarLayoutForTesting().isShown());

        WebContents contents = activity.getActivityTab().getWebContents();

        TouchCommon.singleClickView(activity.getActivityTab().getContentView());
        // Poll because clicking races with evaluating js evaluation.
        CriteriaHelper.pollInstrumentationThread(
                () -> getJavascriptResult(contents, "isBodyFullscreen()").equals("true"));

        TouchCommon.singleClickView(activity.getActivityTab().getContentView());
        CriteriaHelper.pollInstrumentationThread(
                () -> getJavascriptResult(contents, "isBodyFullscreen()").equals("false"));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testMinimalUi() {
        WebappActivity activity = startActivity(DisplayMode.MINIMAL_UI, "");

        Assert.assertEquals(
                DisplayMode.MINIMAL_UI,
                activity.getIntentDataProvider().getWebappExtras().displayMode);
        Assert.assertFalse(activity.getToolbarManager().getToolbarLayoutForTesting().isShown());

        Assert.assertEquals(Color.CYAN, activity.getToolbarManager().getPrimaryColor());
    }

    private String getJavascriptResult(WebContents webContents, String js) {
        try {
            return JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, js);
        } catch (TimeoutException e) {
            Assert.fail(
                    "Fatal interruption or timeout running JavaScript '"
                            + js
                            + "': "
                            + e.toString());
            return "";
        }
    }

    private WebappActivity startActivity(@DisplayMode.EnumType int displayMode, String action) {
        String url = WebappTestPage.getTestUrlWithAction(mActivityTestRule.getTestServer(), action);
        mActivityTestRule.startWebappActivity(
                mActivityTestRule
                        .createIntent()
                        .putExtra(WebappConstants.EXTRA_URL, url)
                        .putExtra(WebappConstants.EXTRA_DISPLAY_MODE, displayMode)
                        .putExtra(WebappConstants.EXTRA_THEME_COLOR, (long) Color.CYAN));

        return mActivityTestRule.getActivity();
    }
}
