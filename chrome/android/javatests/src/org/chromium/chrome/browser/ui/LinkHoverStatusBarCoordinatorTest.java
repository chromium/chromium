// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Tests for {@link LinkHoverStatusBarCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction({DeviceFormFactor.DESKTOP})
@Batch(Batch.PER_CLASS)
public class LinkHoverStatusBarCoordinatorTest {

    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Test
    @SmallTest
    public void testHoverShowAndHide() throws TimeoutException {
        // 1. Start on a Blank PageStation provided by the test rule.
        WebPageStation blankPage = mCtaTestRule.startOnBlankPage();

        // 2. Load an example URL in the initial tab.
        String exampleUrl = mTestServerRule.getServer().getURL("/chrome/test/data/click.html");
        WebPageStation tabPage = blankPage.loadWebPageProgrammatically(exampleUrl);

        final TabWebContentsDelegateAndroid delegate =
                TabTestUtils.getTabWebContentsDelegate(tabPage.getTab());

        // Simulate hovering over a link.
        String hoverUrl = mTestServerRule.getServer().getURL("/links.html");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    delegate.onUpdateTargetUrl(new GURL(hoverUrl));
                });

        // 4. Verify Status Bar Visibility and Text
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        onView(withText(containsString(hoverUrl))).check(matches(isDisplayed()));
                        return true;
                    } catch (Exception e) {
                        return false;
                    }
                },
                "Link hover status bar did not appear or did not contain '" + hoverUrl + "'");
    }
}
