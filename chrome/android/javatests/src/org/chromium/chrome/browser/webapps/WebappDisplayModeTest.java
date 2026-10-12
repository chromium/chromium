// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Color;
import android.net.Uri;
import android.os.SystemClock;
import android.view.View;
import android.widget.TextView;

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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
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
    private static final String WEB_APP_PAGE_TITLE = "Web app banner test page";

    private static final String WEB_APP_PATH = "/chrome/test/data/banners/manifest_test_page.html";

    // Stay well above ViewConfiguration.getDoubleTapTimeout() (300ms) and allow async
    // fullscreen transitions to complete before retrying a dropped click, since the page
    // click handler toggles fullscreen rather than setting it idempotently.
    private static final long FULLSCREEN_CLICK_RETRY_INTERVAL_MS = 1000L;

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

        Tab tab = activity.getActivityTab();
        clickAndWaitForFullscreen(tab, true);
        clickAndWaitForFullscreen(tab, false);
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
        Assert.assertTrue(activity.getToolbarManager().getToolbarLayoutForTesting().isShown());

        Assert.assertEquals(Color.CYAN, activity.getToolbarManager().getPrimaryColor());
        Assert.assertEquals(
                "Web App title should be displayed on the title bar",
                WEB_APP_PAGE_TITLE,
                ((TextView) activity.findViewById(R.id.title_bar)).getText());
        Assert.assertEquals(
                "URL Bar should display URL authority",
                Uri.parse(mActivityTestRule.getTestServer().getURL(WEB_APP_PATH)).getAuthority(),
                ((UrlBar) activity.findViewById(R.id.url_bar)).getText().toString());
        Assert.assertEquals(
                "CCT Close button should not be visible",
                View.GONE,
                activity.findViewById(R.id.close_button).getVisibility());
    }

    private void clickAndWaitForFullscreen(Tab tab, boolean expectedFullscreen) {
        WebContents contents = tab.getWebContents();
        View contentView = tab.getContentView();
        String expected = Boolean.toString(expectedFullscreen);

        TouchCommon.singleClickView(contentView);
        long[] lastClickTimeMs = {SystemClock.uptimeMillis()};

        // Poll at the default interval so an immediate fullscreen transition resolves quickly,
        // while throttling click retries if the initial click raced with freeform window
        // placement transitions.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    if (getJavascriptResult(contents, "isBodyFullscreen()").equals(expected)) {
                        return true;
                    }
                    long now = SystemClock.uptimeMillis();
                    if (now - lastClickTimeMs[0] >= FULLSCREEN_CLICK_RETRY_INTERVAL_MS) {
                        TouchCommon.singleClickView(contentView);
                        lastClickTimeMs[0] = now;
                    }
                    return false;
                });
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

        WebappActivity activity = mActivityTestRule.getActivity();
        ChromeTabUtils.waitForInteractable(activity.getActivityTab());
        return activity;
    }
}
