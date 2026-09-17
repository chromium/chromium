// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.chrome.R.style.Theme_BrowserUI_DayNight;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.context_sharing.R;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Unit tests for {@link TabBottomSheetSkeletonView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabBottomSheetSkeletonViewUnitTest {
    private static final float EPSILON = 0.01f;

    private Context mContext;
    private TabBottomSheetSkeletonView mSkeletonView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(), Theme_BrowserUI_DayNight);
        mSkeletonView =
                (TabBottomSheetSkeletonView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.tab_bottom_sheet_skeleton_view, null);
    }

    @Test
    public void testInflationAndDefaultTitle() {
        FrameLayout headerContainer = mSkeletonView.getHeaderContainerForTesting();
        assertNotNull(headerContainer);
        assertNotNull(mSkeletonView.getBottomGroupForTesting());

        TextView titleView = headerContainer.findViewById(R.id.peek_title);
        assertNotNull(titleView);
        assertEquals(
                mContext.getString(R.string.tab_bottom_sheet_resizing_view_text),
                titleView.getText().toString());
    }

    @Test
    public void testParentClipChildrenToggling() {
        FrameLayout parent = new FrameLayout(mContext);
        parent.setClipChildren(true);
        parent.addView(mSkeletonView);

        mSkeletonView.onAttachedToWindow();
        assertFalse(parent.getClipChildren());

        mSkeletonView.onDetachedFromWindow();
        assertTrue(parent.getClipChildren());
    }

    @Test
    public void testSetBottomGroupAlpha() {
        ViewGroup bottomGroup = mSkeletonView.getBottomGroupForTesting();
        assertNotNull(bottomGroup);

        mSkeletonView.setBottomGroupAlpha(0.5f);
        assertEquals(0.5f, bottomGroup.getAlpha(), EPSILON);

        mSkeletonView.setBottomGroupAlpha(0.0f);
        assertEquals(0.0f, bottomGroup.getAlpha(), EPSILON);

        mSkeletonView.setBottomGroupAlpha(1.0f);
        assertEquals(1.0f, bottomGroup.getAlpha(), EPSILON);
    }

    @Test
    public void testGetHeaderAndBottomGroupHeights() {
        // Pre-layout: heights are 0.
        assertEquals(0, mSkeletonView.getHeaderHeight());
        assertEquals(0, mSkeletonView.getBottomGroupHeight());

        // Post-layout: return actual laid-out heights.
        FrameLayout headerContainer = mSkeletonView.getHeaderContainerForTesting();
        ViewGroup bottomGroup = mSkeletonView.getBottomGroupForTesting();
        assertNotNull(headerContainer);
        assertNotNull(bottomGroup);

        headerContainer.layout(0, 0, 400, 120);
        bottomGroup.layout(0, 120, 400, 420);

        assertEquals(120, mSkeletonView.getHeaderHeight());
        assertEquals(300, mSkeletonView.getBottomGroupHeight());
    }
}
