// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isFocused;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalHistoryUrl;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.DeviceInput;

/** Tests for the History page on large form factors device. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@Restriction({DeviceFormFactor.TABLET_OR_DESKTOP})
public class HistoryPageOnLffTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    /**
     * Helper method containing the common navigation steps. It starts on a blank page, opens two
     * tabs (one with example.com, one with chrome://history) and switches between them, ending on
     * the history page.
     */
    private void performNavigationToHistoryTab() {
        // 1. Start on a Blank PageStation provided by the test rule.
        WebPageStation blankPage = mCtaTestRule.startOnBlankPage();

        // 2. Load an example URL in the initial tab.
        String exampleUrl = "https://www.example.com/";
        WebPageStation tab1Page = blankPage.loadWebPageProgrammatically(exampleUrl);
        Tab tab1 = tab1Page.getTab();

        // 3. Open a new tab. This transition returns the PageStation for the new tab.
        RegularNewTabPageStation historyPage = tab1Page.openNewTabFast();

        // 4. Load chrome://history/ in the newly opened tab.
        String historyUrl = getOriginalHistoryUrl();
        WebPageStation historyWebPage =
                historyPage.loadPageProgrammatically(
                        historyUrl,
                        WebPageStation.newBuilder().withExpectedUrlSubstring(historyUrl));
        Tab historyTab = historyWebPage.getTab();

        // 5. Switch back to the first tab (tab1Page).
        tab1Page = historyWebPage.selectTabFast(tab1, WebPageStation::newBuilder);

        // 6. Switch back to the history tab (historyPage).
        historyWebPage = tab1Page.selectTabFast(historyTab, WebPageStation::newBuilder);
    }

    @Test
    @MediumTest
    public void testAutoFocusOnHistoryPageByTabSwitching() {
        DeviceInput.setSupportsKeyboardForTesting(true);

        // Pre-check: The test is for devices w/ keyboard
        boolean isKeyboardAttached =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return DeviceInput.supportsKeyboard(mCtaTestRule.getActivity());
                        });
        if (!isKeyboardAttached) return;

        performNavigationToHistoryTab();

        // Now that we've arrived at the historyPage Station,
        // use Espresso to assert that the search box is focused.
        // Public Transit ensures all page loads and tab switches are complete before this.
        onView(withId(R.id.search_text)).check(matches(isFocused()));
    }
}
