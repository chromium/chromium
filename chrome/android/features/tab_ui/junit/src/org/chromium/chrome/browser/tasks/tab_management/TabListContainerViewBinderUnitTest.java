// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Robolectric tests for {@link TabListContainerViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
public class TabListContainerViewBinderUnitTest {
    private static final int TAB_MODEL_FILTER_INDEX = 2;

    private class MockViewHolder extends RecyclerView.ViewHolder {
        public MockViewHolder(@NonNull View itemView) {
            super(itemView);
        }
    }

    @Mock private PropertyModel mPropertyModelMock;
    @Mock private TabListRecyclerView mTabListRecyclerViewMock;
    @Mock private View mViewMock;

    private MockViewHolder mMockViewHolder;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    public void testFocusTabIndexForAccessibilityProperty() {
        mMockViewHolder = spy(new MockViewHolder(mViewMock));
        doReturn(mMockViewHolder)
                .when(mTabListRecyclerViewMock)
                .findViewHolderForAdapterPosition(eq(TAB_MODEL_FILTER_INDEX));

        doReturn(TAB_MODEL_FILTER_INDEX)
                .when(mPropertyModelMock)
                .get(TabListContainerProperties.FOCUS_TAB_INDEX_FOR_ACCESSIBILITY);
        TabListContainerViewBinder.bind(
                mPropertyModelMock,
                mTabListRecyclerViewMock,
                TabListContainerProperties.FOCUS_TAB_INDEX_FOR_ACCESSIBILITY);
        verify(mViewMock).requestFocus();
        verify(mViewMock).sendAccessibilityEvent(eq(AccessibilityEvent.TYPE_VIEW_FOCUSED));
    }
}
