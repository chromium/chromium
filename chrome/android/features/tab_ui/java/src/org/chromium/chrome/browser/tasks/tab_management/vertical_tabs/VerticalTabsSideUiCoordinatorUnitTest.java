// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabsSideUiCoordinator.VIEW_WIDTH_DP;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.ui.base.ViewUtils;

/** Unit tests for {@link VerticalTabsSideUiCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabsSideUiCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private VerticalTabListCoordinator mMockTabListCoordinator;
    @Mock private SideUiCoordinator mMockSideUiCoordinator;

    private VerticalTabsSideUiCoordinator mCoordinator;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        View mockView = new View(mActivity);
        when(mMockTabListCoordinator.getView()).thenReturn(mockView);
        mCoordinator =
                new VerticalTabsSideUiCoordinator(
                        mActivity, mMockSideUiCoordinator, mMockTabListCoordinator);
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
    public void testDetermineContainerWidth() {
        int viewWidth = ViewUtils.dpToPx(mActivity, VIEW_WIDTH_DP);
        assertEquals(0, mCoordinator.determineContainerWidth(VIEW_WIDTH_DP, 500, 800));
        mCoordinator.setVisible(true);
        assertEquals(VIEW_WIDTH_DP, mCoordinator.determineContainerWidth(VIEW_WIDTH_DP, 500, 800));
    }

    @Test
    @SmallTest
    public void testSetWidth() {
        mCoordinator.setWidth(150);
        ViewGroup.LayoutParams layoutParams = mCoordinator.getView().getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(150, layoutParams.width);
    }
}
