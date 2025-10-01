// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.hub;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import com.google.common.collect.ImmutableSet;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
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

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Spy
    private final ObservableSupplierImpl<Boolean> mIsAnimatingSupplier =
            new ObservableSupplierImpl<>();

    private final ObservableSupplierImpl<Pane> mFocusedPaneSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Tab> mCurrentTabSupplier = new ObservableSupplierImpl<>();
    private HubToolbarCoordinator mCoordinator;
    private HubToolbarView mHubToolbarView;
    private MenuButton mMenuButton;
    private ObservableSupplierImpl<Boolean> mBottomToolbarVisibilitySupplier;

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
        mBottomToolbarVisibilitySupplier = spy(new ObservableSupplierImpl<>());
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        // Determine layout based on the parameter
        int layoutId = mIsXrDevice ? R.layout.hub_xr_layout : R.layout.hub_layout;
        View rootView = LayoutInflater.from(activity).inflate(layoutId, null);
        activity.setContentView(rootView);
        mHubToolbarView = spy(rootView.findViewById(R.id.hub_toolbar));
        mMenuButton = spy(mHubToolbarView.findViewById(R.id.menu_button_wrapper));
        when(mHubToolbarView.findViewById(R.id.menu_button_wrapper)).thenReturn(mMenuButton);
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
                        mCurrentTabSupplier,
                        mExitHubRunnable);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    public void isIphTriggered() {
        verify(mIsAnimatingSupplier).addSyncObserver(any());
        mIsAnimatingSupplier.set(false);
        ShadowLooper.runUiThreadTasks();

        verify(mUserEducationHelper).requestShowIph(any());
        verify(mIsAnimatingSupplier).removeObserver(any());
    }

    @Test
    public void testBottomToolbarVisibilitySupplier() {
        // Verify that observer was added to the bottom toolbar visibility supplier
        verify(mBottomToolbarVisibilitySupplier).addObserver(any());

        // Destroy coordinator
        mCoordinator.destroy();

        // Verify that observer was removed from the bottom toolbar visibility supplier
        verify(mBottomToolbarVisibilitySupplier).removeObserver(any());
    }
}
