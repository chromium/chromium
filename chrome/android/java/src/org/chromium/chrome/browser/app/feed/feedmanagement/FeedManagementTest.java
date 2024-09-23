// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.app.feed.feedmanagement;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.intent.matcher.IntentMatchers.toPackage;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import android.content.res.Configuration;

import androidx.test.espresso.action.ViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.LocaleUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.net.NetworkChangeNotifier;

import java.io.IOException;
import java.util.Locale;

/** Test the gear menu and management options reachable by the gear menu. */
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "vmodule=metrics_reporter=2"})
public final class FeedManagementTest {
    static final String PACKAGE_NAME = "org.chromium.chrome";

    private UserActionTester mActionTester;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Before
    public void setUp() {
        Configuration config = new Configuration();
        config.setLocale(new Locale("en", "US"));
        LocaleUtils.setDefaultLocalesFromConfiguration(config);

        mActivityTestRule.startMainActivityOnBlankPage();
        // EULA must be accepted, and internet connectivity is required, or the Feed will not
        // attempt to load.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NetworkChangeNotifier.forceConnectivityState(true);
                    FirstRunUtils.setEulaAccepted();
                });
        mActionTester = new UserActionTester();
        // Initialize the intent catcher/matcher.
        Intents.init();
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
        Intents.release();
    }

    @Test
    @MediumTest
    public void launchNtp_launchFeedManagement() throws IOException, InterruptedException {
        // The web feed requires login to enable, so we must log in first.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        // Load the NTP.
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        // Bring up the gear icon menu.
        onView(withId(R.id.header_menu)).perform(ViewActions.click());
        // Choose "Manage" to go to the Feed management activity.
        onView(withText("Manage")).perform(ViewActions.click());
    }

    @Test
    @MediumTest
    public void launchNtp_launchActivitySettings() throws IOException, InterruptedException {
        // The web feed requires login to enable, so we must log in first.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        // Load the NTP.
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);

        // Bring up the gear icon menu.
        onView(withId(R.id.header_menu)).perform(ViewActions.click());

        // Choose "Manage" to go to the Feed management activity.
        onView(withText("Manage")).perform(ViewActions.click());

        // Launch the page for the selected activity.
        onView(withText(R.string.feed_manage_activity)).perform(ViewActions.click());

        // Verifies that the Feed Interestitial Activity received an intent
        // with the correct package name and data.
        intended(
                allOf(
                        toPackage(PACKAGE_NAME),
                        hasData("https://myactivity.google.com/myactivity?product=50")));
    }
}
