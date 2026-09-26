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
import static org.mockito.Mockito.clearInvocations;
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
import android.transition.TransitionValues;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Px;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.ui.base.ViewUtils;

import java.util.Map;

/** Unit tests for {@link VerticalTabsSideUiCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabsSideUiCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private VerticalTabListCoordinator mMockTabListCoordinator;
    @Mock private SideUiCoordinator mMockSideUiCoordinator;
    @Mock private Tab mTab;

    private VerticalTabRailCollapseController mCollapseController;
    private VerticalTabsSideUiCoordinator mCoordinator;
    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private View mTabListView;
    private @Px int mWideWindowWidth;
    private @Px int mMediumWindowWidth;
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
        mMediumWindowWidth = ViewUtils.dpToPx(mActivity, 600);
        mNarrowWindowWidth = ViewUtils.dpToPx(mActivity, 500);
        mExpandedRailWidth = ViewUtils.dpToPx(mActivity, VIEW_WIDTH_DP);
        mCollapsedRailWidth =
                ViewUtils.dpToPx(mActivity, VerticalTabsSideUiCoordinator.COLLAPSED_WIDTH_DP);
        // Initialize window width to wide before mCoordinator creation to avoid
        // triggering a layout change event during constructor setup.
        setWindowWidthPx(mWideWindowWidth);
        mTabListView = new View(mActivity);
        when(mMockTabListCoordinator.getView()).thenReturn(mTabListView);
        mCollapseController =
                new VerticalTabRailCollapseController(
                        mMockTabListCoordinator::setRailCollapseState,
                        mMockTabListCoordinator::setCollapseButtonEnabled);
        when(mMockTabListCoordinator.getCollapseController()).thenReturn(mCollapseController);

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
        // toggleCollapseState() and resizes persist user preferences; reset them between tests.
        VerticalTabUtils.resetSharedPrefsForTesting();
    }

    @Test
    public void testDetermineShowableSize_isAutoHiddenSupplierWhenHiddenDueToNarrow() {
        mCoordinator.setVisible(/* show= */ true, /* suppressAnimations= */ false);
        mCoordinator.determineShowableSize(
                mCollapsedRailWidth - 1, mWideWindowWidth, /* isFullscreen= */ false);

        assertTrue(mCoordinator.getIsAutoHiddenSupplier().get());

        mCoordinator.determineShowableSize(
                mCollapsedRailWidth, mWideWindowWidth, /* isFullscreen= */ false);

        assertFalse(mCoordinator.getIsAutoHiddenSupplier().get());

        mCoordinator.setVisible(/* show= */ false, /* suppressAnimations= */ false);
        assertFalse(mCoordinator.getIsAutoHiddenSupplier().get());
    }

    @Test
    public void testRegistration() {
        // Constructor is called in setUp(), verify registration happened.
        verify(mMockSideUiCoordinator).addObserver(mCoordinator);
        verify(mMockSideUiCoordinator).registerSideUiContainer(mCoordinator);
    }

    @Test
    public void testDestroy() {
        mCoordinator.setVisible(/* show= */ true, /* suppressAnimations= */ false);
        mCoordinator.onUiUpdateCompleted(
                /* oldReservedWidth= */ 0,
                /* newReservedWidth= */ 100,
                HeightType.NOT_APPLICABLE,
                HeightType.TOOLBAR);
        assertTrue(mIsVerticalTabsActiveSupplier.get());

        mCoordinator.destroy();
        verify(mMockSideUiCoordinator).removeObserver(mCoordinator);
        verify(mMockSideUiCoordinator).unregisterSideUiContainer(mCoordinator);
        verify(mMockTabListCoordinator).destroy();
        assertFalse(mIsVerticalTabsActiveSupplier.get());
    }

    @Test
    public void testGetView() {
        View view = mCoordinator.getView();
        assertNotNull(view);
        assertTrue(view instanceof FrameLayout);
    }

    @Test
    public void testGetAnchorSide() {
        assertEquals(AnchorSide.LEFT, mCoordinator.getAnchorSide());
    }

    @Test
    public void testDetermineShowableSize() {
        assertEquals(
                new SideUiSize(0, HeightType.NOT_APPLICABLE),
                mCoordinator.determineShowableSize(
                        /* availableWidth= */ mCollapsedRailWidth - 1,
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
    public void testDetermineShowableSize_collapsedState() {
        mCollapseController.toggleCollapseState();

        // Available width smaller than expanded rail width (240dp), but >= collapsed rail width
        // (76dp)
        assertEquals(
                new SideUiSize(mCollapsedRailWidth, HeightType.TOOLBAR),
                mCoordinator.determineShowableSize(
                        /* availableWidth= */ mCollapsedRailWidth + 10,
                        /* windowWidth= */ mWideWindowWidth,
                        /* isFullscreen= */ false));
    }

    @Test
    public void testDetermineShowableSize_FullscreenReturnsZeroWidth() {
        assertEquals(
                new SideUiSize(0, HeightType.NOT_APPLICABLE),
                mCoordinator.determineShowableSize(
                        /* availableWidth= */ mExpandedRailWidth,
                        /* windowWidth= */ mWideWindowWidth,
                        /* isFullscreen= */ true));
    }

    @Test
    public void testHasContentToShow() {
        mCoordinator.setVisible(/* show= */ true, /* suppressAnimations= */ false);
        assertTrue(mCoordinator.hasContentToShow(mTab));

        mCoordinator.setVisible(/* show= */ false, /* suppressAnimations= */ false);
        assertFalse(mCoordinator.hasContentToShow(mTab));
    }

    @Test
    public void testSetRenderedWidth() {
        mCoordinator.setRenderedWidth(150);
        ViewGroup.LayoutParams layoutParams = mCoordinator.getView().getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(150, layoutParams.width);
    }

    @Test
    public void testOnUiUpdateCompleted() {
        mCoordinator.setVisible(/* show= */ true, /* suppressAnimations= */ false);
        mCoordinator.onUiUpdateCompleted(
                /* oldReservedWidth= */ 0,
                /* newReservedWidth= */ 100,
                HeightType.NOT_APPLICABLE,
                HeightType.TOOLBAR);
        assertTrue(mIsVerticalTabsActiveSupplier.get());

        // When hiding with animation, supplier remains true until onUiUpdateCompleted.
        SideUiSpecs specs = new SideUiSpecs(100, HeightType.TOOLBAR);
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs()).thenReturn(specs);
        mCoordinator.setVisible(/* show= */ false, /* suppressAnimations= */ false);
        assertTrue(mIsVerticalTabsActiveSupplier.get());

        mCoordinator.onUiUpdateCompleted(
                /* oldReservedWidth= */ 100,
                /* newReservedWidth= */ 0,
                HeightType.TOOLBAR,
                HeightType.NOT_APPLICABLE);
        assertFalse(mIsVerticalTabsActiveSupplier.get());
    }

    @Test
    public void testOnUiUpdateCompleted_SideUiAlreadyHiddenFallback() {
        mCoordinator.setVisible(/* show= */ true, /* suppressAnimations= */ false);
        mCoordinator.onUiUpdateCompleted(
                /* oldReservedWidth= */ 0,
                /* newReservedWidth= */ 100,
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
    public void testActiveSupplierRemainsTrueWhenAutoHidden() {
        // Enable Vertical Tabs.
        mCoordinator.setVisible(/* show= */ true, /* suppressAnimations= */ false);
        mCoordinator.onUiUpdateCompleted(
                /* oldReservedWidth= */ 0,
                /* newReservedWidth= */ 100,
                HeightType.NOT_APPLICABLE,
                HeightType.TOOLBAR);
        assertTrue(mIsVerticalTabsActiveSupplier.get());

        // Simulate auto-hide during narrow window resize (newWidth = 0).
        mCoordinator.onUiUpdateCompleted(
                /* oldReservedWidth= */ 100,
                /* newReservedWidth= */ 0,
                HeightType.TOOLBAR,
                HeightType.NOT_APPLICABLE);

        // Supplier must remain true when auto-hidden to prevent Horizontal Tab Strip popup.
        assertTrue(mIsVerticalTabsActiveSupplier.get());
    }

    @Test
    public void testCollapseToggle() {
        // Initial state: expanded
        assertEquals(RailCollapseState.EXPANDED, mCoordinator.getRailCollapseStateForTesting());
        assertShowableWidth(mExpandedRailWidth, mWideWindowWidth);

        // Collapse requested
        mCollapseController.toggleCollapseState();
        assertEquals(RailCollapseState.COLLAPSED, mCoordinator.getRailCollapseStateForTesting());
        assertShowableWidth(mCollapsedRailWidth, mWideWindowWidth);
        verify(mMockSideUiCoordinator).updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // Expand again
        mCollapseController.toggleCollapseState();
        assertEquals(RailCollapseState.EXPANDED, mCoordinator.getRailCollapseStateForTesting());
        assertShowableWidth(mExpandedRailWidth, mWideWindowWidth);
    }

    @Test
    public void testHoverExpandAndCollapse() {
        // Collapse the rail
        mCollapseController.toggleCollapseState();
        assertEquals(RailCollapseState.COLLAPSED, mCoordinator.getRailCollapseStateForTesting());
        assertShowableWidth(mCollapsedRailWidth, mWideWindowWidth);
        verify(mMockSideUiCoordinator).updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // Hover enter: the rail renders expanded over the web contents, but keeps reserving its
        // collapsed width so nothing outside the rail moves.
        mCollapseController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        assertEquals(
                RailCollapseState.EXPANDED_FOR_HOVERING,
                mCoordinator.getRailCollapseStateForTesting());
        assertShowableSize(mCollapsedRailWidth, mExpandedRailWidth, mWideWindowWidth);
        verify(mMockSideUiCoordinator, times(2))
                .updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // Hover exit: rail collapses back
        mCollapseController.expandOrCollapseOnHover(RailCollapseState.COLLAPSED);
        assertEquals(RailCollapseState.COLLAPSED, mCoordinator.getRailCollapseStateForTesting());
        assertShowableSize(mCollapsedRailWidth, mCollapsedRailWidth, mWideWindowWidth);
        verify(mMockSideUiCoordinator, times(3))
                .updateUi(any(SideUiCoordinator.UiUpdateRequest.class));
    }

    @Test
    public void testPinRailWhenHoverExpanded() {
        // Start in COLLAPSED
        mCollapseController.toggleCollapseState();
        assertEquals(RailCollapseState.COLLAPSED, mCoordinator.getRailCollapseStateForTesting());
        verify(mMockSideUiCoordinator, times(1))
                .updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // Hover expand
        mCollapseController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        assertEquals(
                RailCollapseState.EXPANDED_FOR_HOVERING,
                mCoordinator.getRailCollapseStateForTesting());
        verify(mMockSideUiCoordinator, times(2))
                .updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // User clicks expand chevron button to open pinned rail. The rail keeps the same rendered
        // width, but now reserves it, so the web contents shrink for the first time.
        mCollapseController.toggleCollapseState();
        assertEquals(RailCollapseState.EXPANDED, mCoordinator.getRailCollapseStateForTesting());
        assertShowableSize(mExpandedRailWidth, mExpandedRailWidth, mWideWindowWidth);
        verify(mMockSideUiCoordinator, times(3))
                .updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // The new state reaches the rail only once that Side UI update is applied.
        verify(mMockTabListCoordinator, never()).setRailCollapseState(RailCollapseState.EXPANDED);
        mCoordinator.onSideUiSpecsChanged(
                specs(mExpandedRailWidth, mExpandedRailWidth),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ false));
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.EXPANDED);
    }

    @Test
    public void testHoverExpanded_DoesNotSupportManualResize() {
        enableManualResize();
        mCollapseController.toggleCollapseState();
        assertTrue(mCoordinator.supportsManualResize());

        mCollapseController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        assertFalse(mCoordinator.supportsManualResize());

        mCollapseController.expandOrCollapseOnHover(RailCollapseState.COLLAPSED);
        assertTrue(mCoordinator.supportsManualResize());
    }

    @Test
    public void testHoverExpanded_RendersUserResizedWidth() {
        enableManualResize();
        mCoordinator.onResizeCommitted(ViewUtils.dpToPx(mActivity, 300));
        mCollapseController.toggleCollapseState();
        assertEquals(RailCollapseState.COLLAPSED, mCoordinator.getRailCollapseStateForTesting());

        // The hover overlay previews the width the rail would be pinned at.
        mCollapseController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        assertShowableSize(mCollapsedRailWidth, ViewUtils.dpToPx(mActivity, 300), mWideWindowWidth);
    }

    @Test
    public void testOnPreSideUiSpecsChange_Resize() {
        SideUiSpecs currentSpecs = new SideUiSpecs(mExpandedRailWidth, 0);
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs()).thenReturn(currentSpecs);

        SideUiSpecs newSpecs = new SideUiSpecs(mCollapsedRailWidth, 0);
        Transition transition =
                mCoordinator.onPreSideUiSpecsChange(
                        newSpecs,
                        UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));

        assertNotNull(transition);
        TransitionSet transitionSet = (TransitionSet) transition;
        assertEquals(2, transitionSet.getTransitionCount());
        assertTrue(transitionSet.getTransitionAt(0) instanceof ChangeBounds);
        assertTrue(transitionSet.getTransitionAt(1) instanceof Fade);
        verify(mMockTabListCoordinator).setInTransition(true);
    }

    @Test
    public void testOnPreSideUiSpecsChange_OnlyTargetsRailViews() {
        // Place the rail next to views outside of it (e.g. the toolbar and the bookmark bar).
        FrameLayout windowRoot = new FrameLayout(mActivity);
        FrameLayout outsideContainer = new FrameLayout(mActivity);
        View outsideChild = new View(mActivity);
        outsideContainer.addView(outsideChild);
        windowRoot.addView(outsideContainer);
        windowRoot.addView(mCoordinator.getView());

        when(mMockSideUiCoordinator.getCurrentSideUiSpecs())
                .thenReturn(new SideUiSpecs(mExpandedRailWidth, 0));
        Transition transition =
                mCoordinator.onPreSideUiSpecsChange(
                        new SideUiSpecs(mCollapsedRailWidth, 0),
                        UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        assertNotNull(transition);

        // Rail views are captured.
        for (View view : new View[] {mCoordinator.getView(), mTabListView}) {
            TransitionValues values = new TransitionValues(view);
            transition.captureStartValues(values);
            assertFalse(values.values.isEmpty());
        }

        // Views outside the rail are not captured.
        for (View view : new View[] {windowRoot, outsideContainer, outsideChild}) {
            TransitionValues values = new TransitionValues(view);
            transition.captureStartValues(values);
            assertTrue(values.values.isEmpty());
        }
    }

    @Test
    public void testOnPreSideUiSpecsChange_HoverEnterFollowsRenderedWidth() {
        // Collapsed and not overlaying anything yet.
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs())
                .thenReturn(specs(mCollapsedRailWidth, mCollapsedRailWidth));

        // Hover enter: the reserved width is unchanged, only the rendered width grows, and the
        // rail's contents must still animate to it.
        Transition transition =
                mCoordinator.onPreSideUiSpecsChange(
                        specs(mCollapsedRailWidth, mExpandedRailWidth),
                        UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));

        assertNotNull(transition);
        verify(mMockTabListCoordinator).setInTransition(true);
    }

    // TODO(crbug.com/542280452): The rail should animate when it is pinned from hover, as the
    // pinned UI will differ from the hover overlay. Update this test once it does.
    @Test
    public void testOnPreSideUiSpecsChange_PinningHoverExpandedRailKeepsRailStill() {
        // Expanded on hover over the web contents.
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs())
                .thenReturn(specs(mCollapsedRailWidth, mExpandedRailWidth));

        // Pinning it only reserves what is already rendered, so the rail itself does not move. The
        // web contents and the toolbar still animate, driven by their own Side UI observers.
        assertNull(
                mCoordinator.onPreSideUiSpecsChange(
                        specs(mExpandedRailWidth, mExpandedRailWidth),
                        UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true)));
        verify(mMockTabListCoordinator, never()).setInTransition(true);
    }

    @Test
    public void testOnTransitionEnded_ResetsInTransition() {
        SideUiSpecs newSpecs = new SideUiSpecs(mCollapsedRailWidth, 0);
        mCoordinator.onTransitionEnded(
                newSpecs, UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        verify(mMockTabListCoordinator).setInTransition(false);
    }

    @Test
    public void testOnPreSideUiSpecsChange_Show() {
        SideUiSpecs currentSpecs = new SideUiSpecs(0, 0);
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs()).thenReturn(currentSpecs);

        SideUiSpecs newSpecs = new SideUiSpecs(mExpandedRailWidth, 0);
        assertNull(
                mCoordinator.onPreSideUiSpecsChange(
                        newSpecs,
                        UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true)));
        verify(mMockTabListCoordinator, never()).setInTransition(true);
    }

    @Test
    public void testOnPreSideUiSpecsChange_Hide() {
        SideUiSpecs currentSpecs = new SideUiSpecs(mExpandedRailWidth, 0);
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs()).thenReturn(currentSpecs);

        SideUiSpecs newSpecs = new SideUiSpecs(0, 0);
        assertNull(
                mCoordinator.onPreSideUiSpecsChange(
                        newSpecs,
                        UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true)));
        verify(mMockTabListCoordinator, never()).setInTransition(true);
    }

    @Test
    public void testDeferredStateApplication_OnSideUiSpecsChanged() {
        // Trigger collapse request
        mCollapseController.toggleCollapseState();

        // Verify setRailCollapseState is NOT called immediately
        verify(mMockTabListCoordinator, never()).setRailCollapseState(anyInt());

        // Trigger specs changed (static resize case)
        mCoordinator.onSideUiSpecsChanged(
                new SideUiSpecs(0, 0),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));

        // Verify setRailCollapseState is now called with COLLAPSED
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.COLLAPSED);
    }

    @Test
    public void testNarrowWindow_AutoCollapsesAndDisablesButton() {
        // When window is narrow (< 504dp), determineShowableSize returns collapsed width and
        // auto-collapses.
        setWindowWidthPx(mNarrowWindowWidth);
        assertShowableWidth(mCollapsedRailWidth, mNarrowWindowWidth);
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.COLLAPSED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(false);
        clearInvocations(mMockTabListCoordinator);

        // onSideUiSpecsChanged() should maintain auto-collapse and disabled collapse button.
        mCoordinator.onSideUiSpecsChanged(
                new SideUiSpecs(mCollapsedRailWidth, 0),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.COLLAPSED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(false);
        clearInvocations(mMockTabListCoordinator);

        // When window is wide (>= 504dp), determineShowableSize returns expanded width.
        setWindowWidthPx(mWideWindowWidth);
        assertShowableWidth(mExpandedRailWidth, mWideWindowWidth);
        clearInvocations(mMockTabListCoordinator);

        // onSideUiSpecsChanged() should restore expanded state and re-enable collapse button.
        mCoordinator.onSideUiSpecsChanged(
                new SideUiSpecs(mExpandedRailWidth, 0),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.EXPANDED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(true);
    }

    @Test
    public void testNarrowWindow_AlreadyCollapsed_ReenablesButtonOnWindowExpanded() {
        // Collapse rail manually while in wide window.
        mCollapseController.toggleCollapseState();
        clearInvocations(mMockTabListCoordinator);

        // Shrink window to narrow (< 504dp). determineShowableSize updates button state for empty
        // spec diff.
        setWindowWidthPx(mNarrowWindowWidth);
        assertShowableWidth(mCollapsedRailWidth, mNarrowWindowWidth);
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.COLLAPSED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(false);
        clearInvocations(mMockTabListCoordinator);

        // Expand window back to wide (>= 504dp). Specs diff is empty (76dp -> 76dp),
        // determineShowableSize re-enables button.
        setWindowWidthPx(mWideWindowWidth);
        assertShowableWidth(mCollapsedRailWidth, mWideWindowWidth);
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.COLLAPSED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
    public void testAutoResize_ScalesWidthWithWindow() {
        setWindowWidthPx(mMediumWindowWidth);
        int minWebContentsWidthPx =
                ViewUtils.dpToPx(mActivity, SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP);
        int availableWidthPx = mMediumWindowWidth - minWebContentsWidthPx;
        int expectedMediumWidth =
                Math.min(
                        mExpandedRailWidth,
                        Math.min(
                                Math.round(
                                        mMediumWindowWidth
                                                * VerticalTabUtils.EXPANDED_WINDOW_WIDTH_RATIO),
                                availableWidthPx));
        assertShowableWidth(expectedMediumWidth, mMediumWindowWidth);
        clearInvocations(mMockTabListCoordinator);
        mCoordinator.onSideUiSpecsChanged(
                new SideUiSpecs(expectedMediumWidth, 0),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.EXPANDED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
    public void testAutoResize_BelowMinWebContents_HidesVerticalTabs() {
        @Px int hiddenWindowWidth = ViewUtils.dpToPx(mActivity, 400);
        setWindowWidthPx(hiddenWindowWidth);
        assertShowableWidth(0, hiddenWindowWidth);
        clearInvocations(mMockTabListCoordinator);
        mCoordinator.onSideUiSpecsChanged(
                new SideUiSpecs(0, 0),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.COLLAPSED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(false);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
    public void testAutoResize_NarrowWindowThreshold_CollapsesRail() {
        // Threshold: max(412 + 92, round(92 / 0.33)) = 504dp.
        // 503dp (< 504dp) -> Narrow: Rail collapses and collapse button is disabled.
        @Px int narrowWidthPx = ViewUtils.dpToPx(mActivity, 503);
        setWindowWidthPx(narrowWidthPx);
        assertShowableWidth(mCollapsedRailWidth, narrowWidthPx);
        clearInvocations(mMockTabListCoordinator);
        mCoordinator.onSideUiSpecsChanged(
                new SideUiSpecs(mCollapsedRailWidth, 0),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.COLLAPSED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(false);

        // 504dp (>= 504dp) -> Not narrow: Rail expands with auto-resize width (92dp) and button is
        // enabled.
        @Px int wideWidthPx = ViewUtils.dpToPx(mActivity, 504);
        setWindowWidthPx(wideWidthPx);
        @Px int expectedExpandedWidthPx = ViewUtils.dpToPx(mActivity, 92);
        assertShowableWidth(expectedExpandedWidthPx, wideWidthPx);
        clearInvocations(mMockTabListCoordinator);
        mCoordinator.onSideUiSpecsChanged(
                new SideUiSpecs(expectedExpandedWidthPx, 0),
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));
        verify(mMockTabListCoordinator).setRailCollapseState(RailCollapseState.EXPANDED);
        verify(mMockTabListCoordinator).setCollapseButtonEnabled(true);
    }

    @Test
    public void testRequestKeyboardFocus_DelegatesToTabListCoordinator() {
        mCoordinator.requestKeyboardFocus();
        verify(mMockTabListCoordinator).requestKeyboardFocus();
    }

    @Test
    public void testContainsKeyboardFocus() {
        assertFalse(mCoordinator.containsKeyboardFocus());

        mTabListView.setFocusableInTouchMode(true);
        mTabListView.requestFocus();
        assertTrue(mCoordinator.containsKeyboardFocus());

        when(mMockTabListCoordinator.getView()).thenReturn(null);
        assertFalse(mCoordinator.containsKeyboardFocus());
    }

    @Test
    public void testOpenKeyboardFocusedContextMenu_DelegatesToTabListCoordinator() {
        // Without focus, returns false without delegating.
        assertFalse(mCoordinator.openKeyboardFocusedContextMenu());
        verify(mMockTabListCoordinator, never()).openKeyboardFocusedContextMenu();

        // With focus, delegates to tab list coordinator.
        mTabListView.setFocusableInTouchMode(true);
        mTabListView.requestFocus();

        when(mMockTabListCoordinator.openKeyboardFocusedContextMenu()).thenReturn(true);
        assertTrue(mCoordinator.openKeyboardFocusedContextMenu());
        verify(mMockTabListCoordinator).openKeyboardFocusedContextMenu();

        when(mMockTabListCoordinator.openKeyboardFocusedContextMenu()).thenReturn(false);
        assertFalse(mCoordinator.openKeyboardFocusedContextMenu());
    }

    @Test
    public void testSupportsManualResize() {
        // Off by default.
        assertFalse(mCoordinator.supportsManualResize());

        enableManualResize();
        assertTrue(mCoordinator.supportsManualResize());

        // A user-collapsed rail can still be manually resized.
        mCollapseController.toggleCollapseState();
        assertTrue(mCoordinator.supportsManualResize());
    }

    @Test
    public void testSupportsManualResize_ForcedCollapsedWindow() {
        enableManualResize();

        // A window too narrow to expand forces the rail collapsed, which removes the handle.
        mCoordinator.determineShowableSize(
                mCollapsedRailWidth, mNarrowWindowWidth, /* isFullscreen= */ false);
        assertFalse(mCoordinator.supportsManualResize());
    }

    @Test
    public void testOnResizeLive_AppliesProposedWidth() {
        enableManualResize();

        @Px int proposedWidth = ViewUtils.dpToPx(mActivity, 300);
        mCoordinator.onResizeLive(proposedWidth);

        assertShowableWidth(proposedWidth, mWideWindowWidth);
    }

    @Test
    public void testOnResizeLive_ClampsToRailBounds() {
        enableManualResize();

        mCoordinator.onResizeLive(ViewUtils.dpToPx(mActivity, 10));
        assertShowableWidth(
                ViewUtils.dpToPx(mActivity, VerticalTabUtils.MIN_EXPANDED_WIDTH_DP),
                mWideWindowWidth);

        mCoordinator.onResizeLive(ViewUtils.dpToPx(mActivity, 10000));
        assertShowableWidth(
                ViewUtils.dpToPx(mActivity, VerticalTabUtils.MAX_EXPANDED_WIDTH_DP),
                ViewUtils.dpToPx(mActivity, 2000));
    }

    @Test
    public void testOnResizeCommitted_PersistsWidthAndExpandsFromCollapsed() {
        enableManualResize();
        mCollapseController.toggleCollapseState();
        assertTrue(VerticalTabUtils.isRailCollapsedFromSharedPref());

        mCoordinator.onResizeLive(ViewUtils.dpToPx(mActivity, 200));
        mCoordinator.onResizeCommitted(ViewUtils.dpToPx(mActivity, 300));

        assertFalse(VerticalTabUtils.isRailCollapsedFromSharedPref());
        assertEquals(RailCollapseState.EXPANDED, mCoordinator.getRailCollapseStateForTesting());
        assertEquals(300, VerticalTabUtils.getUserResizedWidthDp());
        // The persisted width is used once the drag is over.
        assertShowableWidth(ViewUtils.dpToPx(mActivity, 300), mWideWindowWidth);
    }

    @Test
    public void testOnResizeCommitted_BelowMinimumCollapsesRail() {
        enableManualResize();

        mCoordinator.onResizeCommitted(ViewUtils.dpToPx(mActivity, 40));

        assertTrue(VerticalTabUtils.isRailCollapsedFromSharedPref());
        assertEquals(RailCollapseState.COLLAPSED, mCoordinator.getRailCollapseStateForTesting());
        assertShowableWidth(mCollapsedRailWidth, mWideWindowWidth);
    }

    private void enableManualResize() {
        FeatureOverrides.newBuilder()
                .param(ChromeFeatureList.ANDROID_VERTICAL_TABS, "manual_resize", true)
                .apply();
    }

    private void assertShowableWidth(@Px int expectedWidth, @Px int windowWidth) {
        assertEquals(expectedWidth, determineShowableSize(windowWidth).mReservedWidth);
    }

    private void assertShowableSize(
            @Px int expectedReservedWidth, @Px int expectedRenderedWidth, @Px int windowWidth) {
        assertEquals(
                new SideUiSize(expectedReservedWidth, expectedRenderedWidth, HeightType.TOOLBAR),
                determineShowableSize(windowWidth));
    }

    private SideUiSize determineShowableSize(@Px int windowWidth) {
        int minWebContentsWidthPx =
                ViewUtils.dpToPx(mActivity, SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP);
        int availableWidth = windowWidth - minWebContentsWidthPx;
        return mCoordinator.determineShowableSize(
                /* availableWidth= */ availableWidth, windowWidth, /* isFullscreen= */ false);
    }

    /** Returns specs for the rail, which is anchored to the left, with no other container. */
    private SideUiSpecs specs(@Px int reservedWidth, @Px int renderedWidth) {
        return new SideUiSpecs(
                Map.of(
                        AnchorSide.LEFT,
                        new SideUiSize(reservedWidth, renderedWidth, HeightType.TOOLBAR)));
    }

    private void setWindowWidthPx(@Px int widthPx) {
        Configuration config = new Configuration(mActivity.getResources().getConfiguration());
        config.screenWidthDp = ViewUtils.pxToDp(mActivity, widthPx);
        mActivityController.configurationChange(config);
    }
}
