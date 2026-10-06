// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;

import com.google.android.material.tabs.TabLayout;
import com.google.common.collect.ImmutableSet;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;

import java.util.Arrays;
import java.util.Collection;

/** Unit tests for {@link HubToolbarCoordinator}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class HubToolbarCoordinatorUnitTest {
    // All the tests in this file will run twice, once for isXrDevice=true and once for
    // isXrDevice=false. Expect all the tests with the same results on XR devices too.
    // The setup ensures the correct environment is configured for each run.
    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    @Parameter(0)
    public boolean mIsXrDevice;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    private final SettableNonNullObservableSupplier<Boolean> mIsAnimatingSupplier =
            ObservableSuppliers.createNonNull(false);

    private final SettableMonotonicObservableSupplier<Pane> mFocusedPaneSupplier =
            ObservableSuppliers.createMonotonic();
    private ActivityController<TestActivity> mActivityController;
    private HubToolbarCoordinator mCoordinator;
    private HubToolbarView mHubToolbarView;
    private final SettableNonNullObservableSupplier<Boolean> mBottomToolbarVisibilitySupplier =
            ObservableSuppliers.createNonNull(false);

    @Mock private PaneManager mPaneManager;
    @Mock private PaneOrderController mPaneOrderController;
    @Mock private MenuButtonCoordinator mMenuButtonCoordinator;
    @Mock private Tracker mTracker;
    @Mock private SearchActivityClient mSearchActivityClient;
    @Mock private HubColorMixer mHubColorMixer;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Runnable mExitHubRunnable;

    @Before
    public void setUp() {
        DeviceInfo.setIsXrForTesting(mIsXrDevice);

        when(mPaneManager.getFocusedPaneSupplier()).thenReturn(mFocusedPaneSupplier);
        when(mPaneManager.getPaneOrderController()).thenReturn(mPaneOrderController);
        when(mPaneOrderController.getPaneOrder()).thenReturn(ImmutableSet.of());
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        onActivity(mActivityController.get());
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    private void onActivity(Activity activity) {
        // Determine layout based on the parameter
        int layoutId = mIsXrDevice ? R.layout.hub_xr_layout : R.layout.hub_layout;
        View rootView = LayoutInflater.from(activity).inflate(layoutId, null);
        activity.setContentView(rootView);
        mHubToolbarView = rootView.findViewById(R.id.hub_toolbar);
        mCoordinator =
                new HubToolbarCoordinator(
                        activity,
                        mHubToolbarView,
                        mPaneManager,
                        mMenuButtonCoordinator,
                        mTracker,
                        mSearchActivityClient,
                        mHubColorMixer,
                        mUserEducationHelper,
                        mIsAnimatingSupplier,
                        mBottomToolbarVisibilitySupplier,
                        mExitHubRunnable);
    }

    @Test
    public void isIphTriggered() {
        assertTrue(mIsAnimatingSupplier.hasObservers());
        mIsAnimatingSupplier.set(true);
        mIsAnimatingSupplier.set(false);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mUserEducationHelper).requestShowIph(any());
        assertFalse(mIsAnimatingSupplier.hasObservers());
    }

    @Test
    public void testBottomToolbarVisibilitySupplier() {
        // Verify that observer was added to the bottom toolbar visibility supplier
        assertTrue(mBottomToolbarVisibilitySupplier.hasObservers());

        // Destroy coordinator
        mCoordinator.destroy();

        // Verify that observer was removed from the bottom toolbar visibility supplier
        assertFalse(mBottomToolbarVisibilitySupplier.hasObservers());
    }

    @Test
    public void testSetPaneSwitcherScrollPosition() {
        TabLayout paneSwitcher = mHubToolbarView.findViewById(R.id.pane_switcher);
        paneSwitcher.addTab(paneSwitcher.newTab().setText("A"));
        paneSwitcher.addTab(paneSwitcher.newTab().setText("B"));
        paneSwitcher.setVisibility(View.VISIBLE);
        paneSwitcher.measure(
                MeasureSpec.makeMeasureSpec(400, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY));
        paneSwitcher.layout(0, 0, 400, 100);
        int firstTabLeft = paneSwitcher.getTabAt(0).view.getLeft();
        int secondTabLeft = paneSwitcher.getTabAt(1).view.getLeft();

        // The indicator should be halfway between the first and second tab.
        mCoordinator.setPaneSwitcherScrollPosition(0, 0.5f);
        Rect indicatorBounds = paneSwitcher.getTabSelectedIndicator().getBounds();
        assertTrue(indicatorBounds.left > firstTabLeft);
        assertTrue(indicatorBounds.left < secondTabLeft);
    }

    @Test
    public void testSetBlockTabSelectionCallback() {
        TabLayout paneSwitcher = mHubToolbarView.findViewById(R.id.pane_switcher);
        mCoordinator.setBlockTabSelectionCallback(true);
        assertNotNull(shadowOf(paneSwitcher).getOnTouchListener());

        mCoordinator.setBlockTabSelectionCallback(false);
        assertNull(shadowOf(paneSwitcher).getOnTouchListener());
    }
}
