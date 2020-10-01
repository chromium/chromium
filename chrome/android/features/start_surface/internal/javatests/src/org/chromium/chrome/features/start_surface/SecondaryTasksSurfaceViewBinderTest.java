// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SECONDARY_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_STACK_TAB_SWITCHER;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/** Tests for {@link SecondaryTasksSurfaceViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SecondaryTasksSurfaceViewBinderTest extends DummyUiActivityTestCase {
    private ViewGroup mParentView;
    private ViewGroup mTasksSurfaceView;
    private View mTopToolbarPlaceholderView;
    private PropertyModel mPropertyModel;
    @SuppressWarnings({"FieldCanBeLocal", "unused"})
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Note that the specific type of the parent view and tasks surface view do not matter
            // for the SecondaryTasksSurfaceViewBinderTest.
            mParentView = new FrameLayout(getActivity());
            mTasksSurfaceView = new FrameLayout(getActivity());
            mTopToolbarPlaceholderView = new View(getActivity());
            mTasksSurfaceView.addView(mTopToolbarPlaceholderView);
            getActivity().setContentView(mParentView);
        });

        mPropertyModel = new PropertyModel(StartSurfaceProperties.ALL_KEYS);
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(mPropertyModel,
                new TasksSurfaceViewBinder.ViewHolder(
                        mParentView, mTasksSurfaceView, mTopToolbarPlaceholderView),
                SecondaryTasksSurfaceViewBinder::bind);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetVisibilityAfterShowingOverview() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        assertFalse(mPropertyModel.get(IS_SHOWING_STACK_TAB_SWITCHER));
        assertNull(mTasksSurfaceView.getParent());

        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        assertNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.VISIBLE);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, false);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, false);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetVisibilityBeforeShowingOverview() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        assertFalse(mPropertyModel.get(IS_SHOWING_STACK_TAB_SWITCHER));
        assertNull(mTasksSurfaceView.getParent());

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);
        assertNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.VISIBLE);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, false);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, false);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetVisibilityAfterShowingStackTabSwitcher() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        assertFalse(mPropertyModel.get(IS_SHOWING_STACK_TAB_SWITCHER));
        assertNull(mTasksSurfaceView.getParent());

        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        assertNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SHOWING_STACK_TAB_SWITCHER, true);
        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);
        assertNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, false);
        assertNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SHOWING_STACK_TAB_SWITCHER, false);
        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.VISIBLE);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, false);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetVisibilityWithTopMargin() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        assertFalse(mPropertyModel.get(IS_SHOWING_STACK_TAB_SWITCHER));
        assertNull(mTasksSurfaceView.getParent());
        mPropertyModel.set(TOP_MARGIN, 20);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        assertNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.VISIBLE);
        ViewGroup.LayoutParams layoutParams = mTopToolbarPlaceholderView.getLayoutParams();
        assertEquals(20, layoutParams.height);

        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, false);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, false);
        assertNotNull(mTasksSurfaceView.getParent());
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetTopMargin() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE));
        assertFalse(mPropertyModel.get(IS_SHOWING_STACK_TAB_SWITCHER));

        // Setting the top margin shouldn't cause a NullPointerException when the layout params are
        // null, since this should be handled in the *ViewBinder.
        mPropertyModel.set(TOP_MARGIN, 20);
        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, true);

        ViewGroup.LayoutParams layoutParams = mTopToolbarPlaceholderView.getLayoutParams();
        assertEquals("Top margin isn't initialized correctly.", 20, layoutParams.height);

        layoutParams = mTopToolbarPlaceholderView.getLayoutParams();
        mPropertyModel.set(TOP_MARGIN, 40);
        assertEquals("Wrong top margin.", 40, layoutParams.height);
    }
}
