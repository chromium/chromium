// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator.BottomControlsVisibilityController;
import org.chromium.chrome.browser.ui.bottombar.BottomBar;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link BottomBarContainerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomBarContainerCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Callback<Boolean> mRequestLayerUpdateCallback;
    @Mock private BottomControlsVisibilityController mVisibilityController;
    @Mock private Callback<Object> mOnModelTokenChange;
    @Mock private Tab mTab;
    @Mock private ThemeColorProvider mThemeColorProvider;

    private final SettableNullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();

    private Activity mActivity;
    private FrameLayout mBottomBarContainer;
    private BottomBarContainerCoordinator mCoordinator;

    @Before
    public void setUp() {
        mTabSupplier.set(null);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = activity;
                            mBottomBarContainer = new FrameLayout(mActivity);
                            mCoordinator =
                                    new BottomBarContainerCoordinator(
                                            mBottomBarContainer,
                                            mRequestLayerUpdateCallback,
                                            mTabSupplier,
                                            mThemeColorProvider);
                        });
    }

    @Test
    public void testInitialization() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);
        verify(mVisibilityController).setBottomControlsVisible(true);
        verify(mOnModelTokenChange).onResult(any());
    }

    @Test
    public void testGetScrollBehavior() {
        assertEquals(LayerScrollBehavior.DEFAULT_SCROLL_OFF, mCoordinator.getScrollBehavior());
    }

    @Test
    public void testGetBackgroundColor() {
        assertNull(mCoordinator.getBackgroundColor());
    }

    @Test
    public void testGetBottomBar() {
        BottomBar bottomBar = mCoordinator.getBottomBar();
        assertNotNull(bottomBar);

        View view = bottomBar.getView();
        assertNotNull(view);

        bottomBar.setParent(Host.HUB); // Should not crash
    }

    @Test
    public void testAttachBottomBarView_notInitialized() {
        View childView = new View(mActivity);
        mCoordinator.attachBottomBarView(childView);
        assertEquals(1, mBottomBarContainer.getChildCount());
        assertEquals(childView, mBottomBarContainer.getChildAt(0));
        verify(mRequestLayerUpdateCallback).onResult(true);
        verify(mOnModelTokenChange, never()).onResult(any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true"})
    public void testDisableOnNtp() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mTab.isIncognito()).thenReturn(false);
        mTabSupplier.set(mTab);

        verify(mVisibilityController).setBottomControlsVisible(false);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true"})
    public void testDisableOnNtp_Incognito() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mTab.isIncognito()).thenReturn(true);
        mTabSupplier.set(mTab);

        verify(mVisibilityController, times(2)).setBottomControlsVisible(true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true"})
    public void testDisableOnNtp_NotNtp() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(false);
        mTabSupplier.set(mTab);

        verify(mVisibilityController, times(2)).setBottomControlsVisible(true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/false"})
    public void testDisableOnNtp_FlagDisabled() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mTab.isIncognito()).thenReturn(false);
        mTabSupplier.set(mTab);

        verify(mVisibilityController, times(2)).setBottomControlsVisible(true);
    }

    @Test
    public void testAttachBottomBarView_initialized() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);

        View childView = new View(mActivity);
        mCoordinator.attachBottomBarView(childView);
        assertEquals(1, mBottomBarContainer.getChildCount());
        assertEquals(childView, mBottomBarContainer.getChildAt(0));
        verify(mRequestLayerUpdateCallback).onResult(true);
        verify(mOnModelTokenChange, times(2)).onResult(any());
    }
}
