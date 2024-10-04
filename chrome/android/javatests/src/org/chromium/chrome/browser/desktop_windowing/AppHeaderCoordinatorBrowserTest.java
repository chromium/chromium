// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_windowing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doAnswer;

import static org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderCoordinator.INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW;

import android.content.ComponentCallbacks;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Build;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout.LayoutParams;
import android.widget.ImageButton;

import androidx.annotation.RequiresApi;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.AreaMotionEventFilter;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.hub.HubLayout;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.top.ToolbarTablet;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderCoordinator;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.InsetsRectProvider;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Browser test for {@link AppHeaderCoordinator} */
@RequiresApi(Build.VERSION_CODES.R)
@Restriction(DeviceFormFactor.TABLET)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
public class AppHeaderCoordinatorBrowserTest {
    private static final int APP_HEADER_LEFT_PADDING = 10;
    private static final int APP_HEADER_RIGHT_PADDING = 20;
    private static final String TEXTFIELD_DOM_ID = "inputElement";
    private static final int KEYBOARD_TIMEOUT = 10000;

    private static final WindowInsetsCompat BOTTOM_NAV_BAR_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(
                            WindowInsetsCompat.Type.navigationBars(),
                            Insets.of(0, 0, 0, /* bottom= */ 100))
                    .build();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock InsetsRectProvider mInsetsRectProvider;

    private Rect mWidestUnoccludedRect = new Rect();
    private Rect mWindowRect = new Rect();
    private int mTestAppHeaderHeight;

