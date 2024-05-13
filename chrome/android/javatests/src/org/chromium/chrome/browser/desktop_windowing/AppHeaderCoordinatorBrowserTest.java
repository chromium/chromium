// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_windowing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doAnswer;

import static org.chromium.chrome.browser.desktop_windowing.AppHeaderCoordinator.INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW;

import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Build;
import android.widget.FrameLayout.LayoutParams;
import android.widget.ImageButton;

import androidx.annotation.RequiresApi;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.hub.HubLayout;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherLayout;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.top.ToolbarTablet;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderState;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.components.browser_ui.widget.InsetsRectProvider;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/** Browser test for {@link AppHeaderCoordinator} */
@RequiresApi(Build.VERSION_CODES.R)
@Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
public class AppHeaderCoordinatorBrowserTest {
    private static final int APP_HEADER_LEFT_PADDING = 10;
    private static final int APP_HEADER_RIGHT_PADDING = 20;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock InsetsRectProvider mInsetsRectProvider;
    private @Mock InsetObserver mInsetObserver;
    private @Mock WindowInsetsCompat mWindowInsets;

    private Rect mWidestUnoccludedRect = new Rect();
    private Rect mWindowRect = new Rect();
    private int mTestAppHeaderHeight;

    @Before
    public void setup() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        InsetObserver.setInitialRawWindowInsetsForTesting(mWindowInsets);
        AppHeaderCoordinator.setInsetsRectProviderForTesting(mInsetsRectProvider);

        doAnswer(args -> mWidestUnoccludedRect).when(mInsetsRectProvider).getWidestUnoccludedRect();
        doAnswer(args -> mWindowRect).when(mInsetsRectProvider).getWindowRect();

        mActivityTestRule.startMainActivityOnBlankPage();

