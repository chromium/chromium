// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshot_protection;

import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Intent;
import android.view.WindowManager;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.CctTransitTestRule;
import org.chromium.net.test.EmbeddedTestServer;

/** Custom Tab integration tests for {@link ScreenshotProtectionController} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    // Can't use the @Policies annotation here as DataControlsRules is not allowed as a machine
    // policy. Rule must contain the entire prefix path to match.
    "enable-chrome-browser-cloud-management",
    "policy={\"DataControlsRules\":[{\"restrictions\":"
            + "[{\"class\":\"SCREENSHOT\",\"level\":\"BLOCK\"}],"
            + "\"sources\":{\"urls\":[\"*/chrome/test/data/android/google.html\"]}}]}"
})
@EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_ENTERPRISE_SCREENSHOT_PROTECTION)
@DoNotBatch(reason = "Tests window flags and features which are activity-global")
public class ScreenshotProtectionControllerCctPTTest {

    @Rule public final CctTransitTestRule mCustomTabActivityTestRule = new CctTransitTestRule();

    private String mRestrictedUrl;

    @Before
    public void setUp() {
        EmbeddedTestServer testServer =
                mCustomTabActivityTestRule.getActivityTestRule().getTestServer();
        mRestrictedUrl = testServer.getURL("/chrome/test/data/android/google.html");
    }

    private static boolean isWindowSecure(Activity activity) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int flags = activity.getWindow().getAttributes().flags;
                    return (flags & WindowManager.LayoutParams.FLAG_SECURE) != 0;
                });
    }

    @Test
    @LargeTest
    public void testManagedCustomTab() {
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getInstrumentation().getTargetContext(),
                        mRestrictedUrl);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        assertTrue(
                "Standard Custom Tab window should be secure when managed",
                isWindowSecure(mCustomTabActivityTestRule.getActivity()));
    }
}
