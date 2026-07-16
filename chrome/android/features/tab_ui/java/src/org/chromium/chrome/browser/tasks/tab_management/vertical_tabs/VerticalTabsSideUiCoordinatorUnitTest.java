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
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabsSideUiCoordinator.VIEW_WIDTH_DP;

import android.app.Activity;
import android.transition.ChangeBounds;
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

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListCoordinator.RailCollapseListener;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.ui.base.ViewUtils;

import java.util.List;

/** Unit tests for {@link VerticalTabsSideUiCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabsSideUiCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private VerticalTabListCoordinator mMockTabListCoordinator;
    @Mock private SideUiCoordinator mMockSideUiCoordinator;

    @Captor private ArgumentCaptor<RailCollapseListener> mCollapseListenerCaptor;

    private VerticalTabsSideUiCoordinator mCoordinator;
    private Activity mActivity;
    private final SettableNonNullObservableSupplier<Boolean> mIsVerticalTabsActiveSupplier =
            ObservableSuppliers.createNonNull(false);

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        View mockView = new View(mActivity);
        when(mMockTabListCoordinator.getView()).thenReturn(mockView);
        when(mMockTabListCoordinator.getViewsForResizeAnimation()).thenReturn(List.of(mockView));

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
        mCoordinator.destroy();
        verify(mMockSideUiCoordinator).removeObserver(mCoordinator);
        verify(mMockTabListCoordinator).destroy();
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
        @Px int viewWidth = ViewUtils.dpToPx(mActivity, VIEW_WIDTH_DP);

        assertEquals(
                new SideUiSize(0, HeightType.NOT_APPLICABLE),
                mCoordinator.determineShowableSize(
                        /* availableWidth= */ viewWidth - 1, /* windowWidth= */ viewWidth + 100));
        assertEquals(
                new SideUiSize(viewWidth, HeightType.TOOLBAR),
                mCoordinator.determineShowableSize(
                        /* availableWidth= */ viewWidth, /* windowWidth= */ viewWidth + 100));
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
        mCoordinator.onUiUpdateCompleted(
                /* oldWidth= */ 0,
                /* newWidth= */ 100,
                HeightType.NOT_APPLICABLE,
                HeightType.TOOLBAR);
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
    public void testCollapseToggle() {
        verify(mMockTabListCoordinator).setCollapseListener(mCollapseListenerCaptor.capture());
        RailCollapseListener listener = mCollapseListenerCaptor.getValue();
        assertNotNull(listener);

        // Initial state: expanded
        @Px int expandedWidth = ViewUtils.dpToPx(mActivity, VIEW_WIDTH_DP);
        assertEquals(
                expandedWidth,
                mCoordinator.determineShowableSize(
                                /* availableWidth= */ expandedWidth,
                                /* windowWidth= */ expandedWidth + 100)
                        .width);

        // Collapse requested
        listener.onRailCollapseRequested(true);
        @Px
        int collapsedWidth =
                ViewUtils.dpToPx(mActivity, VerticalTabsSideUiCoordinator.COLLAPSED_WIDTH_DP);
        assertEquals(
                collapsedWidth,
                mCoordinator.determineShowableSize(
                                /* availableWidth= */ collapsedWidth,
                                /* windowWidth= */ collapsedWidth + 100)
                        .width);
        verify(mMockSideUiCoordinator).updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // Expand again
        listener.onRailCollapseRequested(false);
        assertEquals(
                expandedWidth,
                mCoordinator.determineShowableSize(
                                /* availableWidth= */ expandedWidth,
                                /* windowWidth= */ expandedWidth + 100)
                        .width);
    }

    @Test
    @SmallTest
    public void testOnPreSideUiSpecsChange_Resize() {
        // Mock current specs to be expanded
        @Px int expandedWidth = ViewUtils.dpToPx(mActivity, VIEW_WIDTH_DP);
        SideUiSpecs currentSpecs = new SideUiSpecs(expandedWidth, 0);
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs()).thenReturn(currentSpecs);

        // New specs are collapsed
        @Px
        int collapsedWidth =
                ViewUtils.dpToPx(mActivity, VerticalTabsSideUiCoordinator.COLLAPSED_WIDTH_DP);
        SideUiSpecs newSpecs = new SideUiSpecs(collapsedWidth, 0);

        // Call onPreSideUiSpecsChange
        Transition transition = mCoordinator.onPreSideUiSpecsChange(newSpecs);

        // Verify it returned a transition containing ChangeBounds
        assertNotNull(transition);
        TransitionSet transitionSet = (TransitionSet) transition;
        assertEquals(1, transitionSet.getTransitionCount());
        assertTrue(transitionSet.getTransitionAt(0) instanceof ChangeBounds);
    }

    @Test
    @SmallTest
    public void testOnPreSideUiSpecsChange_Show() {
        // Mock current specs to be hidden (0)
        SideUiSpecs currentSpecs = new SideUiSpecs(0, 0);
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs()).thenReturn(currentSpecs);

        // New specs are expanded
        @Px int expandedWidth = ViewUtils.dpToPx(mActivity, VIEW_WIDTH_DP);
        SideUiSpecs newSpecs = new SideUiSpecs(expandedWidth, 0);

        // Should return null for show events
        assertNull(mCoordinator.onPreSideUiSpecsChange(newSpecs));
    }

    @Test
    @SmallTest
    public void testOnPreSideUiSpecsChange_Hide() {
        // Mock current specs to be expanded
        @Px int expandedWidth = ViewUtils.dpToPx(mActivity, VIEW_WIDTH_DP);
        SideUiSpecs currentSpecs = new SideUiSpecs(expandedWidth, 0);
        when(mMockSideUiCoordinator.getCurrentSideUiSpecs()).thenReturn(currentSpecs);

        // New specs are hidden (0)
        SideUiSpecs newSpecs = new SideUiSpecs(0, 0);

        // Should return null for hide events
        assertNull(mCoordinator.onPreSideUiSpecsChange(newSpecs));
    }

    @Test
    @SmallTest
    public void testDeferredStateApplication_OnTransitionBegun() {
        verify(mMockTabListCoordinator).setCollapseListener(mCollapseListenerCaptor.capture());
        RailCollapseListener listener = mCollapseListenerCaptor.getValue();

        // Trigger collapse request
        listener.onRailCollapseRequested(true);

        // Verify setCollapsed is NOT called immediately
        verify(mMockTabListCoordinator, never()).setCollapsed(any(Boolean.class));

        // Trigger transition begun
        mCoordinator.onTransitionBegun(new SideUiSpecs(0, 0));

        // Verify setCollapsed is now called with true
        verify(mMockTabListCoordinator).setCollapsed(true);
    }

    @Test
    @SmallTest
    public void testDeferredStateApplication_OnSideUiSpecsChanged() {
        verify(mMockTabListCoordinator).setCollapseListener(mCollapseListenerCaptor.capture());
        RailCollapseListener listener = mCollapseListenerCaptor.getValue();

        // Trigger collapse request
        listener.onRailCollapseRequested(true);

        // Verify setCollapsed is NOT called immediately
        verify(mMockTabListCoordinator, never()).setCollapsed(any(Boolean.class));

        // Trigger specs changed (static resize case)
        mCoordinator.onSideUiSpecsChanged(new SideUiSpecs(0, 0));

        // Verify setCollapsed is now called with true
        verify(mMockTabListCoordinator).setCollapsed(true);
    }
}
