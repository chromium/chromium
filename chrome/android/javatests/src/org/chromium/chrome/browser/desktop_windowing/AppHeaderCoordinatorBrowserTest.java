// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_windowing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import android.graphics.Rect;
import android.os.Build;
import android.widget.ImageButton;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherLayout;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.top.ToolbarTablet;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.widget.InsetsRectProvider;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.test.util.UiRestriction;

/** Browser test for {@link AppHeaderCoordinator} */
@RequiresApi(Build.VERSION_CODES.R)
@Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
public class AppHeaderCoordinatorBrowserTest {

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock InsetsRectProvider mInsetsRectProvider;
    private @Captor ArgumentCaptor<InsetsRectProvider.Observer> mInsetsRectObserverCaptor;

    private Rect mWidestUnoccludedRect = new Rect();
    private Rect mWindowRect = new Rect();

    @Before
    public void setup() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        AppHeaderCoordinator.setInsetsRectProviderForTesting(mInsetsRectProvider);

        doAnswer(args -> mWidestUnoccludedRect).when(mInsetsRectProvider).getWidestUnoccludedRect();
        doAnswer(args -> mWindowRect).when(mInsetsRectProvider).getWindowRect();

        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.DYNAMIC_TOP_CHROME)
    public void testTabStripHeightChangeForTabStripLayoutOptimization() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Configure mock InsetsRectProvider.
        activity.getWindow().getDecorView().getGlobalVisibleRect(mWindowRect);

        int topPadding = 5;
        int leftPadding = 10;
        int rightPadding = 20;
        int tabStripHeight =
                activity.getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
        mWidestUnoccludedRect.set(leftPadding, 0, rightPadding, topPadding + tabStripHeight);

        // Invoke observer to trigger browser controls transition.
        verify(mInsetsRectProvider, atLeastOnce()).addObserver(mInsetsRectObserverCaptor.capture());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (var obs : mInsetsRectObserverCaptor.getAllValues()) {
                        obs.onBoundingRectsUpdated(mWidestUnoccludedRect);
                    }
                });

        int newTabStripHeight = tabStripHeight + topPadding;
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(activity.getToolbarManager(), Matchers.notNullValue());
                    Criteria.checkThat(
                            "Tab strip height is different",
                            activity.getToolbarManager().getTabStripHeightSupplier().get(),
                            Matchers.equalTo(newTabStripHeight));
                });
    }

    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
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
        triggerDesktopWindowingModeChange(true);
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
        float stripHeightPx =
                ViewUtils.dpToPx(
                        activity.getApplicationContext(),
                        activity.getLayoutManager().getStripLayoutHelperManager().getHeight());
        assertTrue(
                "Tab switcher container view y-offset should be non-zero.",
                tabSwitcherContainerView.getY() != 0);
        assertEquals(
                "Tab switcher container view y-offset should match the tab strip height.",
                stripHeightPx,
                tabSwitcherContainerView.getY(),
                0f);

        // Exit desktop windowing mode.
        triggerDesktopWindowingModeChange(false);
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
        triggerDesktopWindowingModeChange(true);

        float stripHeightPx =
                ViewUtils.dpToPx(
                        activity.getApplicationContext(),
                        activity.getLayoutManager().getStripLayoutHelperManager().getHeight());
        assertTrue(
                "Tab switcher container view y-offset should be non-zero.",
                tabSwitcherContainerView.getY() != 0);
        assertEquals(
                "Tab switcher container view y-offset should match the tab strip height.",
                stripHeightPx,
                tabSwitcherContainerView.getY(),
                0f);

        // Exit desktop windowing mode.
        triggerDesktopWindowingModeChange(false);
        assertEquals(
                "Tab switcher container view y-offset should be zero.",
                0,
                tabSwitcherContainerView.getY(),
                0f);
        // Exit tab switcher.
        TabUiTestHelper.clickFirstCardFromTabSwitcher(activity);
    }

    private void doTestOnTopResumedActivityChanged(
            boolean isInDesktopWindow, boolean isActivityFocused) {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        var omniboxIconTint =
                ThemeUtils.getThemedToolbarIconTint(activity, BrandedColorScheme.APP_DEFAULT);
        var nonOmniboxIconTint =
                isInDesktopWindow
                        ? ThemeUtils.getThemedToolbarIconTintForActivityState(
                                activity, BrandedColorScheme.APP_DEFAULT, isActivityFocused)
                        : omniboxIconTint;

        CriteriaHelper.pollUiThread(
                () -> {
                    var appHeaderCoordinator =
                            activity.getRootUiCoordinatorForTesting()
                                    .getAppHeaderCoordinatorSupplier()
                                    .get();
                    Criteria.checkThat(appHeaderCoordinator, Matchers.notNullValue());
                });

        // Assume that the current activity lost focus in desktop windowing mode.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var appHeaderCoordinator =
                            activity.getRootUiCoordinatorForTesting()
                                    .getAppHeaderCoordinatorSupplier()
                                    .get();
                    appHeaderCoordinator.set(isInDesktopWindow);
                    activity.onTopResumedActivityChanged(isActivityFocused);
                });

        // Verify the toolbar icon tints.
        CriteriaHelper.pollUiThread(
                () -> {
                    var toolbarTablet =
                            (ToolbarTablet)
                                    activity.getToolbarManager().getToolbarLayoutForTesting();
                    Criteria.checkThat(
                            "Home button tint is incorrect",
                            toolbarTablet.getHomeButton().getImageTintList(),
                            Matchers.is(nonOmniboxIconTint));
                    Criteria.checkThat(
                            "Tab switcher icon tint is incorrect.",
                            toolbarTablet.getTabSwitcherButton().getImageTintList(),
                            Matchers.is(nonOmniboxIconTint));
                    Criteria.checkThat(
                            "App menu button tint is incorrect.",
                            ((ImageButton) activity.getToolbarManager().getMenuButtonView())
                                    .getImageTintList(),
                            Matchers.is(nonOmniboxIconTint));
                    Criteria.checkThat(
                            "Bookmark button tint is incorrect.",
                            toolbarTablet.getBookmarkButtonForTesting().getImageTintList(),
                            Matchers.is(omniboxIconTint));
                });
    }

    private void triggerDesktopWindowingModeChange(boolean isInDesktopWindow) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var desktopWindowModeSupplier =
                            mActivityTestRule
                                    .getActivity()
                                    .getRootUiCoordinatorForTesting()
                                    .getAppHeaderCoordinatorSupplier()
                                    .get();
                    desktopWindowModeSupplier.set(isInDesktopWindow);
                });
    }
}
