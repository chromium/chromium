// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link PinnedTabStripViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PinnedTabStripViewBinderTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PinnedTabStripAnimationManager mAnimationManager;

    private RecyclerView mRecyclerView;
    private LinearLayoutManager mLayoutManager;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        mLayoutManager = spy(new LinearLayoutManager(context));
        mRecyclerView = new RecyclerView(context);
        mRecyclerView.setLayoutManager(mLayoutManager);

        mPropertyModel =
                new PropertyModel.Builder(PinnedTabStripProperties.ALL_KEYS)
                        .with(PinnedTabStripProperties.IS_VISIBLE, false)
                        .with(PinnedTabStripProperties.SCROLL_TO_POSITION, -1)
                        .with(PinnedTabStripProperties.ANIMATION_MANAGER, mAnimationManager)
                        .with(
                                PinnedTabStripProperties.IS_VISIBILITY_ANIMATION_RUNNING_SUPPLIER,
                                ObservableSuppliers.createNonNull(false))
                        .build();

        PropertyModelChangeProcessor.create(
                mPropertyModel, mRecyclerView, PinnedTabStripViewBinder::bind);
    }

    @Test
    public void testSetIsVisible_becomesVisible() {
        mPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, true);
        verify(mAnimationManager).animatePinnedTabBarVisibility(eq(true), any());
    }

    @Test
    public void testSetIsVisible_becomesHidden() {
        mPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, false);
        verify(mAnimationManager).animatePinnedTabBarVisibility(eq(false), any());
    }

    @Test
    public void testSetScrollToPosition() {
        mPropertyModel.set(PinnedTabStripProperties.SCROLL_TO_POSITION, 5);
        verify(mAnimationManager).cancelPinnedTabBarAnimations(any());
        verify(mLayoutManager).scrollToPosition(5);
    }

    @Test
    public void testSetScrollToPosition_invalidPosition() {
        mPropertyModel.set(PinnedTabStripProperties.SCROLL_TO_POSITION, -1);
        verify(mLayoutManager, never()).scrollToPosition(anyInt());
    }

    @Test
    public void testSetBackgroundColor() {
        mPropertyModel.set(PinnedTabStripProperties.BACKGROUND_COLOR, Color.RED);
        assertEquals(Color.RED, ((ColorDrawable) mRecyclerView.getBackground()).getColor());
    }
}
