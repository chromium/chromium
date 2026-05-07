// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
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
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator.BottomControlsVisibilityController;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.bottombar.BottomBar;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.chrome.browser.ui.bottombar.BottomBarUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

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
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private ActionRegistry mActionRegistry;
    @Mock private Profile mProfile;

    private final SettableNullableObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<PropertyModel> mActionSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<Profile> mProfileSupplier =
            ObservableSuppliers.createNullable();

    private Activity mActivity;
    private FrameLayout mBottomBarContainer;
    private SettableNonNullObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private SettableNonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private BottomBarContainerCoordinator mCoordinator;

    @Before
    public void setUp() {
        mTabSupplier.set(null);
        when(mActionRegistry.get(anyInt())).thenReturn(mActionSupplier);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = activity;
                            mBottomBarContainer = new FrameLayout(mActivity);
                            mHomepageEnabledSupplier = ObservableSuppliers.createNonNull(true);
                            mOmniboxFocusStateSupplier = ObservableSuppliers.createNonNull(false);
                            mProfileSupplier.set(mProfile);
                            mCoordinator =
                                    new BottomBarContainerCoordinator(
                                            mBottomBarContainer,
                                            mRequestLayerUpdateCallback,
                                            mActionRegistry,
                                            mTabSupplier,
                                            mThemeColorProvider,
                                            mHomepageEnabledSupplier,
                                            mProfileSupplier,
                                            mOmniboxFocusStateSupplier);
                        });
    }

    @Test
    public void testInitialization() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);
        verify(mVisibilityController).setBottomControlsVisible(true);
        verify(mOnModelTokenChange).onResult(any());
        verify(mActionRegistry, times(2)).get(ActionId.NEW_TAB);
    }

    @Test
    public void testGetScrollBehavior() {
        assertEquals(LayerScrollBehavior.DEFAULT_SCROLL_OFF, mCoordinator.getScrollBehavior());
    }

    @Test
    public void testGetBackgroundColor() {
        when(mThemeColorProvider.getBrandedColorScheme())
                .thenReturn(BrandedColorScheme.APP_DEFAULT);
        assertEquals(
                (Integer)
                        BottomBarUtils.getBottomBarBackgroundColor(
                                mActivity, BrandedColorScheme.APP_DEFAULT),
                mCoordinator.getBackgroundColor());
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
    public void testOnVisibilityChanged() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);

        mCoordinator.onVisibilityChanged(false);
        assertEquals(View.GONE, mBottomBarContainer.getVisibility());
        verify(mVisibilityController).setBottomControlsVisible(false);

        mCoordinator.onVisibilityChanged(true);
        assertEquals(View.VISIBLE, mBottomBarContainer.getVisibility());
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
