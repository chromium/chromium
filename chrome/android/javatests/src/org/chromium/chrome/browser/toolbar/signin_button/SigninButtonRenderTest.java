// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterBefore;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;

/** Render tests for {@link SigninButtonCoordinator}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@DoNotBatch(reason = "This test relies on native initialization")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(SigninFeatures.SIGNIN_LEVEL_UP_BUTTON)
public class SigninButtonRenderTest {

    @Rule(order = 1)
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    // Mock sign-in environment needs to be destroyed after ChromeTabbedActivity in case there are
    // observers registered in the AccountManagerFacade mock.
    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    private FakeSyncServiceImpl mFakeSyncServiceImpl;

    private RegularNewTabPageStation mPage;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnNtp();
        NewTabPageTestUtils.waitForNtpLoaded(mPage.getTab());
    }

    @After
    public void tearDown() {
        if (mFakeSyncServiceImpl != null) {
            mFakeSyncServiceImpl = null;
            SyncServiceFactory.setInstanceForTesting(null);
        }
    }

    // TODO(https://crbug.com/478828569): Add further render tests for signed out and signed in with
    // no error states.
    @Test
    @MediumTest
    @Feature("RenderTest")
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSigninButtonWithErrorBadge(boolean nightModeEnabled) throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mFakeSyncServiceImpl = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(mFakeSyncServiceImpl);
                });

        // SigninButton may have already been initialized with a real SyncService. As such,
        // recreating the activity in order to ensure the fake SyncService override is used.
        mActivityTestRule.recreateActivity();

        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        // Test transition to error.
        mFakeSyncServiceImpl.setRequiresClientUpgrade(true);

        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(R.id.signin_button),
                "signin_button_identity_error_exist");
    }
}
