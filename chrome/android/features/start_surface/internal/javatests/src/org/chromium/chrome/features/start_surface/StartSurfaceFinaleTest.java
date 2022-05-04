// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.pressKey;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_TEST_BASE_PARAMS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.sClassParamsForStartSurfaceTest;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.ntp.NewTabPageUtils;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Integration tests of Finale variation of the {@link StartSurface}.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class StartSurfaceFinaleTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = sClassParamsForStartSurfaceTest;
    private static final long MAX_TIMEOUT_MS = 40000L;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    /**
     * Whether feature {@link ChromeFeatureList#INSTANT_START} is enabled.
     */
    private final boolean mUseInstantStart;

    /**
     * Whether feature {@link ChromeFeatureList#TAB_SWITCHER_ON_RETURN} is enabled as "immediately".
     * When immediate return is enabled, the Start surface is showing when Chrome is launched.
     */
    private final boolean mImmediateReturn;

    private CallbackHelper mLayoutChangedCallbackHelper;
    private LayoutStateProvider.LayoutStateObserver mLayoutObserver;
    @LayoutType
    private int mCurrentlyActiveLayout;
    public StartSurfaceFinaleTest(boolean useInstantStart, boolean immediateReturn) {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.INSTANT_START, useInstantStart);

        mUseInstantStart = useInstantStart;
        mImmediateReturn = immediateReturn;
    }

    @Before
    public void setUp() throws IOException {
        StartSurfaceTestUtils.setUpStartSurfaceTests(mImmediateReturn, mActivityTestRule);

        mLayoutChangedCallbackHelper = new CallbackHelper();

        if (isInstantReturn()) {
            // Assume start surface is shown immediately, and the LayoutStateObserver may miss the
            // first onFinishedShowing event.
            mCurrentlyActiveLayout = LayoutType.TAB_SWITCHER;
        }

        mLayoutObserver = new LayoutStateProvider.LayoutStateObserver() {
            @Override
            public void onFinishedShowing(@LayoutType int layoutType) {
                mCurrentlyActiveLayout = layoutType;
                mLayoutChangedCallbackHelper.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getLayoutManagerSupplier().addObserver((manager) -> {
                if (manager.getActiveLayout() != null) {
                    mCurrentlyActiveLayout = manager.getActiveLayout().getLayoutType();
                    mLayoutChangedCallbackHelper.notifyCalled();
                }
                manager.addObserver(mLayoutObserver);
            });
        });
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/omnibox_focused_on_new_tab/true"})
    public void testOmnibox_FocusedOnNewTabInSingleSurface() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Launches a new Tab from the Start surface, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        waitForView(withId(R.id.search_box_text));
        TextView urlBar = cta.findViewById(R.id.url_bar);
        CriteriaHelper.pollUiThread(
                ()
                        -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        assertEquals(cta.findViewById(R.id.toolbar_buttons).getVisibility(), View.INVISIBLE);
        ToolbarDataProvider toolbarDataProvider =
                cta.getToolbarManager().getLocationBarModelForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });

        // Navigates the new created Tab.
        TestThreadUtils.runOnUiThreadBlocking(() -> urlBar.setText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));

        // Launches a new Tab from the newly navigated tab, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 3, 0);
        waitForView(withId(R.id.search_box_text));
        CriteriaHelper.pollUiThread(
                ()
                        -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/show_last_active_tab_only/true" +
        "/exclude_mv_tiles/true/omnibox_focused_on_new_tab/true"})
    public void testOmnibox_FocusedOnNewTabInSingleSurfaceV2() {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Launches a new Tab from the Start surface, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        waitForView(withId(R.id.search_box_text));
        TextView urlBar = cta.findViewById(R.id.url_bar);
        CriteriaHelper.pollUiThread(
                ()
                        -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        assertEquals(cta.findViewById(R.id.toolbar_buttons).getVisibility(), View.INVISIBLE);
        ToolbarDataProvider toolbarDataProvider =
                cta.getToolbarManager().getLocationBarModelForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });

        // Navigates the new created Tab.
        TestThreadUtils.runOnUiThreadBlocking(() -> urlBar.setText("about:blank"));
        onView(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));

        // Launches a new Tab from the newly navigated tab, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 3, 0);
        waitForView(withId(R.id.search_box_text));
        CriteriaHelper.pollUiThread(
                ()
                        -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/omnibox_focused_on_new_tab/true"})
    public void testOmnibox_TabOpenedFromOmniboxShouldNotGetFocused() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        onViewWaiting(allOf(withId(R.id.search_box_text), isDisplayed()))
                .perform(replaceText("about:blank"));
        onViewWaiting(withId(R.id.url_bar)).perform(pressKey(KeyEvent.KEYCODE_ENTER));
        waitForView(withId(R.id.primary_tasks_surface_view), ViewUtils.VIEW_INVISIBLE);

        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        waitForView(withId(R.id.search_box_text));
        waitForView(withId(R.id.toolbar_buttons));
        TextView urlBar = cta.findViewById(R.id.url_bar);
        Assert.assertFalse(urlBar.isFocused());
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/omnibox_focused_on_new_tab/true"})
    public void testOmnibox_FocusedOnNewTabInSingleSurface_BackButtonDeleteBlankTab() {
        backActionDeleteBlankTabForOmniboxFocusedOnNewTabSingleSurface(
                () -> StartSurfaceTestUtils.pressBack(mActivityTestRule));
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/omnibox_focused_on_new_tab/true"})
    public void testOmnibox_FocusedOnNewTabInSingleSurface_BackGestureDeleteBlankTab() {
        backActionDeleteBlankTabForOmniboxFocusedOnNewTabSingleSurface(
                () -> StartSurfaceTestUtils.gestureNavigateBack(mActivityTestRule));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/exclude_mv_tiles/false"
        + "/new_home_surface_from_home_button/hide_mv_tiles_and_tab_switcher"
        + "/tab_count_button_on_start_surface/true"})
    public void testNewSurfaceFromHomeButton(){
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (mImmediateReturn) {
            StartSurfaceTestUtils.waitForOverviewVisible(
                    mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);

            onViewWaiting(allOf(withId(R.id.mv_tiles_layout), isDisplayed()));
            onViewWaiting(withId(R.id.carousel_tab_switcher_container));
            onViewWaiting(withId(R.id.start_tab_switcher_button));

            // Launch a tab. The home button should show on the normal tab.
            StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        }

        // Go back to the home surface, MV tiles and carousel tab switcher should not show anymore.
        StartSurfaceTestUtils.pressHomePageButton(cta);

        // MV tiles and carousel tab switcher should not show anymore.
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        onViewWaiting(withId(R.id.start_tab_switcher_button));
        onView(withId(R.id.mv_tiles_container)).check(matches(withEffectiveVisibility(GONE)));
        onView(withId(R.id.carousel_tab_switcher_container))
                .check(matches(withEffectiveVisibility(GONE)));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/exclude_mv_tiles/false"
        + "/new_home_surface_from_home_button/hide_tab_switcher_only"
        + "/tab_count_button_on_start_surface/true"})
    public void testNewSurfaceHideTabOnlyFromHomeButton() {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (mImmediateReturn) {
            StartSurfaceTestUtils.waitForOverviewVisible(
                    mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);

            onViewWaiting(withId(R.id.mv_tiles_layout));
            onViewWaiting(withId(R.id.carousel_tab_switcher_container));
            onViewWaiting(withId(R.id.start_tab_switcher_button));

            // Launch a tab. The home button should show on the normal tab.
            StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
            onViewWaiting(withId(R.id.home_button)).check(matches(isDisplayed()));
        }

        // Go back to the home surface, MV tiles and carousel tab switcher should not show anymore.
        StartSurfaceTestUtils.pressHomePageButton(cta);

        // MV tiles should shown and carousel tab switcher should not show anymore.
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        onViewWaiting(withId(R.id.start_tab_switcher_button));
        onView(withId(R.id.mv_tiles_layout)).check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withId(R.id.carousel_tab_switcher_container))
                .check(matches(withEffectiveVisibility(GONE)));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/omnibox_focused_on_new_tab/true"})
    public void testFinale_webFeedLaunchOrigin_notFocusedOnOmnibox() throws ExecutionException {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        // Launches a new Tab, and verifies the omnibox is not focused.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> cta.getTabCreator(false).launchUrl(
                                NewTabPageUtils.encodeNtpUrl(NewTabPageLaunchOrigin.WEB_FEED),
                                TabLaunchType.FROM_CHROME_UI));
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        waitForView(withId(R.id.search_box_text));
        TextView urlBar = cta.findViewById(R.id.url_bar);
        CriteriaHelper.pollUiThread(
                ()
                        -> !StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && !urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * @return Whether both features {@link ChromeFeatureList#INSTANT_START} and
     * {@link ChromeFeatureList#TAB_SWITCHER_ON_RETURN} are enabled.
     */
    private boolean isInstantReturn() {
        return mUseInstantStart && mImmediateReturn;
    }

    private void backActionDeleteBlankTabForOmniboxFocusedOnNewTabSingleSurface(
            Runnable backAction) {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        assertThat(cta.getTabModelSelector().getCurrentModel().getCount(), equalTo(1));

        // Launches a new Tab from the Start surface, and verifies the omnibox is focused.
        TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false).launchNTP());
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
        waitForView(withId(R.id.search_box_text));
        TextView urlBar = cta.findViewById(R.id.url_bar);
        CriteriaHelper.pollUiThread(
                ()
                        -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        assertEquals(cta.findViewById(R.id.toolbar_buttons).getVisibility(), View.INVISIBLE);
        ToolbarDataProvider toolbarDataProvider =
                cta.getToolbarManager().getLocationBarModelForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });

        backAction.run();

        waitForView(withId(R.id.primary_tasks_surface_view));
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
    }
}
