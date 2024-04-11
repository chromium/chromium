// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;

import android.content.Intent;
import android.content.res.Configuration;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Integration tests for the re-FRE. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test relies on native initialization")
@Features.EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/41496906): Tests temporarily disabled for automotive. They should be
// re-enabled once the new sign-in flow is implemented for automotive.
// TODO(crbug.com/332854339,crubg.com/333608711): Figure out why the tests are failing on one
// specific bot and fix them. This restriction is just temporary in order to avoid entirely
// disabling the tests.
@Restriction({
    DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    ChromeRestriction.RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES
})
public class UpgradePromoIntegrationTest {
    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule(order = 1)
    public final BaseActivityTestRule<SigninAndHistoryOptInActivity> mActivityTestRule =
            new BaseActivityTestRule(SigninAndHistoryOptInActivity.class);

    private SigninAndHistoryOptInActivity mActivity;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mSigninTestRule.addAccountAndWaitForSeeding(AccountManagerTestRule.TEST_ACCOUNT_1);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_refuseSignin() {
        launchActivity();

        // Verify that the fullscreen sign-in promo is shown and refuse.
        onView(withId(org.chromium.chrome.test.R.id.fullscreen_signin))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.test.R.id.signin_fre_dismiss_button)).perform(click());

        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_refuseHistorySync() {
        launchActivity();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onView(withId(org.chromium.chrome.test.R.id.fullscreen_signin))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.test.R.id.signin_fre_continue_button)).perform(click());

        // Verify that the history opt-in dialog is shown and refuse.
        onView(withId(org.chromium.chrome.test.R.id.history_sync)).check(matches(isDisplayed()));
        onView(
                        allOf(
                                withId(org.chromium.chrome.test.R.id.button_secondary),
                                isCompletelyDisplayed()))
                .perform(click());

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_acceptHistorySync() {
        launchActivity();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onView(withId(org.chromium.chrome.test.R.id.fullscreen_signin))
                .check(matches(isDisplayed()));
        onView(withId(org.chromium.chrome.test.R.id.signin_fre_continue_button)).perform(click());

        // Verify that the history opt-in dialog is shown and accept.
        onView(withId(org.chromium.chrome.test.R.id.history_sync)).check(matches(isDisplayed()));
        onView(allOf(withId(org.chromium.chrome.test.R.id.button_primary), isCompletelyDisplayed()))
                .perform(click());

        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testUserAlreadySignedIn_onlyShowsHistorySync() {
        mSigninTestRule.addTestAccountThenSignin();

        launchActivity();

        // Verify that the history opt-in dialog is shown and accept.
        onView(withId(org.chromium.chrome.test.R.id.history_sync)).check(matches(isDisplayed()));
        onView(allOf(withId(org.chromium.chrome.test.R.id.button_primary), isCompletelyDisplayed()))
                .perform(click());

        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    // There is an issue causing {@link Activity.setRequestedOrientation} to throw an exception in
    // Android 8 which was fixed in Android 8.1. See b/70718000 for example.
    @MinAndroidSdkLevel(Build.VERSION_CODES.O_MR1)
    public void testScreenRotation() {
        launchActivity();

        // Rotate the screen.
        ActivityTestUtils.rotateActivityToOrientation(
                mActivity, Configuration.ORIENTATION_LANDSCAPE);

        // Verify that the view switcher is displayed with the correct layout.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            onView(withId(org.chromium.chrome.test.R.id.upgrade_promo_portrait))
                    .check(matches(isDisplayed()));
        } else {
            onView(withId(org.chromium.chrome.test.R.id.upgrade_promo_landscape))
                    .check(matches(isDisplayed()));
        }
        onView(withId(org.chromium.chrome.test.R.id.fullscreen_signin))
                .check(matches(isDisplayed()));

        // Sign in.
        onView(withId(org.chromium.chrome.test.R.id.signin_fre_continue_button)).perform(click());

        // Verify that the view switcher is displayed with the correct layout.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            onView(withId(org.chromium.chrome.test.R.id.upgrade_promo_portrait))
                    .check(matches(isDisplayed()));
        } else {
            onView(withId(org.chromium.chrome.test.R.id.upgrade_promo_landscape))
                    .check(matches(isDisplayed()));
        }
        onView(withId(org.chromium.chrome.test.R.id.history_sync)).check(matches(isDisplayed()));

        // Rotate the screen back.
        ActivityTestUtils.rotateActivityToOrientation(
                mActivity, Configuration.ORIENTATION_PORTRAIT);
        onView(withId(org.chromium.chrome.test.R.id.upgrade_promo_portrait))
                .check(matches(isDisplayed()));

        // Accept history sync.
        onView(allOf(withId(org.chromium.chrome.test.R.id.button_primary), isCompletelyDisplayed()))
                .perform(click());

        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    private void launchActivity() {
        Intent intent =
                SigninAndHistoryOptInActivity.createIntentForUpgradePromo(
                        ApplicationProvider.getApplicationContext());
        mActivityTestRule.launchActivity(intent);
        mActivity = mActivityTestRule.getActivity();
    }
}
