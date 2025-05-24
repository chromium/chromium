// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.support.annotation.ColorInt;
import android.view.ContextThemeWrapper;
import android.view.Window;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.util.ColorUtils;

/* Unit tests for StatusBarColorController behavior. */
@RunWith(BaseRobolectricTestRunner.class)
public class StatusBarColorControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private StatusBarColorProvider mStatusBarColorProvider;
    @Mock private ObservableSupplier<LayoutManager> mLayoutManagerSupplier;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;
    @Mock private EdgeToEdgeSystemBarColorHelper mSystemBarColorHelper;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;

    private final ObservableSupplierImpl<Integer> mOverviewColorSupplier =
            new ObservableSupplierImpl<>(Color.TRANSPARENT);
    private StatusBarColorController mStatusBarColorController;
    private Window mWindow;
    private Context mContext;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    private void onActivity(TestActivity activity) {
        mWindow = activity.getWindow();
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE})
    @Config(sdk = 30) // Min version needed for e2e everywhere
    public void testSetStatusBarColor_EdgeToEdgeEnabled() {
        StatusBarColorController.setStatusBarColor(mSystemBarColorHelper, mWindow, Color.BLUE);
        verify(mSystemBarColorHelper).setStatusBarColor(Color.BLUE);

        StatusBarColorController.setStatusBarColor(mSystemBarColorHelper, mWindow, Color.RED);
        verify(mSystemBarColorHelper).setStatusBarColor(Color.RED);
    }

    @Test
    public void testSetStatusBarColor_EdgeToEdgeDisabled() {
        StatusBarColorController.setStatusBarColor(mSystemBarColorHelper, mWindow, Color.BLUE);
        verify(mSystemBarColorHelper, times(0)).setStatusBarColor(anyInt());

        StatusBarColorController.setStatusBarColor(null, mWindow, Color.BLUE);
        verify(mSystemBarColorHelper, times(0)).setStatusBarColor(anyInt());
    }

    @Test
    public void testInitialStatusBarColorOnTablet_NotInDesktopWindow() {
        initialize(/* isTablet */ true, /* isInDesktopWindow= */ false);
        mStatusBarColorController.updateStatusBarColor();
        assertEquals(
                "Status bar color is incorrect.",
                TabUiThemeUtil.getTabStripBackgroundColor(mContext, /* isIncognito= */ false),
                mStatusBarColorController.getStatusBarColorWithoutStatusIndicator());
    }

    @Test
    public void testInitialStatusBarColorOnTablet_InFocusedDesktopWindow() {
        when(mDesktopWindowStateManager.isInUnfocusedDesktopWindow()).thenReturn(false);
        initialize(/* isTablet */ true, /* isInDesktopWindow= */ true);
        mStatusBarColorController.updateStatusBarColor();
        assertEquals(
                "Status bar color is incorrect.",
                TabUiThemeUtil.getTabStripBackgroundColor(mContext, /* isIncognito= */ false),
                mStatusBarColorController.getStatusBarColorWithoutStatusIndicator());
    }

    @Test
    public void testInitialStatusBarColorOnTablet_InUnfocusedDesktopWindow() {
        when(mDesktopWindowStateManager.isInUnfocusedDesktopWindow()).thenReturn(true);
        initialize(/* isTablet */ true, /* isInDesktopWindow= */ true);
        mStatusBarColorController.updateStatusBarColor();
        assertEquals(
                "Status bar color is incorrect.",
                TabUiThemeUtil.getTabStripBackgroundColor(
                        mContext,
                        /* isIncognito= */ false,
                        /* isInDesktopWindow= */ true,
                        /* isActivityFocused= */ false),
                mStatusBarColorController.getStatusBarColorWithoutStatusIndicator());
    }

    @Test
    public void testOverviewMode() {
        initialize(/* isTablet */ false, /* isInDesktopWindow= */ false);
        mOverviewColorSupplier.set(Color.RED);
        assertEquals(
                "Status bar color is incorrect.",
                Color.RED,
                mStatusBarColorController.calculateFinalStatusBarColor());
    }

    @Test
    public void testOverviewModeOverlay() {
        initialize(/* isTablet */ false, /* isInDesktopWindow= */ false);
        mStatusBarColorController.updateStatusBarColor();
        mStatusBarColorController.setScrimColor(Color.TRANSPARENT);

        @ColorInt int expectedColor = ColorUtils.setAlphaComponentWithFloat(Color.RED, 0.5f);
        mOverviewColorSupplier.set(expectedColor);
        expectedColor =
                ColorUtils.overlayColor(
                        mStatusBarColorController.getStatusBarColorWithoutStatusIndicator(),
                        expectedColor);
        assertEquals(
                "Status bar color is incorrect.",
                expectedColor,
                mStatusBarColorController.calculateFinalStatusBarColor());
    }

    @Test
    public void testOnTopResumedActivityChanged() {
        initialize(/* isTablet= */ true, /* isInDesktopWindow= */ true);
        int focusedStripColor =
                TabUiThemeUtil.getTabStripBackgroundColor(mContext, /* isIncognito= */ false);
        int unfocusedStripColor =
                TabUiThemeUtil.getTabStripBackgroundColor(
                        mContext,
                        /* isIncognito= */ false,
                        /* isInDesktopWindow= */ true,
                        /* isActivityFocused= */ false);

        // Assume that the tab strip is in a focused desktop window.
        mStatusBarColorController.onTopResumedActivityChanged(true);
        assertEquals(
                "Status bar color is incorrect.",
                focusedStripColor,
                mStatusBarColorController.getStatusBarColorWithoutStatusIndicator());

        // Assume that the tab strip is in an unfocused desktop window.
        mStatusBarColorController.onTopResumedActivityChanged(false);
        assertEquals(
                "Status bar color is incorrect.",
                unfocusedStripColor,
                mStatusBarColorController.getStatusBarColorWithoutStatusIndicator());
    }

    private void initialize(boolean isTablet, boolean isInDesktopWindow) {
        OneshotSupplierImpl<DesktopWindowStateManager> desktopWindowStateManagerSupplier =
                new OneshotSupplierImpl<>();
        desktopWindowStateManagerSupplier.set(mDesktopWindowStateManager);
        AppHeaderUtils.setAppInDesktopWindowForTesting(isInDesktopWindow);
        mStatusBarColorController =
                new StatusBarColorController(
                        mWindow,
                        isTablet,
                        mContext,
                        mStatusBarColorProvider,
                        mLayoutManagerSupplier,
                        mActivityLifecycleDispatcher,
                        mActivityTabProvider,
                        mTopUiThemeColorProvider,
                        mSystemBarColorHelper,
                        desktopWindowStateManagerSupplier,
                        mOverviewColorSupplier);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }
}
