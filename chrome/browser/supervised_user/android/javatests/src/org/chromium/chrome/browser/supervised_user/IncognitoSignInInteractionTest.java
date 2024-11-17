// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;

/** Verifies the main user journeys for supervised users. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class IncognitoSignInInteractionTest {
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    /** Waits until the Incognito Tab is closed. */
    private static class TabClosedWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;

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
        mActivityTestRule.startMainActivityWithURL(null);
    }

    @Test
    @LargeTest
    public void incognitoTabsClosedWhenSupervisedUserSignsIn() throws Exception {
        // Create a new incognito tab. This succeeds, as the device is not
        // supervised.
        Tab tab = mActivityTestRule.newIncognitoTabFromMenu();
        TabClosedWaiter tabClosedWaiter = new TabClosedWaiter();
        ThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(tabClosedWaiter));

        // Add a supervised user to the device.
        mSigninTestRule.addChildTestAccountThenWaitForSignin();

        // Check that the incognito tab is no longer open.
        tabClosedWaiter.waitForClose();
    }
}
