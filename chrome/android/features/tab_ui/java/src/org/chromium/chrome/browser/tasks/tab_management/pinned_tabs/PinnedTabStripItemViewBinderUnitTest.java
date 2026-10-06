// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab_ui.TabCardThemeUtil;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Junit Tests for {@link PinnedTabStripItemViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class PinnedTabStripItemViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabFavicon mTabFavicon;
    @Mock private TabFaviconFetcher mFaviconFetcher;
    @Mock private TabActionListener mMockTabActionListener;

    private final Drawable mDrawable = new ColorDrawable(Color.RED);
    private Activity mActivity;
    private PinnedTabStripItemView mPinnedTabStripItemView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.IS_INCOGNITO, false)
                        .with(TabProperties.IS_SELECTED, true)
                        .with(TabProperties.TAB_GROUP_CARD_COLOR, 1)
                        .build();
        FrameLayout parent = new FrameLayout(mActivity);
        mPinnedTabStripItemView =
                (PinnedTabStripItemView)
                        LayoutInflater.from(mActivity)
                                .inflate(
                                        R.layout.pinned_tab_strip_item,
                                        parent,
                                        /* attachToRoot= */ false);
    }

    @Test
    public void testBindFavicon() {
        // The model has IS_SELECTED set to true.
        when(mTabFavicon.getSelectedDrawable()).thenReturn(mDrawable);
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
        ImageView favicon = mPinnedTabStripItemView.findViewById(R.id.tab_favicon);
        assertEquals(View.VISIBLE, favicon.getVisibility());
        assertEquals(mDrawable, favicon.getDrawable());
    }

    @Test
    public void testBindTitle() {
        final String title = "test";
        mModel.set(TabProperties.TITLE, title);
        PinnedTabStripItemViewBinder.bind(mModel, mPinnedTabStripItemView, TabProperties.TITLE);
        TextView titleView = mPinnedTabStripItemView.findViewById(R.id.tab_title);
        assertEquals(title, titleView.getText().toString());
    }

    @Test
    public void testBindPinnedStripItemSize() {
        final Size size = new Size(/* width= */ 30, /* height= */ 20);
        mModel.set(TabProperties.PINNED_STRIP_ITEM_SIZE, size);
        PinnedTabStripItemViewBinder.bind(
                mModel, mPinnedTabStripItemView, TabProperties.PINNED_STRIP_ITEM_SIZE);
        ViewGroup.LayoutParams layoutParams = mPinnedTabStripItemView.getLayoutParams();
        assertEquals(30, layoutParams.width);
        assertEquals(20, layoutParams.height);
    }

    @Test
    public void testBindIsSelected() {
        TextView titleView = mPinnedTabStripItemView.findViewById(R.id.tab_title);
        int unselectedColor =
                TabCardThemeUtil.getTitleTextColor(
                        mActivity,
                        /* isIncognito= */ false,
                        /* isSelected= */ false,
                        /* colorId= */ null);
        int selectedColor =
                TabCardThemeUtil.getTitleTextColor(
                        mActivity,
                        /* isIncognito= */ false,
                        /* isSelected= */ true,
                        /* colorId= */ null);
        assertNotEquals(unselectedColor, selectedColor);

        mModel.set(TabProperties.IS_SELECTED, false);
        PinnedTabStripItemViewBinder.bind(
                mModel, mPinnedTabStripItemView, TabProperties.IS_SELECTED);
        assertEquals(unselectedColor, titleView.getCurrentTextColor());

        mModel.set(TabProperties.IS_SELECTED, true);
        PinnedTabStripItemViewBinder.bind(
                mModel, mPinnedTabStripItemView, TabProperties.IS_SELECTED);
        assertEquals(selectedColor, titleView.getCurrentTextColor());
    }

    @Test
    public void testBindClickListener() {
        mModel.set(TabProperties.TAB_ID, 1);
        mModel.set(TabProperties.TAB_CLICK_LISTENER, mMockTabActionListener);
        PinnedTabStripItemViewBinder.bind(
                mModel, mPinnedTabStripItemView, TabProperties.TAB_CLICK_LISTENER);
        assertTrue(mPinnedTabStripItemView.performClick());
        verify(mMockTabActionListener).run(eq(mPinnedTabStripItemView), eq(1), any());
    }

    @Test
    public void testBindContextClickListener() {
        mModel.set(TabProperties.TAB_ID, 1);
        mModel.set(TabProperties.TAB_CONTEXT_CLICK_LISTENER, mMockTabActionListener);
        PinnedTabStripItemViewBinder.bind(
                mModel, mPinnedTabStripItemView, TabProperties.TAB_CONTEXT_CLICK_LISTENER);
        assertTrue(mPinnedTabStripItemView.isContextClickable());
        assertTrue(mPinnedTabStripItemView.performContextClick());
        verify(mMockTabActionListener).run(eq(mPinnedTabStripItemView), eq(1), isNull());
    }
}
