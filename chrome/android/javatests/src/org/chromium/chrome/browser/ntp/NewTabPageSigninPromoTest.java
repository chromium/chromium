// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;

/**
 * Integration tests for {@link SigninPromoCoordinator} launched from {@link NewTabPage} entry
 * point.
 */
@DoNotBatch(reason = "This test relies on native initialization")
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
public class NewTabPageSigninPromoTest {

    private final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();
    private final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    private Tab mTab;
    private NewTabPage mNtp;

    private void openNewTabPage() {
        mActivityTestRule.startFromLauncherAtNtp();
        mTab = mActivityTestRule.getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromo_AccountsNotReady() {
        try (var unused = mSigninTestRule.blockGetAccountsUpdate(/* populateCache= */ false)) {
            openNewTabPage();
            // Check that the sign-in promo is not shown if accounts are not ready.
            onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
        }
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromo_AccountsReady() {
        openNewTabPage();
        // Check that the sign-in promo is displayed this time.
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromo_NotShownAfterSignIn() {
        openNewTabPage();
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));

        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        onView(withId(R.id.signin_promo_view_container))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromoDisplayedWithDefaultUser() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        openNewTabPage();

        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromoDisplayedWithAADCMinorAccount() {
        mSigninTestRule.addAccount(TestAccounts.AADC_MINOR_ACCOUNT);

        openNewTabPage();

        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }
}
