// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;

/** Tests for {@link HubBottomToolbarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubBottomToolbarCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PaneManager mPaneManager;
    @Mock private HubColorMixer mHubColorMixer;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private Tab mRegularTab;
    @Mock private Tab mIncognitoTab;
    @Captor private ArgumentCaptor<EdgeToEdgePadAdjuster> mPadAdjusterCaptor;

    private final SettableMonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNonNullObservableSupplier<Boolean> mIsHidingSupplier =
            ObservableSuppliers.createNonNull(false);

    private ActivityController<TestActivity> mActivityController;
    private Activity mActivity;
    private FrameLayout mContainer;

    @Before
    public void setUp() {
        when(mPaneManager.getFocusedPaneSupplier()).thenReturn(ObservableSuppliers.alwaysNull());
        when(mRegularTab.isIncognito()).thenReturn(false);
        when(mIncognitoTab.isIncognito()).thenReturn(true);
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        mActivity = mActivityController.get();
        mContainer = new FrameLayout(mActivity);
        mActivity.setContentView(mContainer);
    }

    @After
    public void tearDown() {
        mActivityController.close();
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
                        mEdgeToEdgeSupplier,
                        mCurrentTabSupplier,
                        mIsHidingSupplier);
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
                        mEdgeToEdgeSupplier,
                        mCurrentTabSupplier,
                        mIsHidingSupplier);

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
                        mEdgeToEdgeSupplier,
                        mCurrentTabSupplier,
                        mIsHidingSupplier);

        assertTrue(mEdgeToEdgeSupplier.hasObservers());

        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());
        assertNotNull(mPadAdjusterCaptor.getValue());

        coordinator.destroy();
    }

    @Test
    public void testAttachBottomBarView() {
        HubBottomToolbarDelegate emptyDelegate = spy(new EmptyHubBottomToolbarDelegate());
        HubBottomToolbarCoordinator coordinator =
                new HubBottomToolbarCoordinator(
                        mActivity,
                        mContainer,
                        mPaneManager,
                        mHubColorMixer,
                        emptyDelegate,
                        mEdgeToEdgeSupplier,
                        mCurrentTabSupplier,
                        mIsHidingSupplier);

        View childView = new View(mActivity);
        coordinator.attachBottomBarView(childView);

        verify(emptyDelegate).attachBottomBarView(childView);

        coordinator.destroy();
    }

    @Test
    @SmallTest
    public void testCurrentTabSupplier_WhenNotHiding_DoesNotUpdateColorScheme() {
        HubBottomToolbarDelegate emptyDelegate = spy(new EmptyHubBottomToolbarDelegate());
        SettableNullableObservableSupplier<Tab> currentTabSupplier =
                ObservableSuppliers.createNullable();
        SettableNonNullObservableSupplier<Boolean> isHidingSupplier =
                ObservableSuppliers.createNonNull(false);

        currentTabSupplier.set(mRegularTab);

        HubBottomToolbarCoordinator coordinator =
                new HubBottomToolbarCoordinator(
                        mActivity,
                        mContainer,
                        mPaneManager,
                        mHubColorMixer,
                        emptyDelegate,
                        mEdgeToEdgeSupplier,
                        currentTabSupplier,
                        isHidingSupplier);

        View addedView = mContainer.getChildAt(0);
        HubBottomToolbarView bottomToolbarView = addedView.findViewById(R.id.hub_bottom_toolbar);
        assertNotNull(bottomToolbarView);

        int incognitoColor =
                HubColors.getHubBottomToolbarColor(mActivity, HubColorScheme.INCOGNITO);

        // While inside Hub (isHiding == false), mediator does not set color scheme on view
        // (color mixing is handled by HubColorMixer).
        assertNull(bottomToolbarView.getBackground());

        // When not hiding, currentTab changes (e.g. closing last incognito tab) do NOT change color
        currentTabSupplier.set(mIncognitoTab);
        assertNull(bottomToolbarView.getBackground());

        // When Hub starts hiding, color scheme updates to destination tab
        isHidingSupplier.set(true);
        assertEquals(
                incognitoColor, ((ColorDrawable) bottomToolbarView.getBackground()).getColor());

        coordinator.destroy();
    }

    @Test
    @SmallTest
    public void testCurrentTabSupplier_WhenHiding_UpdatesColorScheme() {
        HubBottomToolbarDelegate emptyDelegate = spy(new EmptyHubBottomToolbarDelegate());
        SettableNullableObservableSupplier<Tab> currentTabSupplier =
                ObservableSuppliers.createNullable();
        SettableNonNullObservableSupplier<Boolean> isHidingSupplier =
                ObservableSuppliers.createNonNull(true);

        currentTabSupplier.set(mRegularTab);

        HubBottomToolbarCoordinator coordinator =
                new HubBottomToolbarCoordinator(
                        mActivity,
                        mContainer,
                        mPaneManager,
                        mHubColorMixer,
                        emptyDelegate,
                        mEdgeToEdgeSupplier,
                        currentTabSupplier,
                        isHidingSupplier);

        View addedView = mContainer.getChildAt(0);
        HubBottomToolbarView bottomToolbarView = addedView.findViewById(R.id.hub_bottom_toolbar);
        assertNotNull(bottomToolbarView);

        int defaultColor = HubColors.getHubBottomToolbarColor(mActivity, HubColorScheme.DEFAULT);
        int incognitoColor =
                HubColors.getHubBottomToolbarColor(mActivity, HubColorScheme.INCOGNITO);

        assertEquals(defaultColor, ((ColorDrawable) bottomToolbarView.getBackground()).getColor());

        // Switch to incognito tab (e.g. creating an incognito tab) while hiding
        currentTabSupplier.set(mIncognitoTab);
        assertEquals(
                incognitoColor, ((ColorDrawable) bottomToolbarView.getBackground()).getColor());

        // Switch back to regular tab
        currentTabSupplier.set(mRegularTab);
        assertEquals(defaultColor, ((ColorDrawable) bottomToolbarView.getBackground()).getColor());

        coordinator.destroy();
    }

    @Test
    @SmallTest
    public void testHubExit_ColorMixerRemainsSetUntilDestroy() {
        HubBottomToolbarDelegate emptyDelegate = spy(new EmptyHubBottomToolbarDelegate());
        SettableNullableObservableSupplier<Tab> currentTabSupplier =
                ObservableSuppliers.createNullable();
        SettableNonNullObservableSupplier<Boolean> isHidingSupplier =
                ObservableSuppliers.createNonNull(false);

        currentTabSupplier.set(mIncognitoTab);

        HubBottomToolbarCoordinator coordinator =
                new HubBottomToolbarCoordinator(
                        mActivity,
                        mContainer,
                        mPaneManager,
                        mHubColorMixer,
                        emptyDelegate,
                        mEdgeToEdgeSupplier,
                        currentTabSupplier,
                        isHidingSupplier);

        View addedView = mContainer.getChildAt(0);
        HubBottomToolbarView bottomToolbarView = addedView.findViewById(R.id.hub_bottom_toolbar);
        assertNotNull(bottomToolbarView);

        int incognitoColor =
                HubColors.getHubBottomToolbarColor(mActivity, HubColorScheme.INCOGNITO);

        // Initially in Hub not hiding -> mediator does not set background on view
        assertNull(bottomToolbarView.getBackground());

        // When Hub starts hiding, color scheme updates to destination tab (INCOGNITO)
        isHidingSupplier.set(true);
        assertEquals(
                incognitoColor, ((ColorDrawable) bottomToolbarView.getBackground()).getColor());

        // COLOR_MIXER remains registered during hide transition
        verify(mHubColorMixer, never()).unregisterBlend(any());

        // On destroy, COLOR_MIXER is cleared and unregisterBlend is called
        coordinator.destroy();
        verify(mHubColorMixer).unregisterBlend(any());
    }
}
