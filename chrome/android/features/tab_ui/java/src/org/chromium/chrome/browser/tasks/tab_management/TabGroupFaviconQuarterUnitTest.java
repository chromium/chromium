// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.UNSET;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TabGroupFaviconQuarter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupFaviconQuarterUnitTest {
    private static final int PLUS_COUNT = 123;
    private static final int PARENT_ID = 234;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Drawable mDrawable;

    private Activity mActivity;
    private TabGroupFaviconQuarter mTabGroupFaviconQuarter;
    private GradientDrawable mBackground;
    private ImageView mImageView;
    private TextView mTextView;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        ConstraintLayout parent = new ConstraintLayout(mActivity, null);
        LayoutInflater inflater = LayoutInflater.from(mActivity);
        inflater.inflate(R.layout.tab_group_favicon_quarter, parent, true);
        mTabGroupFaviconQuarter = (TabGroupFaviconQuarter) parent.getChildAt(0);
        mBackground = (GradientDrawable) mTabGroupFaviconQuarter.getBackground();
        mImageView = mTabGroupFaviconQuarter.findViewById(R.id.favicon_image);
        mTextView = mTabGroupFaviconQuarter.findViewById(R.id.hidden_tab_count);
    }

    @Test
    public void testSetCorner() {
        ConstraintLayout.LayoutParams params;

        mTabGroupFaviconQuarter.adjustPositionForCorner(Corner.TOP_LEFT, PARENT_ID);
        params = (ConstraintLayout.LayoutParams) mTabGroupFaviconQuarter.getLayoutParams();
        assertEquals(PARENT_ID, params.leftToLeft);
        assertEquals(PARENT_ID, params.topToTop);
        assertEquals(UNSET, params.rightToRight);
        assertEquals(UNSET, params.bottomToBottom);

        mTabGroupFaviconQuarter.adjustPositionForCorner(Corner.TOP_RIGHT, PARENT_ID);
        params = (ConstraintLayout.LayoutParams) mTabGroupFaviconQuarter.getLayoutParams();
        assertEquals(UNSET, params.leftToLeft);
        assertEquals(PARENT_ID, params.topToTop);
        assertEquals(PARENT_ID, params.rightToRight);
        assertEquals(UNSET, params.bottomToBottom);

        mTabGroupFaviconQuarter.adjustPositionForCorner(Corner.BOTTOM_RIGHT, PARENT_ID);
        params = (ConstraintLayout.LayoutParams) mTabGroupFaviconQuarter.getLayoutParams();
        assertEquals(UNSET, params.leftToLeft);
        assertEquals(UNSET, params.topToTop);
        assertEquals(PARENT_ID, params.rightToRight);
        assertEquals(PARENT_ID, params.bottomToBottom);

        mTabGroupFaviconQuarter.adjustPositionForCorner(Corner.BOTTOM_LEFT, PARENT_ID);
        params = (ConstraintLayout.LayoutParams) mTabGroupFaviconQuarter.getLayoutParams();
        assertEquals(PARENT_ID, params.leftToLeft);
        assertEquals(UNSET, params.topToTop);
        assertEquals(UNSET, params.rightToRight);
        assertEquals(PARENT_ID, params.bottomToBottom);
    }

    @Test
    public void testSetImage() {
        mTabGroupFaviconQuarter.setImage(mDrawable);
        assertEquals(View.VISIBLE, mImageView.getVisibility());
        assertEquals(mDrawable, mImageView.getDrawable());
        assertEquals(View.INVISIBLE, mTextView.getVisibility());
        assertTrue(TextUtils.isEmpty(mTextView.getText()));
        assertEquals(filterColor(mBackground), SemanticColorUtils.getColorSurfaceBright(mActivity));
    }

    @Test
    public void testSetPlusCount() {
        mTabGroupFaviconQuarter.setPlusCount(PLUS_COUNT);
        assertEquals(View.INVISIBLE, mImageView.getVisibility());
        assertNull(mImageView.getDrawable());
        assertEquals(View.VISIBLE, mTextView.getVisibility());
        assertEquals("+123", mTextView.getText());
        assertEquals(
                filterColor(mBackground),
                SemanticColorUtils.getColorSurfaceContainerLow(mActivity));
    }

    @Test
    @Config(qualifiers = "night")
    public void testSetPlusCount_Night() {
        mTabGroupFaviconQuarter.setPlusCount(PLUS_COUNT);
        assertEquals(
                filterColor(mBackground),
                SemanticColorUtils.getColorSurfaceContainerHigh(mActivity));
    }

    @Test
    public void testClear() {
        mTabGroupFaviconQuarter.clear();
        assertEquals(View.INVISIBLE, mImageView.getVisibility());
        assertNull(mImageView.getDrawable());
        assertEquals(View.INVISIBLE, mTextView.getVisibility());
        assertTrue(TextUtils.isEmpty(mTextView.getText()));
        assertEquals(
                filterColor(mBackground),
                SemanticColorUtils.getColorSurfaceContainerLow(mActivity));
    }

    @Test
    @Config(qualifiers = "night")
    public void testClear_Night() {
        mTabGroupFaviconQuarter.clear();
        assertEquals(
                filterColor(mBackground),
                SemanticColorUtils.getColorSurfaceContainerHigh(mActivity));
    }

    @Test
    public void testContainmentColor() {
        mTabGroupFaviconQuarter.setContainmentEnabled(true);
        mTabGroupFaviconQuarter.setImage(mDrawable);
        assertEquals(
                filterColor(mBackground), SemanticColorUtils.getColorSurfaceContainer(mActivity));
    }

    @Test
    public void testContainmentColorWithPlusCount() {
        mTabGroupFaviconQuarter.setContainmentEnabled(true);
        mTabGroupFaviconQuarter.setPlusCount(PLUS_COUNT);
        assertEquals(
                filterColor(mBackground),
                SemanticColorUtils.getColorSurfaceContainerLow(mActivity));
    }

    private static @ColorInt int filterColor(Drawable drawable) {
        PorterDuffColorFilter filter = (PorterDuffColorFilter) drawable.getColorFilter();
        return Shadows.shadowOf(filter).getColor();
    }
}
