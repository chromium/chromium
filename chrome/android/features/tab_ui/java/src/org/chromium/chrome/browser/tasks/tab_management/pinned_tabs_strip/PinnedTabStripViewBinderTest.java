// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewPropertyAnimator;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link PinnedTabStripViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PinnedTabStripViewBinderTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private RecyclerView mRecyclerView;
    @Mock private ViewPropertyAnimator mViewPropertyAnimator;

    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        when(mRecyclerView.animate()).thenReturn(mViewPropertyAnimator);
        when(mViewPropertyAnimator.alpha(any(Float.class))).thenReturn(mViewPropertyAnimator);
        when(mViewPropertyAnimator.translationY(any(Float.class)))
                .thenReturn(mViewPropertyAnimator);
        when(mViewPropertyAnimator.withEndAction(any(Runnable.class)))
                .thenReturn(mViewPropertyAnimator);

        mPropertyModel =
                new PropertyModel.Builder(PinnedTabStripProperties.ALL_KEYS)
                        .with(PinnedTabStripProperties.IS_VISIBLE, false)
                        .with(PinnedTabStripProperties.SCROLL_TO_POSITION, -1)
                        .build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mRecyclerView, PinnedTabStripViewBinder::bind);
    }

    @Test
    public void testSetIsVisible_becomesVisible() {
        when(mRecyclerView.getVisibility()).thenReturn(View.GONE);
        mPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, true);
        verify(mRecyclerView).post(any(Runnable.class));
    }

    @Test
    public void testSetIsVisible_becomesHidden() {
        when(mRecyclerView.getVisibility()).thenReturn(View.VISIBLE);
        mPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, false);
        verify(mViewPropertyAnimator).withEndAction(any(Runnable.class));
    }

    @Test
    public void testSetIsVisible_staysVisible() {
        when(mRecyclerView.getVisibility()).thenReturn(View.VISIBLE);
        mPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, true);
        verify(mViewPropertyAnimator).withEndAction(null);
    }

    @Test
    public void testSetScrollToPosition() {
        mPropertyModel.set(PinnedTabStripProperties.SCROLL_TO_POSITION, 5);
        verify(mRecyclerView).scrollToPosition(5);
    }

    @Test
    public void testSetScrollToPosition_invalidPosition() {
        mPropertyModel.set(PinnedTabStripProperties.SCROLL_TO_POSITION, -1);
        verify(mRecyclerView, never()).smoothScrollToPosition(-1);
    }
}
