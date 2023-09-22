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

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Verifies the main user journeys for supervised users.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SupervisedUserCriticalJourneysIntegrationTest {
    private static final String BLOCKED_SITE_URL = "www.example.com";
    private static final String MATURE_SITE_URL = "www.bestgore.com";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    public final SigninTestRule mSigninTestRule = new SigninTestRule();
    private WebContents mWebContents;

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(null);
        mSigninTestRule.addChildTestAccountThenWaitForSignin();
        mWebContents = mActivityTestRule.getWebContents();
    }

    @Test
    @LargeTest
    public void sitesThatAreOnBlocklistAreBlockedByInterstitialPage() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SupervisedUserSettingsTestUtils.addUrlToBlocklist(
                    Profile.getLastUsedRegularProfile(), BLOCKED_SITE_URL);
        });

        EmbeddedTestServer testServer = mActivityTestRule.getEmbeddedTestServerRule().getServer();
        String blockedHost = testServer.getURLWithHostName(BLOCKED_SITE_URL, "/");
        mActivityTestRule.loadUrl(blockedHost);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        Assert.assertTrue(tab.isShowingErrorPage());
        String title = mActivityTestRule.getActivity().getCurrentWebContents().getTitle();
        Assert.assertFalse(title.isEmpty());
        WebsiteParentApprovalTestUtils.checkLocalApprovalsButtonIsVisible(mWebContents);
        WebsiteParentApprovalTestUtils.checkRemoteApprovalsButtonIsVisible(mWebContents);
    }

    @Test
    @LargeTest
    public void incognitoModeIsUnavailableFromAppMenu() throws InterruptedException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });
        onView(withText(R.string.menu_new_incognito_tab)).check(matches(not(isEnabled())));
        onView(withText(R.string.menu_new_incognito_tab)).check(matches(not(isClickable())));
    }

    @Test
    @LargeTest
    public void incognitoModeIsUnavailableFromTabSwitcherActionMenu() {
        onView(withId(R.id.tab_switcher_button)).perform(longClick());
        onView(withText(R.string.menu_new_incognito_tab)).check(matches(not(isEnabled())));
        onView(withText(R.string.menu_new_incognito_tab)).check(matches(not(isClickable())));
    }

    @Test
    @LargeTest
    @Features.EnableFeatures({ChromeFeatureList.ENABLE_PROTO_API_FOR_CLASSIFY_URL})
    public void matureSitesAreBlockedBySafeSites() throws Exception {
        SupervisedUserSettingsTestUtils.setUpTestUrlLoaderFactoryHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SupervisedUserSettingsTestUtils.setSafeSearchResponseForTesting(
                    Profile.getLastUsedRegularProfile(), /*isAllowed=*/false);
        });

        EmbeddedTestServer testServer = mActivityTestRule.getEmbeddedTestServerRule().getServer();
        String blockedHost = testServer.getURLWithHostName(MATURE_SITE_URL, "/");
        mActivityTestRule.loadUrl(blockedHost);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        Assert.assertTrue(tab.isShowingErrorPage());
        String title = mActivityTestRule.getActivity().getCurrentWebContents().getTitle();
        Assert.assertFalse(title.isEmpty());
        WebsiteParentApprovalTestUtils.checkLocalApprovalsButtonIsVisible(mWebContents);
        WebsiteParentApprovalTestUtils.checkRemoteApprovalsButtonIsVisible(mWebContents);
        SupervisedUserSettingsTestUtils.tearDownTestUrlLoaderFactoryHelper();
    }
}
