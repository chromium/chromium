// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;

/** Tests for {@link HubBottomToolbarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubBottomToolbarCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private PaneManager mPaneManager;
    @Mock private HubColorMixer mHubColorMixer;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Captor private ArgumentCaptor<EdgeToEdgePadAdjuster> mPadAdjusterCaptor;

    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();

    private Activity mActivity;
    private FrameLayout mContainer;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        mContainer = new FrameLayout(activity);
        activity.setContentView(mContainer);
    }

    @Test
    @SmallTest
    public void testDestroy() {
        HubBottomToolbarDelegate emptyDelegate = spy(new EmptyHubBottomToolbarDelegate());
        HubBottomToolbarCoordinator coordinator =
                new HubBottomToolbarCoordinator(
                        mActivity,
                        mContainer,
                        mPaneManager,
                        mHubColorMixer,
                        emptyDelegate,
                        mEdgeToEdgeSupplier);
        coordinator.destroy();
        verify(emptyDelegate).destroy();
    }

    @Test
    @SmallTest
    public void testInitializeBottomToolbarView() {
        HubBottomToolbarDelegate emptyDelegate = spy(new EmptyHubBottomToolbarDelegate());
        HubBottomToolbarCoordinator coordinator =
                new HubBottomToolbarCoordinator(
                        mActivity,
                        mContainer,
                        mPaneManager,
                        mHubColorMixer,
                        emptyDelegate,
                        mEdgeToEdgeSupplier);

        // Verify initializeBottomToolbarView was called
        verify(emptyDelegate)
                .initializeBottomToolbarView(mActivity, mContainer, mPaneManager, mHubColorMixer);

        // EmptyDelegate should have created and added a view to the container
        assertTrue(mContainer.getChildCount() > 0);

        // The view should be a HubBottomToolbarView
        View addedView = mContainer.getChildAt(0);
        HubBottomToolbarView bottomToolbarView = addedView.findViewById(R.id.hub_bottom_toolbar);
        assertNotNull(bottomToolbarView);

        coordinator.destroy();
    }

    @Test
    public void testPadAdjuster() {
        HubBottomToolbarDelegate emptyDelegate = spy(new EmptyHubBottomToolbarDelegate());
        HubBottomToolbarCoordinator coordinator =
                new HubBottomToolbarCoordinator(
                        mActivity,
                        mContainer,
                        mPaneManager,
                        mHubColorMixer,
                        emptyDelegate,
                        mEdgeToEdgeSupplier);

        assertTrue(mEdgeToEdgeSupplier.hasObservers());

        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());
        assertNotNull(mPadAdjusterCaptor.getValue());

        coordinator.destroy();
    }
}
