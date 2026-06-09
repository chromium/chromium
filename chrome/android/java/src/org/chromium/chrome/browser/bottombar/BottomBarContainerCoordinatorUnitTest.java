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
import android.content.res.Configuration;
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
import org.chromium.base.test.RobolectricUtil;
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
import org.chromium.ui.modaldialog.ModalDialogManager;
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
    @Mock private ModalDialogManager mModalDialogManager;

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
    private SettableNonNullObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
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
                            mModalDialogManagerSupplier =
                                    ObservableSuppliers.createNonNull(mModalDialogManager);
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
                                            mOmniboxFocusStateSupplier,
                                            mModalDialogManagerSupplier);
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

    @Test
    public void testOnConfigurationChanged() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);
        verify(mOnModelTokenChange, times(1)).onResult(any());

        Configuration newConfig = new Configuration();
        newConfig.orientation = Configuration.ORIENTATION_LANDSCAPE;

        mCoordinator.getComponentCallbacksForTesting().onConfigurationChanged(newConfig);

        // Runnable is posted, verify it hasn't run yet.
        verify(mOnModelTokenChange, times(1)).onResult(any());

        // Run posted tasks.
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify runnable executed.
        verify(mOnModelTokenChange, times(2)).onResult(any());
    }

    @Test
    public void testOnConfigurationChanged_debounce() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);
        verify(mOnModelTokenChange, times(1)).onResult(any());

        Configuration newConfig1 = new Configuration();
        newConfig1.orientation = Configuration.ORIENTATION_LANDSCAPE;

        Configuration newConfig2 = new Configuration();
        newConfig2.orientation = Configuration.ORIENTATION_PORTRAIT;

        mCoordinator.getComponentCallbacksForTesting().onConfigurationChanged(newConfig1);
        mCoordinator.getComponentCallbacksForTesting().onConfigurationChanged(newConfig2);

        // Run posted tasks.
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify runnable executed only once more (total 2).
        verify(mOnModelTokenChange, times(2)).onResult(any());
    }

    @Test
    public void testOnConfigurationChanged_sameOrientation() {
        mCoordinator.initializeWithNative(mVisibilityController, mOnModelTokenChange);
        verify(mOnModelTokenChange, times(1)).onResult(any());

        Configuration newConfig = new Configuration();
        int currentOrientation = mActivity.getResources().getConfiguration().orientation;
        newConfig.orientation = currentOrientation;

        mCoordinator.getComponentCallbacksForTesting().onConfigurationChanged(newConfig);

        // Run posted tasks.
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify runnable NOT executed.
        verify(mOnModelTokenChange, times(1)).onResult(any());
    }

    @Test
    public void testOnBackgroundColorChanged() {
        mCoordinator.onBackgroundColorChanged();
        verify(mRequestLayerUpdateCallback).onResult(false);
    }
}
