// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.util.Size;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Junit Tests for {@link PinnedTabStripItemViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class PinnedTabStripItemViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PinnedTabStripItemView mPinnedTabStripItemView;
    @Mock private TabFavicon mTabFavicon;
    @Mock private Drawable mDrawable;
    @Mock private TabFaviconFetcher mFaviconFetcher;
    @Mock private TabActionListener mMockTabActionListener;

    private Activity mActivity;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.IS_INCOGNITO, false)
                        .with(TabProperties.IS_SELECTED, true)
                        .with(TabProperties.TAB_GROUP_CARD_COLOR, 1)
                        .build();
        when(mPinnedTabStripItemView.getContext()).thenReturn(mActivity);
    }

    @Test
    public void testBindFavicon() {
        when(mTabFavicon.getDefaultDrawable()).thenReturn(mDrawable);
        doAnswer(
                        (invocation) -> {
                            Callback<TabFavicon> callback = invocation.getArgument(0);
                            callback.onResult(mTabFavicon);
                            return null;
                        })
                .when(mFaviconFetcher)
                .fetch(any());

        mModel.set(TabProperties.FAVICON_FETCHER, mFaviconFetcher);
        PinnedTabStripItemViewBinder.bind(
                mModel, mPinnedTabStripItemView, TabProperties.FAVICON_FETCHER);
        verify(mPinnedTabStripItemView).setFaviconIcon(any(), anyBoolean());
    }

    @Test
    public void testBindTitle() {
        final String title = "test";
        mModel.set(TabProperties.TITLE, title);
        PinnedTabStripItemViewBinder.bind(mModel, mPinnedTabStripItemView, TabProperties.TITLE);
        verify(mPinnedTabStripItemView).setTitle(eq(title));
    }

    @Test
    public void testBindGridCardSize() {
        final Size size = new Size(1, 1);
        mModel.set(TabProperties.GRID_CARD_SIZE, size);
        PinnedTabStripItemViewBinder.bind(
                mModel, mPinnedTabStripItemView, TabProperties.GRID_CARD_SIZE);
        verify(mPinnedTabStripItemView).setGridCardSize(eq(size));
    }

    @Test
    public void testBindIsSelected() {
        mModel.set(TabProperties.IS_SELECTED, true);
        PinnedTabStripItemViewBinder.bind(
                mModel, mPinnedTabStripItemView, TabProperties.IS_SELECTED);
        verify(mPinnedTabStripItemView).setSelected(eq(true), eq(false));
    }

    @Test
    public void testBindClickListener() {
        mModel.set(TabProperties.TAB_ID, 1);
        mModel.set(TabProperties.TAB_CLICK_LISTENER, mMockTabActionListener);
        PinnedTabStripItemViewBinder.bind(
                mModel, mPinnedTabStripItemView, TabProperties.TAB_CLICK_LISTENER);
        ArgumentCaptor<View.OnClickListener> captor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        verify(mPinnedTabStripItemView).setOnClickListener(captor.capture());
        captor.getValue().onClick(mPinnedTabStripItemView);
        verify(mMockTabActionListener).run(eq(mPinnedTabStripItemView), eq(1), any());
    }

    @Test
    public void testBindContextClickListener() {
        mModel.set(TabProperties.TAB_ID, 1);
        mModel.set(TabProperties.TAB_CONTEXT_CLICK_LISTENER, mMockTabActionListener);
        PinnedTabStripItemViewBinder.bind(
                mModel, mPinnedTabStripItemView, TabProperties.TAB_CONTEXT_CLICK_LISTENER);
        verify(mPinnedTabStripItemView)
                .setNullableContextClickListener(
                        eq(mMockTabActionListener), eq(mPinnedTabStripItemView), eq(1));
    }
}
