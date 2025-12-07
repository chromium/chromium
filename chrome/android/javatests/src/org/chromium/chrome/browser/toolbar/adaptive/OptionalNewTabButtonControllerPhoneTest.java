// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.res.Configuration;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.ViewUtils;

/**
 * Tests {@link OptionalNewTabButtonController}. See also {@link
 * OptionalNewTabButtonControllerTabletTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Features.EnableFeatures(
        ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2 + ":mode/always-new-tab")
@Restriction({DeviceFormFactor.PHONE})
public class OptionalNewTabButtonControllerPhoneTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private String mTestPageUrl;
    private String mButtonDescription;
    private WebPageStation mPage;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(
                AdaptiveToolbarButtonVariant.NEW_TAB);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(null);
    }

    @Before
    public void setUp() {
        mTestPageUrl = mActivityTestRule.getTestServer().getURL(TEST_PAGE);
        mButtonDescription =
                mActivityTestRule.getActivity().getResources().getString(R.string.button_new_tab);
        mPage = mActivityTestRule.startOnBlankPage();
    }

    @After
    public void tearDown() {
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testClick_opensNewTab_portrait() {
        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_PORTRAIT);
        mActivityTestRule.loadUrl(mTestPageUrl, /* secondsToWait= */ 10);

        onViewWaiting(
                        allOf(
                                withId(R.id.optional_toolbar_button),
                                isDisplayed(),
                                isEnabled(),
                                withContentDescription(mButtonDescription)))
                .perform(click());

        // Expected tabs:
        // 1: mTestPageUrl
        // 2: opened by the click
        assertEquals(
                Integer.valueOf(2),
                ThreadUtils.<Integer>runOnUiThreadBlocking(
                        () ->
                                mActivityTestRule
                                        .getActivity()
                                        .getCurrentTabModel()
                                        .getComprehensiveModel()
                                        .getCount()));
    }

    @Test
    @MediumTest
    public void testClick_opensNewTab_landscape() {
        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_LANDSCAPE);
        mActivityTestRule.loadUrl(mTestPageUrl, /* secondsToWait= */ 10);

        // Check view exists and is set up correctly.
        onViewWaiting(withId(R.id.optional_toolbar_button))
                .check(
                        matches(
                                allOf(
                                        isDisplayed(),
                                        isEnabled(),
                                        withContentDescription(mButtonDescription))));
        // Clicking with espresso is flaky, perform click directly.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .findViewById(R.id.optional_toolbar_button)
                            .performClick();
                });

        // Expected tabs:
        // 1: mTestPageUrl
        // 2: opened by the click
        assertEquals(
                Integer.valueOf(2),
                ThreadUtils.<Integer>runOnUiThreadBlocking(
                        () -> {
                            return mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabModel()
                                    .getComprehensiveModel()
                                    .getCount();
                        }));
    }

    @Test
    @MediumTest
    public void testClick_opensNewTabInIncognito() {
        mActivityTestRule.loadUrlInNewTab(mTestPageUrl, /* incognito= */ true);

        onViewWaiting(
                        allOf(
                                withId(R.id.optional_toolbar_button),
                                isDisplayed(),
                                isEnabled(),
                                withContentDescription(mButtonDescription)))
                .perform(click());

        // Expected tabs:
        // 1: mTestPageUrl
        // 2: opened by the click
        assertEquals(
                Integer.valueOf(2),
                ThreadUtils.<Integer>runOnUiThreadBlocking(
                        () ->
                                mActivityTestRule
                                        .getActivity()
                                        .getCurrentTabModel()
                                        .getComprehensiveModel()
                                        .getCount()));
        assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mActivityTestRule
                                        .getActivity()
                                        .getTabModelSelectorSupplier()
                                        .get()
                                        .isIncognitoSelected()));
    }

    @Test
    @MediumTest
    public void testClick_recordsUserAction() {
        mActivityTestRule.loadUrl(mTestPageUrl, /* secondsToWait= */ 10);
        UserActionTester userActionTester = new UserActionTester();

        onViewWaiting(
                        allOf(
                                withId(R.id.optional_toolbar_button),
                                isDisplayed(),
                                isEnabled(),
                                withContentDescription(mButtonDescription)))
                .perform(click());

        assertThat(
                /* message= */ userActionTester.toString(),
                userActionTester.getActions(),
                Matchers.hasItem("MobileTopToolbarOptionalButtonNewTab"));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1450561")
    public void testButton_hidesOnNtp() {
        mActivityTestRule.loadUrl(mTestPageUrl, /* secondsToWait= */ 10);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getActivityTab().reload());
        onViewWaiting(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        isEnabled(),
                        withContentDescription(mButtonDescription)));

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        ViewUtils.waitForViewCheckingState(
                withId(R.id.optional_toolbar_button), ViewUtils.VIEW_GONE | ViewUtils.VIEW_NULL);
    }
}
