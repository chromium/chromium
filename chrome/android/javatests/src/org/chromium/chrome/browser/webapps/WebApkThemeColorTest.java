// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Color;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.ThemeTestUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests for status bar color with EXTRA_THEME_COLOR specified in the Intent. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebApkThemeColorTest {
    @Rule public final WebApkActivityTestRule mActivityTestRule = new WebApkActivityTestRule();

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.PHONE})
    // Customizing status bar color is disallowed for tablets.
    @Feature({"WebApk"})
    public void testAllowsBrightThemeColor() throws ExecutionException, TimeoutException {
        final int intentThemeColor = Color.MAGENTA;
        final int pageThemeColor = Color.RED;
        final int white = Color.WHITE;
        String pageWithThemeColorUrl =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/theme_color_test.html");
        Intent intent =
                mActivityTestRule
                        .createIntent()
                        .putExtra(WebappConstants.EXTRA_URL, pageWithThemeColorUrl)
                        .putExtra(WebappConstants.EXTRA_THEME_COLOR, (long) intentThemeColor);
        mActivityTestRule.startWebApkActivity(intent);

        ThemeTestUtils.waitForThemeColor(mActivityTestRule.getActivity(), pageThemeColor);
        ThemeTestUtils.assertStatusBarColor(mActivityTestRule.getActivity(), pageThemeColor);

        // WebAPKs are allowed to set page theme color to white.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivityTestRule.getActivity().getActivityTab().getWebContents(),
                "document.querySelector('meta').setAttribute('content', 'white');");

        ThemeTestUtils.waitForThemeColor(mActivityTestRule.getActivity(), white);
        ThemeTestUtils.assertStatusBarColor(mActivityTestRule.getActivity(), white);
    }
}
