// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.LoadUrlParams;

/** Verifies incognito tab related journeys for various types of supervised profiles. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoInteractionTest {
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    private RegularNewTabPageStation mNtp;

    /** Waits until the Incognito Tab is closed. */
    private static class TabClosedWaiter extends EmptyTabObserver {
        private final CallbackHelper mCallbackHelper;

        public TabClosedWaiter() {
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onDestroyed(Tab tab) {
            mCallbackHelper.notifyCalled();
        }

        public void waitForClose() throws Exception {
            mCallbackHelper.waitForOnly();
        }
    }

    @Before
    public void setUp() {
        mNtp = mActivityTestRule.startFromLauncherAtNtp();
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.PROPAGATE_DEVICE_CONTENT_FILTERS_TO_SUPERVISED_USER})
    // This policy explicitly allows incognito mode, and has higher priority over local parental
    // controls.
    @Policies.Add({@Policies.Item(key = "IncognitoModeAvailability", string = "0")})
    public void incognitoTabsNotClosedWhenPolicyAllowsIncognito() throws Exception {
        Profile profile = mActivityTestRule.getProfile(/* incognito= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SupervisedUserServiceTestBridge.init(profile);
                });

        // Create a new incognito tab. This succeeds, as the device is not
        // supervised.
        IncognitoNewTabPageStation incognitoNtp = mNtp.openNewIncognitoTabOrWindowFast();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = incognitoNtp.getActivity().getActivityTab();
                    // Enable browser content filtering for the current profile.
                    SupervisedUserServiceTestBridge.enableBrowserContentFilters(profile);
                    // Successfully load a URL in the incognito tab. That proves that the tab is not
                    // closed. If the tab was closed, the load would fail due to assertion error.
                    assertNotNull(tab.getWebContents());
                    Tab.LoadUrlResult result = tab.loadUrl(new LoadUrlParams("about:blank"));
                    assertTrue(result.tabLoadStatus == Tab.TabLoadStatus.DEFAULT_PAGE_LOAD);
                });
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.PROPAGATE_DEVICE_CONTENT_FILTERS_TO_SUPERVISED_USER})
    public void incognitoTabsClosedWhenBrowserContentFilteringIsEnabledWithoutAccount()
            throws Exception {
        Profile profile = mActivityTestRule.getProfile(/* incognito= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SupervisedUserServiceTestBridge.init(profile);
                });

        // Create a new incognito tab. This succeeds, as the device is not
        // supervised.
        IncognitoNewTabPageStation incognitoNtp = mNtp.openNewIncognitoTabOrWindowFast();
        TabClosedWaiter tabClosedWaiter = new TabClosedWaiter();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = incognitoNtp.getActivity().getActivityTab();
                    tab.addObserver(tabClosedWaiter);
                    // Enable browser content filtering for the current profile.
                    SupervisedUserServiceTestBridge.enableBrowserContentFilters(profile);
                    assertNull(tab.getWebContents());
                });

        // Check that the incognito tab is no longer open.
        tabClosedWaiter.waitForClose();
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.PROPAGATE_DEVICE_CONTENT_FILTERS_TO_SUPERVISED_USER})
    public void incognitoTabsClosedWhenBrowserContentFilteringIsEnabledWithAccount()
            throws Exception {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        Profile profile = mActivityTestRule.getProfile(/* incognito= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SupervisedUserServiceTestBridge.init(profile);
                });

        // Create a new incognito tab. This succeeds, as the device is not
        // supervised (however, a regular account is signed in).
        IncognitoNewTabPageStation incognitoNtp = mNtp.openNewIncognitoTabOrWindowFast();
        TabClosedWaiter tabClosedWaiter = new TabClosedWaiter();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = incognitoNtp.getActivity().getActivityTab();
                    tab.addObserver(tabClosedWaiter);
                    // Enable browser content filtering for the current profile.
                    SupervisedUserServiceTestBridge.enableBrowserContentFilters(profile);
                    assertNull(tab.getWebContents());
                });

        // Check that the incognito tab is no longer open.
        tabClosedWaiter.waitForClose();
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.PROPAGATE_DEVICE_CONTENT_FILTERS_TO_SUPERVISED_USER})
    public void incognitoTabsClosedWhenSearchContentFilteringIsEnabledWithAccount()
            throws Exception {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        Profile profile = mActivityTestRule.getProfile(/* incognito= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SupervisedUserServiceTestBridge.init(profile);
                });

        // Create a new incognito tab. This succeeds, as the device is not
        // supervised (however, a regular account is signed in).
        IncognitoNewTabPageStation incognitoNtp = mNtp.openNewIncognitoTabOrWindowFast();
        TabClosedWaiter tabClosedWaiter = new TabClosedWaiter();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = incognitoNtp.getActivity().getActivityTab();
                    tab.addObserver(tabClosedWaiter);
                    // Enable browser content filtering for the current profile.
                    SupervisedUserServiceTestBridge.enableSearchContentFilters(profile);
                    assertNull(tab.getWebContents());
                });

        // Check that the incognito tab is no longer open.
        tabClosedWaiter.waitForClose();
    }

    @Test
    @LargeTest
    public void incognitoTabsClosedWhenSupervisedUserSignsIn() throws Exception {
        // Create a new incognito tab. This succeeds, as the device is not
        // supervised.
        IncognitoNewTabPageStation incognitoNtp = mNtp.openNewIncognitoTabOrWindowFast();
        TabClosedWaiter tabClosedWaiter = new TabClosedWaiter();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = incognitoNtp.getActivity().getActivityTab();
                    tab.addObserver(tabClosedWaiter);
                });
        // Add a supervised user to the device.
        mSigninTestRule.addChildTestAccountThenWaitForSignin();

        // Check that the incognito tab is no longer open.
        tabClosedWaiter.waitForClose();
    }
}
