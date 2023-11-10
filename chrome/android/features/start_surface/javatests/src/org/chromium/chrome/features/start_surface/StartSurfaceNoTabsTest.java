// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.sClassParamsForStartSurfaceTest;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.espresso.action.GeneralClickAction;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Tap;
import androidx.test.espresso.action.ViewActions;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.List;

/**
 * Integration tests of the {@link StartSurface} for cases where there are no tabs. See {@link
 * StartSurfaceTest} for test that have tabs.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Restriction({
    UiRestriction.RESTRICTION_TYPE_PHONE,
    Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE
})
@EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
@DisableFeatures({ChromeFeatureList.SHOW_NTP_AT_STARTUP_ANDROID})
@DoNotBatch(reason = "StartSurface*Test tests startup behaviours and thus can't be batched.")
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
public class StartSurfaceNoTabsTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = sClassParamsForStartSurfaceTest;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private final boolean mImmediateReturn;

    public StartSurfaceNoTabsTest(boolean useInstantStart, boolean immediateReturn) {
        ChromeFeatureList.sInstantStart.setForTesting(useInstantStart);

        mImmediateReturn = immediateReturn;
    }

    @Before
    public void setUp() throws IOException {
        if (mImmediateReturn) {
            START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
            assertEquals(0, START_SURFACE_RETURN_TIME_SECONDS.getValue());
            assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));
        } else {
            assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));
        }
        ReturnToChromeUtil.setSkipInitializationCheckForTesting(true);

        mActivityTestRule.startMainActivityFromLauncher();
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    public void testShow_SingleAsHomepage_NoTabs() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);

        onView(withId(R.id.primary_tasks_surface_view)).check(matches(isDisplayed()));
        onView(withId(R.id.search_box_text)).check(matches(isDisplayed()));
        onView(withId(R.id.mv_tiles_container)).check(matches(isDisplayed()));
        onView(withId(R.id.tab_switcher_title)).check(matches(withEffectiveVisibility(GONE)));
        onView(withId(R.id.tab_switcher_module_container))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(withId(R.id.single_tab_view)).check(matches(withEffectiveVisibility(GONE)));
        onView(withId(R.id.more_tabs)).check(matches(withEffectiveVisibility(GONE)));
        onView(withId(R.id.tasks_surface_body)).check(matches(isDisplayed()));
        onView(withId(R.id.start_tab_switcher_button)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.logo)).check(matches(isDisplayed()));

        StartSurfaceTestUtils.launchFirstMVTile(cta, 0);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        pressBack();
        onViewWaiting(withId(R.id.primary_tasks_surface_view));
    }

    private void pressBack() {
        // ChromeTabbedActivity expects the native libraries to be loaded when back is pressed.
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onBackPressed());
    }

    private static GeneralClickAction clickAndPressBackIfAccidentallyLongClicked() {
        // If the click is misinterpreted as a long press, do a pressBack() to dismiss a context
        // menu.
        return new GeneralClickAction(
                Tap.SINGLE, GeneralLocation.CENTER, Press.FINGER, ViewActions.pressBack());
    }
}
