// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomeModulesRecyclerViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mView;
    @Mock private View mView1;

    private Activity mActivity;
    private HomeModulesRecyclerView mRecyclerView;
    private HomeModulesRecyclerView mRecyclerViewSpy;
    private int mModuleInternalPaddingPx;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mRecyclerView =
                (HomeModulesRecyclerView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.home_modules_recycler_view_layout, null);
        mActivity.setContentView(mRecyclerView);
        mRecyclerViewSpy = spy(mRecyclerView);

        mModuleInternalPaddingPx =
                ApplicationProvider.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.module_internal_padding);
    }

    @Test
    @SmallTest
    public void testOnDraw_OneItermPerScreen() {
        int itemPerScreen = 1;
        int startMarginPx = 0;
        int measuredWidth = 500;
        mRecyclerView.initialize(/* isTablet= */ true, startMarginPx, itemPerScreen);

        MarginLayoutParams marginLayoutParams = new MarginLayoutParams(100, 100);
        when(mView.getLayoutParams()).thenReturn(marginLayoutParams);
        startMarginPx = 5;
        mRecyclerView.setStartMarginPxForTesting(startMarginPx);

        // Verifies when there is one item per screen, the width is set to MATCH_PARENT.
        mRecyclerView.onDrawImplTablet(mView, 3, measuredWidth);
        assertEquals(MATCH_PARENT, marginLayoutParams.width);
        assertEquals(startMarginPx, marginLayoutParams.getMarginStart());
        assertEquals(startMarginPx, marginLayoutParams.getMarginEnd());
        verify(mView).setLayoutParams(eq(marginLayoutParams));

        // Verifies that setLayoutParams() is called again to update the margins.
        mRecyclerView.onDrawImplTablet(mView, 3, measuredWidth);
        assertEquals(MATCH_PARENT, marginLayoutParams.width);
        verify(mView, times(2)).setLayoutParams(eq(marginLayoutParams));
    }

    @Test
    @SmallTest
    public void testOnDraw_MultipleItemsPerScreen() {
        int itemPerScreen = 2;
        int startMarginPx = 0;
        int measuredWidth = 500;
        mRecyclerView.initialize(/* isTablet= */ true, startMarginPx, itemPerScreen);

        MarginLayoutParams marginLayoutParams = new MarginLayoutParams(100, 100);
        when(mView.getLayoutParams()).thenReturn(marginLayoutParams);
        startMarginPx = 10;
        mRecyclerView.setStartMarginPxForTesting(startMarginPx);
        int expectedWidth =
                (measuredWidth - mModuleInternalPaddingPx * (itemPerScreen - 1)) / itemPerScreen;

        // Verifies the width becomes the half of the parent's width.
        mRecyclerView.onDrawImplTablet(mView, 3, measuredWidth);
        assertEquals(expectedWidth, marginLayoutParams.width);
        assertEquals(startMarginPx, marginLayoutParams.getMarginStart());
        assertEquals(startMarginPx, marginLayoutParams.getMarginEnd());
        verify(mView).setLayoutParams(eq(marginLayoutParams));

        // Verifies that setLayoutParams() isn't called again whether there isn't any change to the
        // width of the view.
        mRecyclerView.onDrawImplTablet(mView, 3, measuredWidth);
        assertEquals(expectedWidth, marginLayoutParams.width);
        verify(mView).setLayoutParams(eq(marginLayoutParams));
    }

    @Test
    @SmallTest
    public void testGetMaxHeight() {
        int itemPerScreen = 1;
        int startMarginPx = 0;
        mRecyclerView.initialize(/* isTablet= */ false, startMarginPx, itemPerScreen);
        mRecyclerViewSpy = spy(mRecyclerView);
        int miniModuleHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.home_module_height);

        int childCount = 1;
        when(mRecyclerViewSpy.getChildCount()).thenReturn(childCount);
        when(mRecyclerViewSpy.getChildAt(eq(0))).thenReturn(mView);

        int height = miniModuleHeight - 10;
        when(mView.getHeight()).thenReturn(height);
        assertEquals(miniModuleHeight, mRecyclerViewSpy.getMaxHeight());

        height = miniModuleHeight * 2;
        when(mView.getHeight()).thenReturn(height);
        assertEquals(height, mRecyclerViewSpy.getMaxHeight());
    }

    @Test
    @SmallTest
    public void testUpdateHeight_OneChild() {
        int itemPerScreen = 1;
        int startMarginPx = 0;
        mRecyclerView.initialize(/* isTablet= */ false, startMarginPx, itemPerScreen);
        mRecyclerViewSpy = spy(mRecyclerView);
        int miniModuleHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.home_module_height);
        int childCount = 1;
        when(mRecyclerViewSpy.getChildCount()).thenReturn(childCount);
        when(mRecyclerViewSpy.getChildAt(eq(0))).thenReturn(mView);

        int height = miniModuleHeight * 2;
        when(mView.getHeight()).thenReturn(height);
        assertEquals(height, mRecyclerViewSpy.getMaxHeight());
        mRecyclerViewSpy.updateHeight(childCount);
        // Verifies requestLayout() isn't called if the child view is higher than the minimal
        // height.
        verify(mRecyclerViewSpy, never()).requestLayout();

        height = miniModuleHeight - 10;
        when(mView.getHeight()).thenReturn(height);
        assertEquals(miniModuleHeight, mRecyclerViewSpy.getMaxHeight());
        mRecyclerViewSpy.updateHeight(childCount);
        // Verifies requestLayout() is called to change the height of the child view to be the
        // minimal height.
        verify(mRecyclerViewSpy).requestLayout();
        verify(mView).setMinimumHeight(eq(miniModuleHeight));
    }

    @Test
    @SmallTest
    public void testUpdateHeight_TwoChildren() {
        int itemPerScreen = 1;
        int startMarginPx = 0;
        mRecyclerView.initialize(/* isTablet= */ false, startMarginPx, itemPerScreen);
        mRecyclerViewSpy = spy(mRecyclerView);
        int miniModuleHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.home_module_height);

        int height = miniModuleHeight * 2;
        int height1 = miniModuleHeight + 1;
        when(mView.getHeight()).thenReturn(height);
        when(mView1.getHeight()).thenReturn(height1);
        int childCount = 2;
        when(mRecyclerViewSpy.getChildCount()).thenReturn(childCount);
        when(mRecyclerViewSpy.getChildAt(eq(0))).thenReturn(mView);
        when(mRecyclerViewSpy.getChildAt(eq(1))).thenReturn(mView1);
        assertEquals(height, mRecyclerViewSpy.getMaxHeight());

        mRecyclerViewSpy.updateHeight(childCount);
        // Verifies that requestLayout() is called to adjust the minimal height of the child view
        // with lower height.
        verify(mView, never()).setMinimumHeight(eq(height));
        verify(mView1).setMinimumHeight(eq(height));
        verify(mRecyclerViewSpy).requestLayout();
    }
}
