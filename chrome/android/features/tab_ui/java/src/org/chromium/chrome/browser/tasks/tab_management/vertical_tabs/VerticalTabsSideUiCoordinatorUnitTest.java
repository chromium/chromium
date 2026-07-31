// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabsSideUiCoordinator.VIEW_WIDTH_DP;

import android.app.Activity;
import android.content.res.Configuration;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.transition.TransitionSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Px;
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
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListCoordinator.RailCollapseListener;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.ui.base.ViewUtils;

/** Unit tests for {@link VerticalTabsSideUiCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabsSideUiCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private VerticalTabListCoordinator mMockTabListCoordinator;
    @Mock private SideUiCoordinator mMockSideUiCoordinator;

    @Captor private ArgumentCaptor<RailCollapseListener> mCollapseListenerCaptor;

    private VerticalTabsSideUiCoordinator mCoordinator;
    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private @Px int mWideWindowWidth;
    private @Px int mNarrowWindowWidth;
    private @Px int mExpandedRailWidth;
    private @Px int mCollapsedRailWidth;
    private final SettableNonNullObservableSupplier<Boolean> mIsVerticalTabsActiveSupplier =
            ObservableSuppliers.createNonNull(false);

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(Activity.class).setup();
        mActivity = mActivityController.get();
        mWideWindowWidth = ViewUtils.dpToPx(mActivity, 800);
        mNarrowWindowWidth = ViewUtils.dpToPx(mActivity, 600);
        mExpandedRailWidth = ViewUtils.dpToPx(mActivity, VIEW_WIDTH_DP);
        mCollapsedRailWidth =
                ViewUtils.dpToPx(mActivity, VerticalTabsSideUiCoordinator.COLLAPSED_WIDTH_DP);
        // Initialize window width to wide before mCoordinator creation to avoid
        // triggering a layout change event during constructor setup.
        setWindowWidthPx(mWideWindowWidth);
        View mockView = new View(mActivity);
        when(mMockTabListCoordinator.getView()).thenReturn(mockView);

        mCoordinator =
                new VerticalTabsSideUiCoordinator(
                        mActivity,
                        mMockSideUiCoordinator,
                        mMockTabListCoordinator,
                        mIsVerticalTabsActiveSupplier);
    }

    @After
    public void tearDown() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
    }

    @Test
    @SmallTest
    public void testObserverRegistration() {
        // Constructor is called in setUp(), verify registration happened.
        verify(mMockSideUiCoordinator).addObserver(mCoordinator);
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mCoordinator.setVisible(/* show= */ true, /* suppressAnimations= */ false);
        mCoordinator.onUiUpdateCompleted(
                /* oldWidth= */ 0,
                /* newWidth= */ 100,
                HeightType.NOT_APPLICABLE,
                HeightType.TOOLBAR);
        assertTrue(mIsVerticalTabsActiveSupplier.get());

        mCoordinator.destroy();
        verify(mMockSideUiCoordinator).removeObserver(mCoordinator);
        verify(mMockTabListCoordinator).destroy();
        assertFalse(mIsVerticalTabsActiveSupplier.get());
    }

    @Test
    @SmallTest
    public void testGetView() {
        View view = mCoordinator.getView();
        assertNotNull(view);
        assertTrue(view instanceof FrameLayout);
    }

    @Test
    @SmallTest
    public void testGetAnchorSide() {
        assertEquals(AnchorSide.LEFT, mCoordinator.getAnchorSide());
    }

    @Test
    @SmallTest
    public void testDetermineShowableSize() {
        assertEquals(
                new SideUiSize(0, HeightType.NOT_APPLICABLE),
                mCoordinator.determineShowableSize(
                        /* availableWidth= */ mExpandedRailWidth - 1,
                        /* windowWidth= */ mWideWindowWidth,
                        /* isFullscreen= */ false));
        assertEquals(
                new SideUiSize(mExpandedRailWidth, HeightType.TOOLBAR),
                mCoordinator.determineShowableSize(
                        /* availableWidth= */ mExpandedRailWidth,
                        /* windowWidth= */ mWideWindowWidth,
                        /* isFullscreen= */ false));
    }

    @Test
    @SmallTest
    public void testDetermineShowableSize_FullscreenReturnsZeroWidth() {
        assertEquals(
                new SideUiSize(0, HeightType.NOT_APPLICABLE),
                mCoordinator.determineShowableSize(
                        /* availableWidth= */ mExpandedRailWidth,
                        /* windowWidth= */ mWideWindowWidth,
                        /* isFullscreen= */ true));
    }

    @Test
    @SmallTest
    public void testHasContentToShow() {
        mCoordinator.setVisible(/* show= */ true, /* suppressAnimations= */ false);
        assertTrue(mCoordinator.hasContentToShow());

        mCoordinator.setVisible(/* show= */ false, /* suppressAnimations= */ false);
        assertFalse(mCoordinator.hasContentToShow());
    }

    @Test
    @SmallTest
    public void testSetWidth() {
        mCoordinator.setWidth(150);
        ViewGroup.LayoutParams layoutParams = mCoordinator.getView().getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(150, layoutParams.width);
    }

    @Test
    @SmallTest
    public void testOnUiUpdateCompleted() {
        mCoordinator.setVisible(/* show= */ true, /* suppressAnimations= */ false);
        mCoordinator.onUiUpdateCompleted(
                /* oldWidth= */ 0,
                /* newWidth= */ 100,
                HeightType.NOT_APPLICABLE,
                HeightType.TOOLBAR);
        assertTrue(mIsVerticalTabsActiveSupplier.get());

        // When hiding with animation, supplier remains true until onUiUpdateCompleted.
        SideUiSpecs specs = new SideUiSpecs(100, HeightType.TOOLBAR);
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs()).thenReturn(specs);
        mCoordinator.setVisible(/* show= */ false, /* suppressAnimations= */ false);
        assertTrue(mIsVerticalTabsActiveSupplier.get());

        mCoordinator.onUiUpdateCompleted(
                /* oldWidth= */ 100,
                /* newWidth= */ 0,
                HeightType.TOOLBAR,
                HeightType.NOT_APPLICABLE);
        assertFalse(mIsVerticalTabsActiveSupplier.get());
    }

    @Test
    @SmallTest
    public void testOnUiUpdateCompleted_SideUiAlreadyHiddenFallback() {
        mCoordinator.setVisible(/* show= */ true, /* suppressAnimations= */ false);
        mCoordinator.onUiUpdateCompleted(
                /* oldWidth= */ 0,
                /* newWidth= */ 100,
                HeightType.NOT_APPLICABLE,
                HeightType.TOOLBAR);
        assertTrue(mIsVerticalTabsActiveSupplier.get());

        // When side UI width is already 0, hiding updates supplier immediately as a fallback.
        SideUiSpecs specs = new SideUiSpecs(0, HeightType.NOT_APPLICABLE);
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs()).thenReturn(specs);
        mCoordinator.setVisible(/* show= */ false, /* suppressAnimations= */ false);
        assertFalse(mIsVerticalTabsActiveSupplier.get());
    }

    @Test
    @SmallTest
    public void testActiveSupplierRemainsTrueWhenAutoHidden() {
        // Enable Vertical Tabs.
        mCoordinator.setVisible(/* show= */ true, /* suppressAnimations= */ false);
        mCoordinator.onUiUpdateCompleted(
                /* oldWidth= */ 0,
                /* newWidth= */ 100,
                HeightType.NOT_APPLICABLE,
                HeightType.TOOLBAR);
        assertTrue(mIsVerticalTabsActiveSupplier.get());

        // Simulate auto-hide during narrow window resize (newWidth = 0).
        mCoordinator.onUiUpdateCompleted(
                /* oldWidth= */ 100,
                /* newWidth= */ 0,
                HeightType.TOOLBAR,
                HeightType.NOT_APPLICABLE);

        // Supplier must remain true when auto-hidden to prevent Horizontal Tab Strip popup.
        assertTrue(mIsVerticalTabsActiveSupplier.get());
    }

    @Test
    @SmallTest
    public void testCollapseToggle() {
        RailCollapseListener listener = captureCollapseListener();

        // Initial state: expanded
        assertEquals(
                RailCollapseState.EXPANDED, mCoordinator.getRailCollapseStateByUserForTesting());
        assertShowableWidth(mExpandedRailWidth, mWideWindowWidth);

        // Collapse requested
        listener.onRailCollapseStateChangeRequested(RailCollapseState.COLLAPSED);
        assertEquals(
                RailCollapseState.COLLAPSED, mCoordinator.getRailCollapseStateByUserForTesting());
        assertShowableWidth(mCollapsedRailWidth, mWideWindowWidth);
        verify(mMockSideUiCoordinator).updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // Expand again
        listener.onRailCollapseStateChangeRequested(RailCollapseState.EXPANDED);
        assertEquals(
                RailCollapseState.EXPANDED, mCoordinator.getRailCollapseStateByUserForTesting());
        assertShowableWidth(mExpandedRailWidth, mWideWindowWidth);
    }

    @Test
    @SmallTest
    public void testHoverExpandAndCollapse() {
        RailCollapseListener listener = captureCollapseListener();

        // Collapse the rail
        listener.onRailCollapseStateChangeRequested(RailCollapseState.COLLAPSED);
        assertEquals(RailCollapseState.COLLAPSED, mCoordinator.getRailCollapseStateForTesting());
        assertShowableWidth(mCollapsedRailWidth, mWideWindowWidth);
        verify(mMockSideUiCoordinator).updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // Hover enter: rail expands for hovering
        listener.onRailCollapseStateChangeRequested(RailCollapseState.EXPANDED_FOR_HOVERING);
        assertEquals(
                RailCollapseState.EXPANDED_FOR_HOVERING,
                mCoordinator.getRailCollapseStateForTesting());
        assertShowableWidth(mExpandedRailWidth, mWideWindowWidth);
        verify(mMockSideUiCoordinator, times(2))
                .updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // Hover exit: rail collapses back
        listener.onRailCollapseStateChangeRequested(RailCollapseState.COLLAPSED);
        assertEquals(RailCollapseState.COLLAPSED, mCoordinator.getRailCollapseStateForTesting());
        assertShowableWidth(mCollapsedRailWidth, mWideWindowWidth);
        verify(mMockSideUiCoordinator, times(3))
                .updateUi(any(SideUiCoordinator.UiUpdateRequest.class));
    }

    @Test
    @SmallTest
    public void testPinRailWhenHoverExpanded() {
        RailCollapseListener listener = captureCollapseListener();

        // Start in COLLAPSED
        listener.onRailCollapseStateChangeRequested(RailCollapseState.COLLAPSED);
        assertEquals(RailCollapseState.COLLAPSED, mCoordinator.getRailCollapseStateForTesting());
        verify(mMockSideUiCoordinator, times(1))
                .updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // Hover expand
        listener.onRailCollapseStateChangeRequested(RailCollapseState.EXPANDED_FOR_HOVERING);
        assertEquals(
                RailCollapseState.EXPANDED_FOR_HOVERING,
                mCoordinator.getRailCollapseStateForTesting());
        verify(mMockSideUiCoordinator, times(2))
                .updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // User clicks expand chevron button to open pinned rail.
        listener.onRailCollapseStateChangeRequested(RailCollapseState.EXPANDED);
        assertEquals(RailCollapseState.EXPANDED, mCoordinator.getRailCollapseStateForTesting());
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.EXPANDED);
        // updateUi() is not called when transitioning from EXPANDED_FOR_HOVERING to EXPANDED
        verify(mMockSideUiCoordinator, times(2))
                .updateUi(any(SideUiCoordinator.UiUpdateRequest.class));
    }

    @Test
    @SmallTest
    public void testOnPreSideUiSpecsChange_Resize() {
        SideUiSpecs currentSpecs = new SideUiSpecs(mExpandedRailWidth, 0);
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs()).thenReturn(currentSpecs);

        SideUiSpecs newSpecs = new SideUiSpecs(mCollapsedRailWidth, 0);
        Transition transition = mCoordinator.onPreSideUiSpecsChange(newSpecs);

        assertNotNull(transition);
        TransitionSet transitionSet = (TransitionSet) transition;
        assertEquals(2, transitionSet.getTransitionCount());
        assertTrue(transitionSet.getTransitionAt(0) instanceof ChangeBounds);
        assertTrue(transitionSet.getTransitionAt(1) instanceof Fade);
    }

    @Test
    @SmallTest
    public void testOnPreSideUiSpecsChange_Show() {
        SideUiSpecs currentSpecs = new SideUiSpecs(0, 0);
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs()).thenReturn(currentSpecs);

        SideUiSpecs newSpecs = new SideUiSpecs(mExpandedRailWidth, 0);
        assertNull(mCoordinator.onPreSideUiSpecsChange(newSpecs));
    }

    @Test
    @SmallTest
    public void testOnPreSideUiSpecsChange_Hide() {
        SideUiSpecs currentSpecs = new SideUiSpecs(mExpandedRailWidth, 0);
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs()).thenReturn(currentSpecs);

        SideUiSpecs newSpecs = new SideUiSpecs(0, 0);
        assertNull(mCoordinator.onPreSideUiSpecsChange(newSpecs));
    }

    @Test
    @SmallTest
    public void testDeferredStateApplication_OnSideUiSpecsChanged() {
        RailCollapseListener listener = captureCollapseListener();

        // Trigger collapse request
        listener.onRailCollapseStateChangeRequested(RailCollapseState.COLLAPSED);

        // Verify setRailCollapseState is NOT called immediately
        verify(mMockTabListCoordinator, never()).setRailCollapseState(anyInt());

        // Trigger specs changed (static resize case)
        mCoordinator.onSideUiSpecsChanged(new SideUiSpecs(0, 0));

        // Verify setRailCollapseState is now called with COLLAPSED
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.COLLAPSED);
    }

    @Test
    @SmallTest
    public void testNarrowWindow_AutoCollapsesAndDisablesButton() {
        // When window is narrow (< 652dp), determineShowableSize returns collapsed width.
        setWindowWidthPx(mNarrowWindowWidth);
        assertShowableWidth(mCollapsedRailWidth, mNarrowWindowWidth);

        // layout change in setWindowWidthPx() should auto-collapse and disable collapse button.
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.COLLAPSED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(false);

        // When window is wide (>= 652dp), determineShowableSize returns expanded width.
        setWindowWidthPx(mWideWindowWidth);
        assertShowableWidth(mExpandedRailWidth, mWideWindowWidth);

        // layout change in setWindowWidthPx() should restore expanded state and re-enable collapse
        // button.
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.EXPANDED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(true);
    }

    @Test
    @SmallTest
    public void testNarrowWindow_AlreadyCollapsed_ReenablesButtonOnWindowExpanded() {
        RailCollapseListener listener = captureCollapseListener();

        // Collapse rail manually while in wide window.
        listener.onRailCollapseStateChangeRequested(RailCollapseState.COLLAPSED);

        // Shrink window to narrow (< 652dp). Layout listener fires and disables button.
        setWindowWidthPx(mNarrowWindowWidth);
        assertShowableWidth(mCollapsedRailWidth, mNarrowWindowWidth);
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.COLLAPSED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(false);

        // Expand window back to wide (>= 652dp). Specs diff is empty (74dp -> 74dp),
        // but determineShowableSize still returns collapsedWidth, and layout listener fires and
        // re-enables button.
        setWindowWidthPx(mWideWindowWidth);
        assertShowableWidth(mCollapsedRailWidth, mWideWindowWidth);
        verify(mMockTabListCoordinator, times(2)).setRailCollapseState(RailCollapseState.COLLAPSED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(true);
    }

    private RailCollapseListener captureCollapseListener() {
        verify(mMockTabListCoordinator).setCollapseListener(mCollapseListenerCaptor.capture());
        RailCollapseListener listener = mCollapseListenerCaptor.getValue();
        assertNotNull(listener);
        return listener;
    }

    private void assertShowableWidth(@Px int expectedWidth, @Px int windowWidth) {
        assertEquals(
                expectedWidth,
                mCoordinator.determineShowableSize(
                                /* availableWidth= */ mExpandedRailWidth,
                                windowWidth,
                                /* isFullscreen= */ false)
                        .width);
    }

    private void setWindowWidthPx(@Px int widthPx) {
        Configuration config = new Configuration(mActivity.getResources().getConfiguration());
        config.screenWidthDp = ViewUtils.pxToDp(mActivity, widthPx);
        mActivityController.configurationChange(config);
        if (mCoordinator != null) {
            mCoordinator.getView().layout(0, 0, widthPx, 1000);
        }
    }
}
