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
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.text.format.DateUtils;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.espresso.ViewAction;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.GeneralSwipeAction;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Swipe;
import androidx.test.espresso.contrib.RecyclerViewActions;
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
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.educational_tip.EducationalTipModuleUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.setup_list.SetupListManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.signin_promo.NtpSigninPromoDelegate;
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
    private static final int SIGNIN_PROMO_POSITION = 2;

    // Espresso ViewAction that performs a swipe from center to left across the vertical center
    // of the view. Used instead of ViewAction.swipeLeft which swipes from right edge to
    // avoid conflict with gesture navigation UI which consumes the edge swipe.
    private static final ViewAction SWIPE_LEFT =
            new GeneralSwipeAction(
                    Swipe.FAST, GeneralLocation.CENTER, GeneralLocation.CENTER_LEFT, Press.FINGER);

    private final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();
    private final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private SetupListManager mSetupListManager;

    private final SigninTestUtil.CustomDeviceLockActivityLauncher mDeviceLockActivityLauncher =
            new SigninTestUtil.CustomDeviceLockActivityLauncher();

    @Before
    public void setUp() {
        DeviceLockActivityLauncherImpl.setInstanceForTesting(mDeviceLockActivityLauncher);

        mActivityTestRule.startOnBlankPage();

        Mockito.when(mSetupListManager.isSetupListActive()).thenReturn(false);
        SetupListManager.setInstanceForTesting(mSetupListManager);
        EducationalTipModuleUtils.setEducationalTipActiveForTesting(false);
    }

    @After
    public void tearDown() {
        DeviceLockActivityLauncherImpl.setInstanceForTesting(null);
    }

    private void openNewTabPage() {
        mActivityTestRule.loadUrlInNewTab(getOriginalNativeNtpUrl());
        Tab tab = mActivityTestRule.getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        Assert.assertTrue(tab.getNativePage() instanceof NewTabPage);
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    // Restrict to Phones and Tablets because Desktop Android does not show feed in NTP.
    @Restriction({DeviceFormFactor.PHONE_OR_TABLET})
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSignInPromo_AccountsNotReady_Legacy() {
        try (var unused = mSigninTestRule.blockGetAccountsUpdate()) {
            openNewTabPage();
            // Check that the sign-in promo is not shown if accounts are not ready.
            onView(withId(R.id.feed_stream_recycler_view))
                    .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
            onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
        }
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
    // Restrict to Phones and Tablets because Desktop Android does not show feed in NTP.
    @Restriction({DeviceFormFactor.PHONE_OR_TABLET})
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSignInPromo_AccountsReady_Legacy() {
        openNewTabPage();
        // Check that the sign-in promo is displayed this time.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        verifySigninPromoShown();
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromo_AccountsReady() {
        openNewTabPage();
        // Check that the sign-in promo is displayed this time.
        verifySigninPromoShown();
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    // Restrict to Phones and Tablets because Desktop Android does not show feed in NTP.
    @Restriction({DeviceFormFactor.PHONE_OR_TABLET})
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSignInPromo_NotShownAfterSignIn_Legacy() {
        openNewTabPage();
        // Check that the sign-in promo is displayed.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        verifySigninPromoShown();

        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromo_NotShownAfterSignIn() {
        openNewTabPage();
        verifySigninPromoShown();

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

        verifySigninPromoShown();
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    // Restrict to Phones and Tablets because Desktop Android does not show feed in NTP.
    @Restriction({DeviceFormFactor.PHONE_OR_TABLET})
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSignInPromoDisplayedWithAADCMinorAccount_Legacy() {
        mSigninTestRule.addAccount(TestAccounts.AADC_MINOR_ACCOUNT);

        openNewTabPage();
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));

        // Check that the sign-in promo is displayed.
        verifySigninPromoShown();
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromoDisplayedWithAADCMinorAccount() {
        mSigninTestRule.addAccount(TestAccounts.AADC_MINOR_ACCOUNT);

        openNewTabPage();

        verifySigninPromoShown();
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
        verifySigninPromoShown();
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
    // TODO(crbug.com/483438567): Test is flaky on desktop bots.
    @DisableIf.Device(DeviceFormFactor.DESKTOP)
    public void testSeamlessSigninFlow_WithFinalSnackbarUndoSignin() {
        mSigninTestRule.addAccount(TestAccounts.AADC_ADULT_ACCOUNT);
        openNewTabPage();
        // Check that the sign-in promo is displayed for a signed-out user.
        verifySigninPromoShown();
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

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    // Restrict to Phones and Tablets because Desktop Android does not show feed in NTP.
    @Restriction({DeviceFormFactor.PHONE_OR_TABLET})
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSignInPromo_HiddenWhenSetupListActive_Legacy() {
        Mockito.when(mSetupListManager.isSetupListActive()).thenReturn(true);

        openNewTabPage();

        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));

        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    public void testSignInPromo_HiddenWhenSetupListActive() {
        Mockito.when(mSetupListManager.isSetupListActive()).thenReturn(true);

        openNewTabPage();

        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @DisabledTest(message = "https://crbug.com/40116614")
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSignInPromo_DismissBySwipe() {
        openNewTabPage();
        boolean dismissed =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);
        if (dismissed) {
            ChromeSharedPreferences.getInstance()
                    .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);
        }

        // Verify that sign-in promo is displayed initially.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(SIGNIN_PROMO_POSITION));
        verifySigninPromoShown();

        // Swipe away the sign-in promo.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(
                        RecyclerViewActions.actionOnItemAtPosition(
                                SIGNIN_PROMO_POSITION, SWIPE_LEFT));

        NewTabPage newTabPage = (NewTabPage) mActivityTestRule.getActivityTab().getNativePage();
        ViewGroup view = (ViewGroup) newTabPage.getCoordinatorForTesting().getRecyclerView();
        waitForNoView(withId(R.id.signin_promo_view_container));
        waitForView(view, allOf(withId(R.id.header_title), isDisplayed()));

        // Verify that sign-in promo is gone, but new tab page layout and header are displayed.
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
        onView(withId(R.id.header_title)).check(matches(isDisplayed()));
        onView(withId(R.id.ntp_content)).check(matches(isDisplayed()));

        // Reset state.
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, dismissed);
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/continueButton"
    })
    public void testSignInPromo_shownIfTimeElapsedSinceFirstShownIsLessThanFirstShownLimit() {
        // Show the promo for the first time.
        openNewTabPage();
        verifySigninPromoShown();

        // Advance time, but not beyond the first time shown limit.
        mFakeTimeTestRule.advanceMillis(
                (NtpSigninPromoDelegate.NTP_SYNC_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS - 1)
                        * DateUtils.HOUR_IN_MILLIS);

        // Open a new tab, the promo should still be shown.
        openNewTabPage();
        verifySigninPromoShown();
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @EnableFeatures({
        "EnableSeamlessSignin"
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/continueButton"
    })
    public void
            testSignInPromo_shownIfTimeElapsedSinceFirstShownExceedsFirstShownLimitAndResetThreshold() {
        // Show the promo for the first time.
        openNewTabPage();
        verifySigninPromoShown();

        // Advance time beyond the the first time shown limit and the last time shown reset period.
        mFakeTimeTestRule.advanceMillis(
                (NtpSigninPromoDelegate.NTP_SYNC_PROMO_RESET_AFTER_DAYS * DateUtils.DAY_IN_MILLIS));
        // Open a new tab, the promo should be shown.
        openNewTabPage();
        verifySigninPromoShown();
    }

    private void verifySigninPromoShown() {
        // The use of `scrollTo` helps fixing flakiness on desktop bots.
        onView(withId(R.id.signin_promo_view_container))
                .perform(scrollTo())
                .check(matches(isDisplayed()));
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
