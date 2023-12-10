// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import static org.mockito.Mockito.doReturn;

import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.ChromeRobolectricTestRunner;
import org.chromium.chrome.browser.history_clusters.HistoryClustersViewBinder.VerticallyCenterItemDecoration;

/** Unit tests for HistoryClustersViewBinder. */
@RunWith(ChromeRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistoryClustersViewBinderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private View mView;
    @Mock private View mViewAbove;
    @Mock private RecyclerView mParent;
    @Mock private RecyclerView.State mState;
    @Mock private RecyclerView.LayoutManager mLayoutManager;

    HistoryClustersViewBinder.VerticallyCenterItemDecoration mItemDecoration;

    @Before
    public void setUp() {
        mItemDecoration = new VerticallyCenterItemDecoration();
    }

    @Test
    public void testGetOffsets() {
        mItemDecoration.setViewToCenter(mView);

        int viewHeight = 50;
        int parentHeight = 200;
        doReturn(viewHeight).when(mView).getHeight();
        doReturn(parentHeight).when(mParent).getHeight();

        Rect outRect = new Rect();
        mItemDecoration.getItemOffsets(outRect, mView, mParent, mState);
        Assert.assertEquals(parentHeight / 2 - viewHeight / 2, outRect.top);

        doReturn(3).when(mState).getItemCount();
        doReturn(mLayoutManager).when(mParent).getLayoutManager();
        doReturn(50).when(mLayoutManager).getDecoratedBottom(mViewAbove);
        doReturn(mViewAbove).when(mParent).getChildAt(0);
        doReturn(1).when(mLayoutManager).getPosition(mView);

        outRect = new Rect();
        mItemDecoration.getItemOffsets(outRect, mView, mParent, mState);
        Assert.assertEquals((parentHeight - 50) / 2 - viewHeight / 2, outRect.top);
    }

    @Test
    public void testIgnore() {
        Rect outRect = new Rect();
        mItemDecoration.getItemOffsets(outRect, mView, mParent, mState);

        Assert.assertEquals(outRect, new Rect());
    }
}
