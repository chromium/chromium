// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.View.MeasureSpec;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.R;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_DYNAMIC_POSITIONING)
public class NoSwipeViewPagerTest {
    private Activity mActivity;
    private NoSwipeViewPager mViewPager;
    private AccessoryPagerAdapter mAdapter;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mViewPager = new NoSwipeViewPager(mActivity, null);
        mAdapter = mock(AccessoryPagerAdapter.class);
        mViewPager.setAdapter(mAdapter);
    }

    @Test
    public void testMeasureWithExactHeight() {
        int widthMeasureSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        int heightMeasureSpec = MeasureSpec.makeMeasureSpec(200, MeasureSpec.EXACTLY);

        mViewPager.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals(200, mViewPager.getMeasuredHeight());
    }

    @Test
    public void testMeasureWithUnspecifiedHeightCalculatesChildHeight() {
        int widthMeasureSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        View child =
                new View(mActivity) {
                    @Override
                    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                        setMeasuredDimension(100, 150);
                    }
                };

        when(mAdapter.getCount()).thenReturn(1);
        when(mAdapter.getView(0)).thenReturn(child);

        mViewPager.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals(150, mViewPager.getMeasuredHeight());
    }

    @Test
    public void testMeasureRespectsMaxHeight() {
        int widthMeasureSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        int maxHeight =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.keyboard_accessory_sheet_dynamic_positioning_max_height);

        // Child wants to be larger than max height
        View child =
                new View(mActivity) {
                    @Override
                    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                        setMeasuredDimension(100, maxHeight + 100);
                    }
                };

        when(mAdapter.getCount()).thenReturn(1);
        when(mAdapter.getView(0)).thenReturn(child);

        mViewPager.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals(maxHeight, mViewPager.getMeasuredHeight());
    }

    @Test
    @Features.DisableFeatures(
            ChromeFeatureList.AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_DYNAMIC_POSITIONING)
    public void testMeasureDisabledFeatureFallsBackToSuper() {
        // When feature is disabled, it shouldn't measure child.

        int widthMeasureSpec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        View child =
                new View(mActivity) {
                    @Override
                    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                        setMeasuredDimension(100, 150);
                    }
                };

        when(mAdapter.getCount()).thenReturn(1);
        when(mAdapter.getView(0)).thenReturn(child);

        mViewPager.measure(widthMeasureSpec, heightMeasureSpec);

        // It should NOT be 150.
        assertTrue(mViewPager.getMeasuredHeight() != 150);
    }
}
