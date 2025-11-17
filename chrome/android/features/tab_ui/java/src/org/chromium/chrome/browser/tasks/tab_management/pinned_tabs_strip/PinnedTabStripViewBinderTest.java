// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;
import android.view.ViewPropertyAnimator;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link PinnedTabStripViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PinnedTabStripViewBinderTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private RecyclerView mRecyclerView;
    @Mock private ViewPropertyAnimator mViewPropertyAnimator;
    @Mock private PinnedTabStripAnimationManager mAnimationManager;

    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        when(mRecyclerView.animate()).thenReturn(mViewPropertyAnimator);
        when(mViewPropertyAnimator.alpha(any(Float.class))).thenReturn(mViewPropertyAnimator);
        when(mViewPropertyAnimator.translationY(any(Float.class)))
                .thenReturn(mViewPropertyAnimator);
        when(mViewPropertyAnimator.withEndAction(any(Runnable.class)))
                .thenReturn(mViewPropertyAnimator);
        when(mViewPropertyAnimator.withStartAction(any(Runnable.class)))
                .thenReturn(mViewPropertyAnimator);

        mPropertyModel =
                new PropertyModel.Builder(PinnedTabStripProperties.ALL_KEYS)
                        .with(PinnedTabStripProperties.IS_VISIBLE, false)
                        .with(PinnedTabStripProperties.SCROLL_TO_POSITION, -1)
                        .with(PinnedTabStripProperties.ANIMATION_MANAGER, mAnimationManager)
                        .with(
                                PinnedTabStripProperties.IS_VISIBILITY_ANIMATION_RUNNING_SUPPLIER,
                                new ObservableSupplierImpl<>())
                        .build();

        PropertyModelChangeProcessor.create(
                mPropertyModel, mRecyclerView, PinnedTabStripViewBinder::bind);
    }

    @Test
    public void testSetIsVisible_becomesVisible() {
        mPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, true);
        verify(mAnimationManager).animateShowPinnedTabBar(any());
    }

    @Test
    public void testSetIsVisible_becomesHidden() {
        mPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, false);
        verify(mAnimationManager).animateHidePinnedTabBar(any());
    }

    @Test
    public void testSetScrollToPosition() {
        mPropertyModel.set(PinnedTabStripProperties.SCROLL_TO_POSITION, 5);
        verify(mAnimationManager).cancelPinnedTabBarAnimations(any());
        verify(mRecyclerView).scrollToPosition(5);
    }

    @Test
    public void testSetScrollToPosition_invalidPosition() {
        mPropertyModel.set(PinnedTabStripProperties.SCROLL_TO_POSITION, -1);
        verify(mRecyclerView, never()).smoothScrollToPosition(-1);
    }

    @Test
    @SmallTest
    public void testSetBackgroundColor() {
        mPropertyModel.set(PinnedTabStripProperties.BACKGROUND_COLOR, Color.RED);
        verify(mRecyclerView).setBackgroundColor(Color.RED);
    }
}
