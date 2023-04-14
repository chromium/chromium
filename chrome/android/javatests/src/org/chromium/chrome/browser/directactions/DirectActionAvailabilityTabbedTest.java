// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * Tests the availability of core direct actions in different activities.
 *
 * <p>This tests both {@link DirectActionInitializer} and its integration with {@link
 * ChromeActivity} and its different subclasses.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DirectActionAvailabilityTabbedTest {
    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public DirectActionTestRule mDirectActionRule = new DirectActionTestRule();

    @Before
    public void setUp() throws Exception {
        // Using OnBlank times out when waiting for NTP. Using UrlConstants.NTP_URL makes the test
        // work.
        mTabbedActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
    }

    @Test
    @MediumTest
    @Feature({"DirectActions"})
    public void testCoreDirectActionInTabbedActivity() throws Exception {
        assertThat(DirectActionTestUtils.setupActivityAndGetDirectAction(mTabbedActivityTestRule),
                Matchers.containsInAnyOrder("go_back", "reload", "go_forward", "bookmark_this_page",
                        "downloads", "preferences", "open_history", "help", "new_tab", "close_tab",
                        "close_all_tabs"));
    }
}
