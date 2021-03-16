// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiDisableIf;

/**
 * Tests {@link OptionalNewTabButtonController}. See also {@link
 * OptionalNewTabButtonControllerTabletTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR + "<Study",
        "force-fieldtrials=Study/Group", "force-fieldtrial-params=Study.Group:mode/always-new-tab"})
@DisableIf.Device(type = {UiDisableIf.TABLET})
public class OptionalNewTabButtonControllerTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, /*clearAllTabState=*/false);

    private String mTestPageUrl;
    private String mButtonDescription;

    @Before
    public void setUp() {
        mTestPageUrl = sActivityTestRule.getTestServer().getURL(TEST_PAGE);
        mButtonDescription =
                sActivityTestRule.getActivity().getResources().getString(R.string.button_new_tab);
    }

    @Test
    @MediumTest
    public void testClick_opensNewTab() {
        sActivityTestRule.loadUrl(mTestPageUrl, /*secondsToWait=*/10);

        onViewWaiting(allOf(withId(R.id.optional_toolbar_button), isDisplayed(), isEnabled(),
                              withContentDescription(mButtonDescription)))
                .perform(click());

        // Expected tabs:
        // 1: mTestPageUrl
        // 2: opened by the click
        assertEquals(Integer.valueOf(2),
                TestThreadUtils.<Integer>runOnUiThreadBlockingNoException(() -> {
                    return sActivityTestRule.getActivity()
                            .getCurrentTabModel()
                            .getComprehensiveModel()
                            .getCount();
                }));
    }

    @Test
    @MediumTest
    public void testClick_opensNewTabInIncognito() {
        sActivityTestRule.loadUrlInNewTab(mTestPageUrl, /*incognito=*/true);

        onViewWaiting(allOf(withId(R.id.optional_toolbar_button), isDisplayed(), isEnabled(),
                              withContentDescription(mButtonDescription)))
                .perform(click());

        // Expected tabs:
        // 1: mTestPageUrl
        // 2: opened by the click
        assertEquals(Integer.valueOf(2),
                TestThreadUtils.<Integer>runOnUiThreadBlockingNoException(() -> {
                    return sActivityTestRule.getActivity()
                            .getCurrentTabModel()
                            .getComprehensiveModel()
                            .getCount();
                }));
        assertTrue(TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return sActivityTestRule.getActivity()
                    .getTabModelSelectorSupplier()
                    .get()
                    .isIncognitoSelected();
        }));
    }

    @Test
    @MediumTest
    public void testClick_recordsUserAction() {
        sActivityTestRule.loadUrl(mTestPageUrl, /*secondsToWait=*/10);
        UserActionTester userActionTester = new UserActionTester();

        onViewWaiting(allOf(withId(R.id.optional_toolbar_button), isDisplayed(), isEnabled(),
                              withContentDescription(mButtonDescription)))
                .perform(click());

        assertThat(/*reason=*/userActionTester.toString(), userActionTester.getActions(),
                Matchers.hasItem("MobileTopToolbarOptionalButtonNewTab"));
    }

    @Test
    @MediumTest
    public void testButton_hidesOnNTP() {
        sActivityTestRule.loadUrl(mTestPageUrl, /*secondsToWait=*/10);
        onViewWaiting(allOf(withId(R.id.optional_toolbar_button), isDisplayed(), isEnabled(),
                withContentDescription(mButtonDescription)));

        sActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        onView(isRoot()).check(waitForView(
                withId(R.id.optional_toolbar_button), ViewUtils.VIEW_GONE | ViewUtils.VIEW_NULL));
    }
}
