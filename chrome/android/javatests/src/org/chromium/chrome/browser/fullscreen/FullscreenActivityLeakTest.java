// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.os.Build;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.EnableLeakChecks;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.FullscreenTestUtils;

/**
 * Tests that an activity destroyed while in HTML fullscreen is not leaked.
 *
 * <p>Entering fullscreen passes a callback to {@code Activity#requestFullscreenMode()}. The
 * framework sends it to system_server inside a binder object, which stays reachable until
 * system_server releases it. The callback must not keep the activity alive in the meantime.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "The leak check runs after the class, when the activity has been destroyed.")
@EnableLeakChecks
public class FullscreenActivityLeakTest {
    private static final String TEST_PAGE =
            UrlUtils.encodeHtmlDataUri("<html><body>Fullscreen leak test</body></html>");

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Test
    @MediumTest
    @Feature({"Fullscreen"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testActivityDestroyedInFullscreenIsNotLeaked() {
        WebPageStation page = mActivityTestRule.startOnUrl(TEST_PAGE);
        ChromeTabbedActivity activity = page.getActivity();

        FullscreenTestUtils.togglePersistentFullscreen(page.getTab(), true);
        CriteriaHelper.pollUiThread(
                () -> activity.getFullscreenManager().getPersistentFullscreenMode(),
                "The tab should enter persistent fullscreen");

        // The test ends in fullscreen. The rule destroys the activity and @EnableLeakChecks
        // then asserts that it is no longer reachable.
    }
}
