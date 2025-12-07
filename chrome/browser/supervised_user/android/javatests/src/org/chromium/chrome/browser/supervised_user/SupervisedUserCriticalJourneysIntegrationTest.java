// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isClickable;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;

import android.os.Build;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.SupportedProfileType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;

/** Verifies the main user journeys for supervised users. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SupervisedUserCriticalJourneysIntegrationTest {
    private static final String BLOCKED_SITE_URL = "www.example.com";
    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    public final SigninTestRule mSigninTestRule = new SigninTestRule();
    private WebContents mWebContents;

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    private RegularNewTabPageStation mNtp;

    @Before
    public void setUp() {
        mNtp = mActivityTestRule.startFromLauncherAtNtp();
        mSigninTestRule.addChildTestAccountThenWaitForSignin();
        mWebContents = mActivityTestRule.getWebContents();
    }

    @Test
    @LargeTest
    public void sitesThatAreOnBlocklistAreBlockedByInterstitialPage() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SupervisedUserSettingsTestUtils.addUrlToBlocklist(
                            mActivityTestRule.getProfile(/* incognito= */ false), BLOCKED_SITE_URL);
                });

        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        String blockedHost = testServer.getURLWithHostName(BLOCKED_SITE_URL, "/");
        mActivityTestRule.loadUrl(blockedHost);

        Tab tab = mActivityTestRule.getActivityTab();
        Assert.assertTrue(tab.isShowingErrorPage());
        String title = mActivityTestRule.getWebContents().getTitle();
        Assert.assertFalse(title.isEmpty());
        WebsiteParentApprovalTestUtils.checkLocalApprovalsButtonIsVisible(mWebContents);
        WebsiteParentApprovalTestUtils.checkRemoteApprovalsButtonIsVisible(mWebContents);
    }

    @Test
    @LargeTest
    public void incognitoModeIsUnavailableFromAppMenu() throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppMenuTestSupport.showAppMenu(
                            mActivityTestRule.getAppMenuCoordinator(), null, false);
                });
        int incognitoMenuItemStringId =
                mActivityTestRule.getActivity().getSupportedProfileType()
                                == SupportedProfileType.REGULAR
                        ? R.string.menu_new_incognito_window
                        : R.string.menu_new_incognito_tab;
        onView(withText(incognitoMenuItemStringId)).check(matches(not(isEnabled())));
        onView(withText(incognitoMenuItemStringId)).check(matches(not(isClickable())));
    }

    @Test
    @LargeTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.S_V2, message = "crbug.com/41485872")
    public void incognitoModeIsUnavailableFromTabSwitcherActionMenu() {
        onView(withId(R.id.tab_switcher_button)).perform(longClick());
        int incognitoMenuItemStringId =
                mActivityTestRule.getActivity().getSupportedProfileType()
                                == SupportedProfileType.REGULAR
                        ? R.string.menu_new_incognito_window
                        : R.string.menu_new_incognito_tab;
        onView(withText(incognitoMenuItemStringId)).check(matches(not(isEnabled())));
        onView(withText(incognitoMenuItemStringId)).check(matches(not(isClickable())));
    }

    @Test
    @LargeTest
    public void matureSitesAreBlockedBySafeSites() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SupervisedUserSettingsTestUtils.setKidsManagementResponseForTesting(
                            mActivityTestRule.getProfile(/* incognito= */ false),
                            /* isAllowed= */ false);
                });

        EmbeddedTestServer testServer = mActivityTestRule.getEmbeddedTestServerRule().getServer();
        // TODO(b/356932004): configure real infrastructure.
        String blockedHost = testServer.getURL(TEST_PAGE);
        mActivityTestRule.loadUrl(blockedHost);

        Tab tab = mActivityTestRule.getActivityTab();
        Assert.assertTrue(tab.isShowingErrorPage());
        String title = mActivityTestRule.getWebContents().getTitle();
        Assert.assertFalse(title.isEmpty());
        WebsiteParentApprovalTestUtils.checkLocalApprovalsButtonIsVisible(mWebContents);
        WebsiteParentApprovalTestUtils.checkRemoteApprovalsButtonIsVisible(mWebContents);
    }

    @Test
    @LargeTest
    public void regularSitesAreNotBlockedBySafeSites() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SupervisedUserSettingsTestUtils.setKidsManagementResponseForTesting(
                            mActivityTestRule.getProfile(/* incognito= */ false),
                            /* isAllowed= */ true);
                });

        EmbeddedTestServer testServer = mActivityTestRule.getEmbeddedTestServerRule().getServer();
        // TODO(b/356932004): configure real infrastructure.
        String notBlockedHost = testServer.getURL(TEST_PAGE);
        mActivityTestRule.loadUrl(notBlockedHost);

        Tab tab = mActivityTestRule.getActivityTab();
        Assert.assertFalse(tab.isShowingErrorPage());
        String title = mActivityTestRule.getWebContents().getTitle();
        Assert.assertFalse(title.isEmpty());
    }
}