        // Initialize the strip height for testing. This is due to bots might have different
        // densities.
        Resources res = mActivityTestRule.getActivity().getResources();
        int tabStripHeight = res.getDimensionPixelSize(R.dimen.tab_strip_height);
        int reservedStripTopPadding =
                res.getDimensionPixelSize(R.dimen.tab_strip_reserved_top_padding);
        mTestAppHeaderHeight = tabStripHeight + reservedStripTopPadding;
    }

    @Test
    @MediumTest
    public void testTabStripHeightChangeForTabStripLayoutOptimization() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        triggerDesktopWindowingModeChange(activity, true);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(activity.getToolbarManager(), Matchers.notNullValue());
                    Criteria.checkThat(
                            "Tab strip height is different",
                            activity.getToolbarManager().getTabStripHeightSupplier().get(),
                            Matchers.equalTo(mTestAppHeaderHeight));

                    StripLayoutHelperManager stripLayoutHelperManager =
                            activity.getLayoutManager().getStripLayoutHelperManager();
                    Criteria.checkThat(stripLayoutHelperManager, Matchers.notNullValue());
                    float density = activity.getResources().getDisplayMetrics().density;
                    Criteria.checkThat(
                            "Tab strip does not resized.",
                            stripLayoutHelperManager.getHeight() * density,
                            Matchers.equalTo((float) mTestAppHeaderHeight));
                });
    }

    @Test
    @MediumTest
    public void testOnTopResumedActivityChanged_UnfocusedInDesktopWindow() {
        // TODO (crbug/330213938): Also test other scenarios for different values of desktop
        // windowing mode / activity focus states; tests for other input combinations are currently
        // failing even locally due to incorrect tab switcher icon tint.
        doTestOnTopResumedActivityChanged(
                /* isInDesktopWindow= */ true, /* isActivityFocused= */ false);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.ANDROID_HUB)
    public void testEnterTabSwitcherInDesktopWindow() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Enter desktop windowing mode.
        triggerDesktopWindowingModeChange(activity, true);
        // Enter the tab switcher.
        TabUiTestHelper.enterTabSwitcher(activity);

        var layoutManager = (LayoutManagerChromeTablet) activity.getLayoutManager();
        var tabSwitcherLayout =
                ((TabSwitcherLayout) layoutManager.getTabSwitcherLayoutForTesting());
        var tabSwitcherContainerView =
                tabSwitcherLayout
                        .getTabSwitcherForTesting()
                        .getController()
                        .getTabSwitcherContainer();

        assertTrue(
                "Tab switcher container view y-offset should be non-zero.",
                tabSwitcherContainerView.getY() != 0);
        assertEquals(
                "Tab switcher container view y-offset should match the app header height.",
                mTestAppHeaderHeight,
                tabSwitcherContainerView.getY(),
                0f);

        // Exit desktop windowing mode.
        triggerDesktopWindowingModeChange(activity, false);
        assertEquals(
                "Tab switcher container view y-offset should be zero.",
                0,
                tabSwitcherContainerView.getY(),
                0f);
        // Exit tab switcher.
        TabUiTestHelper.clickFirstCardFromTabSwitcher(activity);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.ANDROID_HUB)
    public void testEnterDesktopWindowWithTabSwitcherActive() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Enter the tab switcher. Desktop windowing mode is not active initially.
        TabUiTestHelper.enterTabSwitcher(activity);

        var layoutManager = (LayoutManagerChromeTablet) activity.getLayoutManager();
        var tabSwitcherLayout =
                ((TabSwitcherLayout) layoutManager.getTabSwitcherLayoutForTesting());
        var tabSwitcherContainerView =
                tabSwitcherLayout
                        .getTabSwitcherForTesting()
                        .getController()
                        .getTabSwitcherContainer();
        assertEquals(
                "Tab switcher container view y-offset should be zero.",
                0,
                tabSwitcherContainerView.getY(),
                0.0);

        // Enter desktop windowing mode while the tab switcher is visible.
        triggerDesktopWindowingModeChange(activity, true);

        assertTrue(
                "Tab switcher container view y-offset should be non-zero.",
                tabSwitcherContainerView.getY() != 0);
        assertEquals(
                "Tab switcher container view y-offset should match the app header height.",
                mTestAppHeaderHeight,
                tabSwitcherContainerView.getY(),
                0f);

        // Exit desktop windowing mode.
        triggerDesktopWindowingModeChange(activity, false);
        assertEquals(
                "Tab switcher container view y-offset should be zero.",
                0,
                tabSwitcherContainerView.getY(),
                0f);
        // Exit tab switcher.
        TabUiTestHelper.clickFirstCardFromTabSwitcher(activity);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_HUB)
    public void testEnterTabSwitcherInDesktopWindow_HubLayout() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Enter desktop windowing mode.
        triggerDesktopWindowingModeChange(activity, true);
        // Enter the tab switcher.
        TabUiTestHelper.enterTabSwitcher(activity);

        var layoutManager = (LayoutManagerChromeTablet) activity.getLayoutManager();
        var hubLayout = ((HubLayout) layoutManager.getTabSwitcherLayoutForTesting());
        var hubContainerView = hubLayout.getHubControllerForTesting().getContainerView();
        var params = (LayoutParams) hubContainerView.getLayoutParams();

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab switcher container view y-offset should match the app header"
                                    + " height.",
                            (int) hubContainerView.getY(),
                            Matchers.is(mTestAppHeaderHeight));
                    Criteria.checkThat(
                            "Tab switcher container view top margin should match the app header"
                                    + " height.",
                            params.topMargin,
                            Matchers.is(mTestAppHeaderHeight));
                });

        // Exit desktop windowing mode.
        triggerDesktopWindowingModeChange(activity, false);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab switcher container view y-offset should be zero.",
                            hubContainerView.getY(),
                            Matchers.is(0f));
                });
        TabUiTestHelper.clickFirstCardFromTabSwitcher(activity);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_HUB)
    public void testEnterDesktopWindowWithTabSwitcherActive_HubLayout() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Enter the tab switcher. Desktop windowing mode is not active initially.
        TabUiTestHelper.enterTabSwitcher(activity);

        var layoutManager = (LayoutManagerChromeTablet) activity.getLayoutManager();
        var hubLayout = ((HubLayout) layoutManager.getTabSwitcherLayoutForTesting());
        var hubContainerView = hubLayout.getHubControllerForTesting().getContainerView();
        var params = (LayoutParams) hubContainerView.getLayoutParams();

        assertEquals(
                "Tab switcher container view y-offset should be zero.",
                0,
                hubContainerView.getY(),
                0.0);
        assertEquals("Tab switcher container view top margin should be zero.", 0, params.topMargin);

        // Enter desktop windowing mode while the tab switcher is visible.
        triggerDesktopWindowingModeChange(activity, true);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab switcher container view y-offset should match the app header"
                                    + " height.",
                            (int) hubContainerView.getY(),
                            Matchers.is(mTestAppHeaderHeight));
                    Criteria.checkThat(
                            "Tab switcher container view top margin should match the app header"
                                    + " height.",
                            params.topMargin,
                            Matchers.is(mTestAppHeaderHeight));
                });

        // Exit desktop windowing mode.
        triggerDesktopWindowingModeChange(activity, false);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab switcher container view y-offset should be zero.",
                            hubContainerView.getY(),
                            Matchers.is(0f));
                });
        TabUiTestHelper.clickFirstCardFromTabSwitcher(activity);
    }

    @Test
    @MediumTest
    public void testRecreateActivitiesInDesktopWindow() {
        // Assume that the current activity enters desktop windowing mode.
        ChromeTabbedActivity firstActivity = mActivityTestRule.getActivity();
        AppHeaderUtils.setAppInDesktopWindowForTesting(true);
        firstActivity = ApplicationTestUtils.recreateActivity(firstActivity);
        triggerDesktopWindowingModeChange(firstActivity, true);

        // Create a new (desktop) window, that should gain focus and cause the first activity to
        // lose focus.
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        firstActivity.getApplicationContext(),
                        MultiWindowUtils.INVALID_INSTANCE_ID,
                        true,
                        false,
                        true);
        ChromeTabbedActivity secondActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () -> ContextUtils.getApplicationContext().startActivity(intent));
        triggerDesktopWindowingModeChange(secondActivity, true);

        // Trigger activity recreation in desktop windowing mode (an app theme change for eg. would
        // trigger this).
        firstActivity = ApplicationTestUtils.recreateActivity(firstActivity);
        secondActivity = ApplicationTestUtils.recreateActivity(secondActivity);

        // Activity recreation will send an #onTopResumedActivityChanged(false) signal as the
        // activity is stopping, so both activities will be considered unfocused.
        assertTrue(
                "Saved instance state bundle should hold correct desktop window focus state.",
                firstActivity
                        .getSavedInstanceState()
                        .getBoolean(INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW));
        assertTrue(
                "Saved instance state bundle should hold correct desktop window focus state.",
                secondActivity
                        .getSavedInstanceState()
                        .getBoolean(INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW));

        // As |secondActivity| regains focus after recreation, it will receive an
        // #onTopResumedActivityChanged(true) signal, that should re-apply the correct top Chrome
        // colors. |firstActivity| should start with the unfocused window colors, based on the saved
        // instance state value.
        verifyToolbarIconTints(
                firstActivity, /* isActivityFocused= */ false, /* isInDesktopWindow= */ true);
        verifyToolbarIconTints(
                secondActivity, /* isActivityFocused= */ true, /* isInDesktopWindow= */ true);

        // TODO(aishwaryarj): Verify tab strip background color too. This is currently failing on
        // the CQ bot.

        // Exit desktop windowing mode and finish the second activity.
        AppHeaderUtils.setAppInDesktopWindowForTesting(false);
        secondActivity.finish();
    }

    private void doTestOnTopResumedActivityChanged(
            boolean isInDesktopWindow, boolean isActivityFocused) {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        CriteriaHelper.pollUiThread(
                () -> {
                    var appHeaderCoordinator =
                            activity.getRootUiCoordinatorForTesting()
                                    .getDesktopWindowStateProvider();
                    Criteria.checkThat(appHeaderCoordinator, Matchers.notNullValue());
                });

        // Assume that the current activity lost focus in desktop windowing mode.
        triggerDesktopWindowingModeChange(activity, true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.onTopResumedActivityChanged(isActivityFocused));

        // Verify the toolbar icon tints.
        verifyToolbarIconTints(activity, isActivityFocused, isInDesktopWindow);
    }

    private void verifyToolbarIconTints(
            ChromeTabbedActivity activity, boolean isActivityFocused, boolean isInDesktopWindow) {
        var omniboxIconTint =
                ThemeUtils.getThemedToolbarIconTint(activity, BrandedColorScheme.APP_DEFAULT)
                        .getDefaultColor();
        var nonOmniboxIconTint =
                isInDesktopWindow
                        ? ThemeUtils.getThemedToolbarIconTintForActivityState(
                                        activity, BrandedColorScheme.APP_DEFAULT, isActivityFocused)
                                .getDefaultColor()
                        : omniboxIconTint;

        CriteriaHelper.pollUiThread(
                () -> {
                    var toolbarTablet =
                            (ToolbarTablet)
                                    activity.getToolbarManager().getToolbarLayoutForTesting();
                    Criteria.checkThat(
                            "Home button tint is incorrect",
                            toolbarTablet.getHomeButton().getImageTintList().getDefaultColor(),
                            Matchers.is(nonOmniboxIconTint));
                    Criteria.checkThat(
                            "Tab switcher icon tint is incorrect.",
                            toolbarTablet
                                    .getTabSwitcherButton()
                                    .getImageTintList()
                                    .getDefaultColor(),
                            Matchers.is(nonOmniboxIconTint));
                    Criteria.checkThat(
                            "App menu button tint is incorrect.",
                            ((ImageButton) activity.getToolbarManager().getMenuButtonView())
                                    .getImageTintList()
                                    .getDefaultColor(),
                            Matchers.is(nonOmniboxIconTint));
                    Criteria.checkThat(
                            "Bookmark button tint is incorrect.",
                            toolbarTablet
                                    .getBookmarkButtonForTesting()
                                    .getImageTintList()
                                    .getDefaultColor(),
                            Matchers.is(omniboxIconTint));
                });
    }

    private void triggerDesktopWindowingModeChange(
            ChromeTabbedActivity activity, boolean isInDesktopWindow) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var appHeaderStateProvider =
                            activity.getRootUiCoordinatorForTesting()
                                    .getDesktopWindowStateProvider();
                    setupAppHeaderRects(isInDesktopWindow);
                    var appHeaderState =
                            new AppHeaderState(
                                    mWindowRect, mWidestUnoccludedRect, isInDesktopWindow);
                    ((AppHeaderCoordinator) appHeaderStateProvider)
                            .setStateForTesting(isInDesktopWindow, appHeaderState);
                    AppHeaderUtils.setAppInDesktopWindowForTesting(isInDesktopWindow);
                });
    }

    private void setupAppHeaderRects(boolean isInDesktopWindow) {
        // Configure mock InsetsRectProvider.
        var activity = mActivityTestRule.getActivity();
        activity.getWindow().getDecorView().getGlobalVisibleRect(mWindowRect);
        if (isInDesktopWindow) {
            mWidestUnoccludedRect.set(
                    APP_HEADER_LEFT_PADDING,
                    0,
                    mWindowRect.right - APP_HEADER_RIGHT_PADDING,
                    mTestAppHeaderHeight);
        } else {
            mWidestUnoccludedRect.setEmpty();
        }
    }
}
