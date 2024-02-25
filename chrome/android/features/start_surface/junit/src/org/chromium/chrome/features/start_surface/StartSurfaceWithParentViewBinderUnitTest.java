// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link StartSurfaceWithParentViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StartSurfaceWithParentViewBinderUnitTest {
    private Activity mActivity;
    private ViewGroup mParentView;
    private ViewGroup mTasksSurfaceView;
    private ViewGroup mFeedSwipeRefreshLayout;
    private PropertyModel mPropertyModel;

    @SuppressWarnings({"FieldCanBeLocal", "unused"})
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        // Note that the specific type of the parent view and tasks surface view do not matter for
        // the TasksSurfaceViewBinder.
        mParentView = new FrameLayout(mActivity);
        mTasksSurfaceView = new FrameLayout(mActivity);
        mFeedSwipeRefreshLayout = new FrameLayout(mActivity);
        mActivity.setContentView(mParentView);

        mPropertyModel = new PropertyModel(StartSurfaceProperties.ALL_KEYS);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel,
                        new StartSurfaceWithParentViewBinder.ViewHolder(
                                mParentView, mTasksSurfaceView, mFeedSwipeRefreshLayout),
                        StartSurfaceWithParentViewBinder::bind);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetShowAndHideOverview() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertNull(mTasksSurfaceView.getParent());

        mPropertyModel.set(BOTTOM_BAR_HEIGHT, 10);
        mPropertyModel.set(TOP_MARGIN, 20);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        assertEquals(View.VISIBLE, mTasksSurfaceView.getVisibility());
        assertEquals(View.VISIBLE, mFeedSwipeRefreshLayout.getVisibility());
        assertNotNull(mTasksSurfaceView.getParent());
        MarginLayoutParams layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals(10, layoutParams.bottomMargin);
        assertEquals(20, layoutParams.topMargin);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, false);
        assertEquals(View.GONE, mTasksSurfaceView.getVisibility());
        assertEquals(View.GONE, mFeedSwipeRefreshLayout.getVisibility());
        assertNotNull(mTasksSurfaceView.getParent());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetBottomBarHeight() {
        mPropertyModel.set(BOTTOM_BAR_HEIGHT, 10);
        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        MarginLayoutParams layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals(10, layoutParams.bottomMargin);

        mPropertyModel.set(BOTTOM_BAR_HEIGHT, 20);
        layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals(20, layoutParams.bottomMargin);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetTopBarHeight() {
        mPropertyModel.set(TOP_MARGIN, 10);
        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        MarginLayoutParams layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals("Top margin isn't initialized correctly.", 10, layoutParams.topMargin);

        mPropertyModel.set(TOP_MARGIN, 20);
        layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals("Wrong top margin.", 20, layoutParams.topMargin);
    }
}
