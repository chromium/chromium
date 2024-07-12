// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomeModulesRecyclerViewUnitTest {
    @Mock private View mView;

    private Activity mActivity;
    private HomeModulesRecyclerView mRecyclerView;
    private int mModuleInternalPaddingPx;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mRecyclerView =
                (HomeModulesRecyclerView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.home_modules_recycler_view_layout, null);
        mActivity.setContentView(mRecyclerView);

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
    public void testOnDraw_MultipleItermsPerScreen() {
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
}
