// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_CLICKLISTENER;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_SELECTED_TAB_POSITION;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_BOTTOM_BAR_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import com.google.android.material.tabs.TabLayout;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.start_surface.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link BottomBarViewBinder}. */
@SuppressWarnings("ConstantConditions")
@RunWith(ChromeJUnit4ClassRunner.class)
public class BottomBarViewBinderTest extends DummyUiActivityTestCase {
    @SuppressWarnings("unused")
    private BottomBarCoordinator mBottomBarCoordinator;
    private TabLayout mTabLayout;
    private ViewGroup mParentView;
    private BottomBarView mBottomBarView;
    private PropertyModel mPropertyModel;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mParentView = new FrameLayout(getActivity());
            getActivity().setContentView(mParentView);
            mPropertyModel = new PropertyModel(StartSurfaceProperties.ALL_KEYS);
            mBottomBarCoordinator =
                    new BottomBarCoordinator(getActivity(), mParentView, mPropertyModel);
        });

        mBottomBarView = mParentView.findViewById(R.id.ss_bottom_bar);
        mTabLayout = mBottomBarView.findViewById(R.id.bottom_tab_layout);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetBottomBarVisibility() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_BOTTOM_BAR_VISIBLE));
        assertNotEquals(mBottomBarView.getVisibility(), View.VISIBLE);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        assertNotEquals(mBottomBarView.getVisibility(), View.VISIBLE);

        mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, true);
        assertEquals(mBottomBarView.getVisibility(), View.VISIBLE);

        mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, false);
        assertNotEquals(mBottomBarView.getVisibility(), View.VISIBLE);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, false);
        assertNotEquals(mBottomBarView.getVisibility(), View.VISIBLE);

        mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, true);
        assertNotEquals(mBottomBarView.getVisibility(), View.VISIBLE);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        assertEquals(mBottomBarView.getVisibility(), View.VISIBLE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetBottomBarClickListener() {
        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, true);

        AtomicBoolean homeButtonClicked = new AtomicBoolean();
        AtomicBoolean exploreButtonClicked = new AtomicBoolean();
        StartSurfaceProperties.BottomBarClickListener clickListener =
                new StartSurfaceProperties.BottomBarClickListener() {
                    @Override
                    public void onHomeButtonClicked() {
                        homeButtonClicked.set(true);
                    }
                    @Override
                    public void onExploreButtonClicked() {
                        exploreButtonClicked.set(true);
                    }
                };
        mPropertyModel.set(BOTTOM_BAR_CLICKLISTENER, clickListener);

        TabLayout.Tab homeTab = mTabLayout.getTabAt(0);
        TabLayout.Tab exploreTab = mTabLayout.getTabAt(1);
        assertTrue(homeTab.isSelected());

        exploreButtonClicked.set(false);
        exploreTab.select();
        assertTrue(exploreButtonClicked.get());
        homeButtonClicked.set(false);
        homeTab.select();
        assertTrue(homeButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetSelectedTabPosition() {
        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, true);

        assertEquals(mTabLayout.getSelectedTabPosition(), 0);
        mPropertyModel.set(BOTTOM_BAR_SELECTED_TAB_POSITION, 1);
        assertEquals(mTabLayout.getSelectedTabPosition(), 1);
        mPropertyModel.set(BOTTOM_BAR_SELECTED_TAB_POSITION, 0);
        assertEquals(mTabLayout.getSelectedTabPosition(), 0);
    }
}
