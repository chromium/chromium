// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.test.util.ViewUtils;

/** Integration tests for {@link SigninButtonCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test relies on native initialization")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(SigninFeatures.SIGNIN_LEVEL_UP_BUTTON)
public class SigninButtonCoordinatorTest {
    private final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    private RegularNewTabPageStation mPage;

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnNtp();
        NewTabPageTestUtils.waitForNtpLoaded(mPage.getTab());
    }

    @Test
    @MediumTest
    public void testSigninButtonVisibleOnNtp() {
        // Sign-in button should be visible on NTP.
        ViewUtils.waitForVisibleView(allOf(withId(R.id.signin_button), isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSigninButtonHiddenOnNavigation() {
        // Initially visible on NTP.
        ViewUtils.waitForVisibleView(allOf(withId(R.id.signin_button), isDisplayed()));

        // Should be hidden on navigation away from NTP.
        WebPageStation aboutBlank =
                mPage.loadWebPageProgrammatically(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        onView(withId(R.id.signin_button)).check(matches(not(isDisplayed())));

        // Should be visible again when navigating back to NTP.
        aboutBlank.loadPageProgrammatically(
                getOriginalNativeNtpUrl(), RegularNewTabPageStation.newBuilder());
        ViewUtils.waitForVisibleView(allOf(withId(R.id.signin_button), isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSigninButtonHiddenOnIncognitoNtp() {
        // Initially visible on NTP.
        ViewUtils.waitForVisibleView(allOf(withId(R.id.signin_button), isDisplayed()));

        mPage.openAppMenu().openNewIncognitoTab();

        // Signin button should not be visible on incognito NTP.
        // It may not be inflated yet in the new incognito tab, so we check for both the
        // inflated view and its stub.
        onView(anyOf(withId(R.id.signin_button), withId(R.id.signin_button_stub)))
                .check(matches(not(isDisplayed())));
    }
}
