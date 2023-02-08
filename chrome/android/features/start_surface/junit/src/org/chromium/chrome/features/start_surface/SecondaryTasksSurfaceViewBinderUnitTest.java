// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SECONDARY_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link SecondaryTasksSurfaceViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
// After the refactoring, the SecondaryTasksSurface will go away.
@DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
@Config(manifest = Config.NONE)
public class SecondaryTasksSurfaceViewBinderUnitTest {
    private Activity mActivity;
    private ViewGroup mParentView;
    private View mTasksSurfaceView;
    private PropertyModel mPropertyModel;
    @SuppressWarnings({"FieldCanBeLocal", "unused"})
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() throws Exception {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        // Note that the specific type of the parent view and tasks surface view do not matter for
        // the SecondaryTasksSurfaceViewBinderTest.
        mParentView = new FrameLayout(mActivity);
        mTasksSurfaceView = new View(mActivity);
        mTasksSurfaceView.setBackground(new ColorDrawable(Color.WHITE));
        mActivity.setContentView(mParentView);

        mPropertyModel = new PropertyModel(StartSurfaceProperties.ALL_KEYS);
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(mPropertyModel,
                new StartSurfaceWithParentViewBinder.ViewHolder(
                        mParentView, mTasksSurfaceView, null),
                SecondaryTasksSurfaceViewBinder::bind);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetVisibility() {
        assertFalse(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        assertNull(mTasksSurfaceView.getParent());

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(View.VISIBLE, mTasksSurfaceView.getVisibility());

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, false);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(View.GONE, mTasksSurfaceView.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetVisibilityWithTopMargin() {
        assertFalse(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        assertNull(mTasksSurfaceView.getParent());
        mPropertyModel.set(TOP_MARGIN, 20);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(View.VISIBLE, mTasksSurfaceView.getVisibility());
        MarginLayoutParams layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals(20, layoutParams.topMargin);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, false);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(View.GONE, mTasksSurfaceView.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetTopMargin() {
        assertFalse(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        assertNull(mTasksSurfaceView.getParent());

        // Setting the top margin shouldn't cause a NullPointerException when the layout params are
        // null, since this should be handled in the *ViewBinder.
        mPropertyModel.set(TOP_MARGIN, 20);
        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);

        MarginLayoutParams layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals("Top margin isn't initialized correctly.", 20, layoutParams.topMargin);

        layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        mPropertyModel.set(TOP_MARGIN, 40);
        assertEquals("Wrong top margin.", 40, layoutParams.topMargin);
    }
}
