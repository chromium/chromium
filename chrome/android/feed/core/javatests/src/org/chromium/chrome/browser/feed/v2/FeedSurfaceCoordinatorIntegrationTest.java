// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isFocusable;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.core.IsNot.not;

import androidx.test.espresso.action.ViewActions;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.net.NetworkChangeNotifier;

import java.io.IOException;

/** Integration tests for the {@link FeedSurfaceCoordinator} class. */
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "vmodule=metrics_reporter=2"})
public final class FeedSurfaceCoordinatorIntegrationTest {
    static final String PACKAGE_NAME = "org.chromium.chrome";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        // EULA must be accepted, and internet connectivity is required, or the Feed will not
        // attempt to load.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NetworkChangeNotifier.forceConnectivityState(true);
                    FirstRunUtils.setEulaAccepted();
                });
    }

    /** Test for turning the feed on and off via the gear menu. */
    @Test
    @MediumTest
    public void launchNtp_disableAndEnableViaGearMenu() throws IOException, InterruptedException {
        // The web feed requires login to enable, so we must log in first.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        // Load the NTP.
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);

        // Make sure the eye icon starts off invisible, tab views enabled.
        onView(withId(R.id.section_status_indicator)).check(matches(not(isDisplayed())));
        // We need to select the TabView which is a parent of the text view with "Following".
        onView(
                        allOf(
                                isFocusable(),
                                withContentDescription("Following"),
                                hasDescendant(withText("Following"))))
                .check(matches(isEnabled()));

        // Bring up the gear icon menu, and turn off the feed.
        onView(withId(R.id.header_menu)).perform(ViewActions.click());
        onView(withText("Turn off")).perform(ViewActions.click());

        // Verify that the eye icon appears, and the tab view disables.
        onView(withId(R.id.section_status_indicator)).check(matches(isDisplayed()));
        // Make sure the tab gets disabled.
        onView(
                        allOf(
                                isFocusable(),
                                withContentDescription("Following"),
                                hasDescendant(withText("Following"))))
                .check(matches(not(isEnabled())));

        // Turn the feed back on.
        onView(withId(R.id.header_menu)).perform(ViewActions.click());
        onView(withText("Turn on")).perform(ViewActions.click());

        // Verify that the eye icon is gone, and the text is enabled.
        onView(withId(R.id.section_status_indicator)).check(matches(not(isDisplayed())));
        onView(
                        allOf(
                                isFocusable(),
                                withContentDescription("Following"),
                                hasDescendant(withText("Following"))))
                .check(matches(isEnabled()));
    }
}
