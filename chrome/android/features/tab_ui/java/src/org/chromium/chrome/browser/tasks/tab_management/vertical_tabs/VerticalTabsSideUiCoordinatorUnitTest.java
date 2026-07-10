// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabsSideUiCoordinator.VIEW_WIDTH_DP;

import android.app.Activity;
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
import org.chromium.ui.base.ViewUtils;

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
    public void testDestroy() {
        mCoordinator.destroy();
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
    public void testDetermineShowableWidth() {
        @Px int viewWidth = ViewUtils.dpToPx(mActivity, VIEW_WIDTH_DP);

        assertEquals(
                0,
                mCoordinator.determineShowableWidth(
                        /* availableWidth= */ viewWidth - 1, /* windowWidth= */ viewWidth + 100));
        assertEquals(
                viewWidth,
                mCoordinator.determineShowableWidth(
                        /* availableWidth= */ viewWidth, /* windowWidth= */ viewWidth + 100));
    }

    @Test
    @SmallTest
    public void testHasContentToShow() {
        mCoordinator.setVisible(true);
        assertTrue(mCoordinator.hasContentToShow());

        mCoordinator.setVisible(false);
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
        mCoordinator.onUiUpdateCompleted(/* oldWidth= */ 0, /* newWidth= */ 100);
        assertTrue(mIsVerticalTabsActiveSupplier.get());

        mCoordinator.onUiUpdateCompleted(/* oldWidth= */ 100, /* newWidth= */ 0);
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
                mCoordinator.determineShowableWidth(
                        /* availableWidth= */ expandedWidth,
                        /* windowWidth= */ expandedWidth + 100));

        // Collapse
        listener.onRailCollapseChanged(true);
        @Px
        int collapsedWidth =
                ViewUtils.dpToPx(mActivity, VerticalTabsSideUiCoordinator.COLLAPSED_WIDTH_DP);
        assertEquals(
                collapsedWidth,
                mCoordinator.determineShowableWidth(
                        /* availableWidth= */ collapsedWidth,
                        /* windowWidth= */ collapsedWidth + 100));
        verify(mMockSideUiCoordinator).updateUi(any(SideUiCoordinator.UiUpdateRequest.class));

        // Expand again
        listener.onRailCollapseChanged(false);
        assertEquals(
                expandedWidth,
                mCoordinator.determineShowableWidth(
                        /* availableWidth= */ expandedWidth,
                        /* windowWidth= */ expandedWidth + 100));
    }
}