    @Before
    public void setup() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        InsetObserver.setInitialRawWindowInsetsForTesting(BOTTOM_NAV_BAR_INSETS);
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
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_TRANSITION_IN_DESKTOP_WINDOW)
    public void testToggleTabStripVisibilityInDesktopWindow() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        triggerDesktopWindowingModeChange(activity, true);

        ComponentCallbacks tabStripCallback =
                activity.getToolbarManager().getTabStripTransitionCoordinator();
        Assert.assertNotNull("Tab strip transition callback is null.", tabStripCallback);

        // Set the strip width threshold and trigger a configuration change to force tab strip
        // visibility. This is a test only strategy, as we don't want to actually change the
        // configuration which might result in an activity restart.

        // A very large strip width threshold should hide the strip by adding the scrim.
        TabStripTransitionCoordinator.setFadeTransitionThresholdForTesting(10000);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabStripCallback.onConfigurationChanged(
                                activity.getResources().getConfiguration()));

        var stripLayoutHelperManager = activity.getLayoutManager().getStripLayoutHelperManager();
        var stripAreaMotionEventFilter =
                (AreaMotionEventFilter) stripLayoutHelperManager.getEventFilter();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab strip scrim should be visible.",
                            stripLayoutHelperManager.isStripScrimVisibleForTesting(),
                            Matchers.equalTo(true));
                    Criteria.checkThat(
                            "Motion event filter area should be empty on an invisible strip.",
                            stripAreaMotionEventFilter.getEventAreaForTesting().isEmpty(),
                            Matchers.equalTo(true));
                });

        // A very small strip width threshold value should show the strip by removing the scrim.
        TabStripTransitionCoordinator.setFadeTransitionThresholdForTesting(1);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabStripCallback.onConfigurationChanged(
                                activity.getResources().getConfiguration()));
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab strip scrim should not be visible.",
                            stripLayoutHelperManager.isStripScrimVisibleForTesting(),
                            Matchers.equalTo(false));
                    Criteria.checkThat(
                            "Motion event filter area should be non-empty on a visible strip.",
                            stripAreaMotionEventFilter.getEventAreaForTesting().isEmpty(),
                            Matchers.equalTo(false));
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
    @DisabledTest(message = "Flaky, crbug.com/339854841")
    public void testEnterTabSwitcherInDesktopWindow_HubLayout() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Enter desktop windowing mode.
        triggerDesktopWindowingModeChange(activity, true);
        // Enter the tab switcher.
        TabUiTestHelper.enterTabSwitcher(activity);

        var layoutManager = (LayoutManagerChromeTablet) activity.getLayoutManager();
        var hubLayout = ((HubLayout) layoutManager.getHubLayoutForTesting());
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
    @DisabledTest(message = "Flaky, crbug.com/339854841")
    public void testEnterDesktopWindowWithTabSwitcherActive_HubLayout() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Enter the tab switcher. Desktop windowing mode is not active initially.
        TabUiTestHelper.enterTabSwitcher(activity);

        var layoutManager = (LayoutManagerChromeTablet) activity.getLayoutManager();
        var hubLayout = ((HubLayout) layoutManager.getHubLayoutForTesting());
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
    @DisabledTest(message = "Flaky, crbug.com/340589545")
    public void testRecreateActivitiesInDesktopWindow() {
        // Assume that the current activity enters desktop windowing mode.
        ChromeTabbedActivity firstActivity = mActivityTestRule.getActivity();
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
        mActivityTestRule.recreateActivity();
        firstActivity = mActivityTestRule.getActivity();
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

    @Test
    @MediumTest
    public void testKeyboardInDesktopWindowingModePadsRootView() throws TimeoutException {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        triggerDesktopWindowingModeChange(activity, true);
        var insetObserver = activity.getWindowAndroid().getInsetObserver();

        // Navigate to a URL with an input field. Clicking on it should trigger the OSK.
        mActivityTestRule.loadUrl(
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/page_with_editable.html"));
        DOMUtils.clickNode(activity.getActivityTab().getWebContents(), TEXTFIELD_DOM_ID);
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean isKeyboardShowing =
                            mActivityTestRule
                                    .getKeyboardDelegate()
                                    .isKeyboardShowing(activity, activity.getTabsView());
                    Criteria.checkThat(isKeyboardShowing, Matchers.is(true));
                },
                KEYBOARD_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        // Verify that the root view is padded at the bottom to account for the OSK inset.
        var rootView = activity.getWindow().getDecorView().getRootView();
        CriteriaHelper.pollUiThread(
                () -> {
                    var keyboardInset = insetObserver.getSupplierForKeyboardInset().get();
                    Criteria.checkThat(rootView.getPaddingBottom(), Matchers.is(keyboardInset));
                });

        // Remove input field focus to hide the keyboard.
        JavaScriptUtils.executeJavaScript(
                activity.getActivityTab().getWebContents(),
                "document.querySelector('input').blur()");

        // Verify that the root view bottom padding uses the system bar bottom inset.
        CriteriaHelper.pollUiThread(
                () -> {
                    var systemBarBottomInset =
                            insetObserver
                                    .getLastRawWindowInsets()
                                    .getInsets(WindowInsetsCompat.Type.systemBars())
                                    .bottom;
                    Criteria.checkThat(
                            rootView.getPaddingBottom(), Matchers.is(systemBarBottomInset));
                });

        // Dispatch window insets to simulate no overlap of the app window with system bar windows.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    insetObserver.onApplyWindowInsets(
                            rootView,
                            new WindowInsetsCompat.Builder()
                                    .setInsets(
                                            WindowInsetsCompat.Type.systemBars(),
                                            Insets.of(0, 0, 0, 0))
                                    .build());
                });

        // Verify that the root view bottom padding is reset.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(rootView.getPaddingBottom(), Matchers.is(0)));
    }

    @Test
    @MediumTest
    public void testBottomSheet() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        // Switch to desktop windowing mode.
        triggerDesktopWindowingModeChange(activity, true);

        // Trigger a bottom sheet, verify that the sheet container's top margin is updated to
        // account for the app header height.
        var bottomSheetContent = new TestBottomSheetContent(activity, ContentPriority.HIGH, false);
        var bottomSheetController =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var controller =
                                    (ManagedBottomSheetController)
                                            BottomSheetControllerProvider.from(
                                                    activity.getWindowAndroid());
                            controller.requestShowContent(bottomSheetContent, false);
                            return controller;
                        });

        var sheetContainer = activity.findViewById(R.id.sheet_container);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Bottom sheet should be visible.",
                            bottomSheetController.getSheetState(),
                            Matchers.not(SheetState.HIDDEN));
                    Criteria.checkThat(
                            "Sheet container top margin should account for app header height.",
                            ((MarginLayoutParams) sheetContainer.getLayoutParams()).topMargin,
                            Matchers.is(mTestAppHeaderHeight));
                });

        // Switch out of desktop windowing mode, verify that the sheet container's top margin is
        // reset.
        triggerDesktopWindowingModeChange(activity, false);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Bottom sheet should be visible.",
                            bottomSheetController.getSheetState(),
                            Matchers.not(SheetState.HIDDEN));
                    Criteria.checkThat(
                            "Sheet container top margin should be reset.",
                            ((MarginLayoutParams) sheetContainer.getLayoutParams()).topMargin,
                            Matchers.is(0));
                });

        // Hide bottom sheet.
        ThreadUtils.runOnUiThreadBlocking(
                () -> bottomSheetController.hideContent(bottomSheetContent, false));
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
        ThreadUtils.runOnUiThreadBlocking(
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
                            ((ImageButton)
                                            activity.getToolbarManager()
                                                    .getTabSwitcherButtonCoordinatorForTesting()
                                                    .getContainerView())
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
        ThreadUtils.runOnUiThreadBlocking(
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
