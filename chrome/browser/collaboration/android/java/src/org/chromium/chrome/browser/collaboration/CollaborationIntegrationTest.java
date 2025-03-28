// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.data_sharing.DataSharingServiceImpl;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;

/** Instrumentation tests for {@link CollaborationService}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.DATA_SHARING})
@DoNotBatch(reason = "Tabs can't be closed reliably between tests.")
// TODO(crbug.com/399444939) Re-enable on automotive devices if needed.
// Only run on device non-auto and with valid Google services.
@Restriction({
    DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_20W02
})
public class CollaborationIntegrationTest {
    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule(order = 1)
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Profile mProfile;
    private String mUrl;

    @Before
    public void setUp() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = mActivityTestRule.getProfile(/* incognito= */ false);
                    FirstRunStatus.setFirstRunFlowComplete(true);
                });
        mUrl =
                DataSharingServiceImpl.getDataSharingUrlForTesting(
                                new GroupToken("collaboration_id", "access_token"))
                        .getSpec();
    }

    @Test
    @MediumTest
    public void testDataSharingUrlInterceptionRefuseSignin() {
        Assert.assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));
        mActivityTestRule.loadUrlInNewTab(mUrl);

        // Verify that the fullscreen sign-in promo is shown and cancel.
        onViewWaiting(withText(R.string.collaboration_signin_description))
                .check(matches(isDisplayed()));
        onView(withText(R.string.collaboration_cancel)).perform(scrollTo(), click());

        // The new data sharing url was intercepted and the tab closed.
        Assert.assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));
    }

    @Test
    @MediumTest
    public void testDataSharingUrlInterceptionExternalAppRefuseSignin() {
        mActivityTestRule.loadUrlInNewTab(
                mUrl, /* incognito= */ false, TabLaunchType.FROM_EXTERNAL_APP);

        // Verify that the fullscreen sign-in promo is shown and cancel.
        onViewWaiting(withText(R.string.collaboration_signin_description))
                .check(matches(isDisplayed()));
        onView(withText(R.string.collaboration_cancel)).perform(scrollTo(), click());

        // The new data sharing url was intercepted and the tab closed.
        Assert.assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));
    }

    @Test
    @MediumTest
    public void testDataSharingUrlInterceptionRefuseSync() {
        mActivityTestRule.loadUrlInNewTab(mUrl);

        // Verify that the fullscreen sign-in promo is shown and accept.
        onViewWaiting(withText(R.string.collaboration_signin_description))
                .check(matches(isDisplayed()));
        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.sync_promo_continue_as,
                                TestAccounts.ACCOUNT1.getGivenName());
        onView(withText(continueAsText)).perform(click());

        // Verify that the history opt-in dialog is shown and refuse.
        onViewWaiting(withText(R.string.collaboration_sync_description))
                .check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.button_secondary)).perform(click());

        // The user is signed out.
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }
}
