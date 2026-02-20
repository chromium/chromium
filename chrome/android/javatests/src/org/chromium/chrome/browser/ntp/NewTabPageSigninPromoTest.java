// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import static org.chromium.base.test.transit.ViewFinder.waitForNoView;

import android.widget.TextView;

import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.DeviceFormFactor;

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

    private final SigninTestUtil.CustomDeviceLockActivityLauncher mDeviceLockActivityLauncher =
            new SigninTestUtil.CustomDeviceLockActivityLauncher();

    private Tab mTab;
    private NewTabPage mNtp;

    @Before
    public void setUp() {
        DeviceLockActivityLauncherImpl.setInstanceForTesting(mDeviceLockActivityLauncher);
    }

    @After
    public void tearDown() {
        DeviceLockActivityLauncherImpl.setInstanceForTesting(null);
    }

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
        try (var unused = mSigninTestRule.blockGetAccountsUpdate()) {
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

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/twoButtons"
                + "/seamless-signin-string-type/signinButton"
    })
    // TODO(crbug.com/483105856): Test is flaky on desktop bots.
    @DisableIf.Device(DeviceFormFactor.DESKTOP)
    public void testSigninPromoLoadingState() {
        openNewTabPage();
        // An account with an unknown hosted domain emulates a long sign-in. This way the loading
        // state will be shown for a longer time.
        AccountInfo accountHostedDomainUnknown =
                new AccountInfo.Builder(
                                "test@example.com",
                                FakeAccountManagerFacade.toGaiaId("test@example.com"))
                        .build();
        mSigninTestRule.addAccount(accountHostedDomainUnknown);
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));

        onView(withId(R.id.signin_promo_primary_button)).perform(scrollTo(), click());
        onView(withId(R.id.signin_promo_primary_button))
                .check(matches(allOf(isDisplayed(), not(isEnabled()))));
        onView(withId(R.id.signin_promo_dismiss_button))
                .check(matches(withEffectiveVisibility(Visibility.INVISIBLE)));
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/twoButtons"
                + "/seamless-signin-string-type/signinButton"
    })
    @DisabledTest(message = "crbug.com/483438567")
    public void testSeamlessSigninFlow_WithFinalSnackbarUndoSignin() {
        mSigninTestRule.addAccount(TestAccounts.AADC_ADULT_ACCOUNT);
        openNewTabPage();
        // Check that the sign-in promo is displayed for a signed-out user.
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        // Click the sign-in button.
        // For landscape mode tests, scroll to ensure button is visible.
        onView(withId(R.id.signin_promo_primary_button)).perform(scrollTo(), click());
        // Handle Automotive Device Lock (for Automotive Tests).
        SigninTestUtil.completeDeviceLockIfOnAutomotive(mDeviceLockActivityLauncher);
        // Wait for promo to disappear.
        waitForNoView(withId(R.id.signin_promo_view_container));
        // Once the sign-in promo disappears, the sign-in flow is complete.
        assertNotNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));

        // Click "Undo" button in the snackbar. This should sign the user out and permanently
        // dismiss the promo.
        verifySigninSnackbarShown();
        clickSigninSnackbarActionButton();
        verifySignoutSnackbarShown();

        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        onView(withId(R.id.signin_promo_view_container))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
    }

    private void verifySigninSnackbarShown() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    String email = TestAccounts.AADC_ADULT_ACCOUNT.getEmail();
                    String expectedSnackbarSigninMessage =
                            activity.getString(R.string.snackbar_signed_in_as, email);
                    SnackbarManager snackbarManager = activity.getSnackbarManager();
                    Criteria.checkThat(snackbarManager.isShowing(), Matchers.is(true));
                    TextView snackbarMessage = activity.findViewById(R.id.snackbar_message);
                    Snackbar snackbar = snackbarManager.getCurrentSnackbarForTesting();

                    Criteria.checkThat(
                            snackbar.getIdentifierForTesting(), Matchers.is(Snackbar.UMA_SIGN_IN));
                    Criteria.checkThat(snackbarMessage, Matchers.notNullValue());
                    Criteria.checkThat(
                            snackbarMessage.getText().toString(),
                            Matchers.is(expectedSnackbarSigninMessage));
                    Criteria.checkThat(snackbar.getController(), Matchers.notNullValue());
                });
    }

    private void clickSigninSnackbarActionButton() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Snackbar snackbar =
                            activity.getSnackbarManager().getCurrentSnackbarForTesting();
                    assertEquals(
                            "Expecting to click on the sign-in snackbar",
                            Snackbar.UMA_SIGN_IN,
                            snackbar.getIdentifierForTesting());
                    snackbar.getController().onAction(snackbar.getActionData());
                });
    }

    private void verifySignoutSnackbarShown() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    SnackbarManager snackbarManager = activity.getSnackbarManager();
                    Criteria.checkThat(snackbarManager.isShowing(), Matchers.is(true));
                    Snackbar snackbar = snackbarManager.getCurrentSnackbarForTesting();
                    Criteria.checkThat(
                            snackbar.getIdentifierForTesting(), Matchers.is(Snackbar.UMA_SIGN_OUT));
                });
    }
}
