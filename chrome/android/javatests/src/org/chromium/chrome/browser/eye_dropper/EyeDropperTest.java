// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.eye_dropper;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Color;
import android.os.Build;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

import java.util.concurrent.TimeoutException;

/** Integration test for the EyeDropper API on Android. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures("EyeDropper")
@Batch(Batch.PER_CLASS)
public class EyeDropperTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private static final String TEST_FILE = "/chrome/test/data/android/eyedropper.html";

    private WebContents mWebContents;

    @Before
    public void setUp() {
        String url = mActivityTestRule.getTestServer().getURL(TEST_FILE);
        WebPageStation pageStation = mActivityTestRule.startOnWebPage(url);
        mWebContents = pageStation.webContentsElement.value();
        DOMUtils.waitForNonZeroNodeBounds(mWebContents, "open_eyedropper");
    }

    /** Verifies that window.EyeDropper is defined on Android. */
    @Test
    @MediumTest
    @Feature({"EyeDropper"})
    public void testEyeDropperDefined() throws TimeoutException {
        String result =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mWebContents, "typeof window.EyeDropper");
        Assert.assertEquals("\"function\"", result);
    }

    /**
     * Verifies that opening the eyedropper with a user gesture on API 37+ launches the system
     * EyeDropper activity and resolves with the selected color.
     */
    @Test
    @MediumTest
    @Feature({"EyeDropper"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.CINNAMON_BUN)
    public void testEyeDropperOpenSuccess() throws Throwable {
        Intent resultData = new Intent();
        resultData.putExtra(Intent.EXTRA_COLOR, Color.RED);
        Instrumentation.ActivityResult activityResult =
                new Instrumentation.ActivityResult(Activity.RESULT_OK, resultData);
        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                new IntentFilter(Intent.ACTION_OPEN_EYE_DROPPER),
                                activityResult,
                                /* block= */ true);
        try {
            DOMUtils.clickNode(mWebContents, "open_eyedropper");

            waitForLastResult(Matchers.is("\"selected: #ff0000\""));
            Assert.assertEquals(1, monitor.getHits());
        } finally {
            InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
        }
    }

    /**
     * Verifies that opening the eyedropper with a user gesture on Android versions below API 37
     * rejects with AbortError because the system EyeDropper API is not supported.
     */
    @Test
    @MediumTest
    @Feature({"EyeDropper"})
    @MaxAndroidSdkLevel(Build.VERSION_CODES.BAKLAVA)
    public void testEyeDropperOpenFailsBelowApi37() throws Throwable {
        DOMUtils.clickNode(mWebContents, "open_eyedropper");

        waitForLastResult(Matchers.startsWith("\"error: AbortError"));
    }

    /**
     * Verifies that EyeDropper.open() without transient user activation rejects with
     * NotAllowedError.
     */
    @Test
    @MediumTest
    @Feature({"EyeDropper"})
    public void testEyeDropperRequiresUserGesture() throws TimeoutException {
        String result =
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        mWebContents,
                        "(async () => {"
                                + "  const eyeDropper = new EyeDropper();"
                                + "  try {"
                                + "    await eyeDropper.open();"
                                + "    window.domAutomationController.send('success');"
                                + "  } catch (e) {"
                                + "    window.domAutomationController.send(e.name);"
                                + "  }"
                                + "})()");
        Assert.assertEquals("\"NotAllowedError\"", result);
    }

    private void waitForLastResult(Matcher<String> matcher) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    String result;
                    try {
                        result =
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        mWebContents, "window.lastResult");
                    } catch (TimeoutException e) {
                        throw new RuntimeException(e);
                    }
                    Criteria.checkThat(result, matcher);
                });
    }
}
