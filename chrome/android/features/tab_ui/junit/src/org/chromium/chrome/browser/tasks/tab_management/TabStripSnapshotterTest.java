// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.ResourceTabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.StaticTabFaviconType;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.UrlTabFavicon;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabStripSnapshotter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabStripSnapshotterTest {
    private static final PropertyKey[] PROPERTY_KEYS =
            new PropertyKey[] {
                TabProperties.FAVICON_FETCHER,
                TabProperties.FAVICON_FETCHED,
                TabProperties.IS_SELECTED
            };

    @Captor private ArgumentCaptor<OnScrollListener> mOnScrollListenerCaptor;

    @Mock private RecyclerView mRecyclerView;

    @Mock private TabFaviconFetcher mTabFaviconFetcherA;
    @Mock private TabFaviconFetcher mTabFaviconFetcherB;
    @Mock private TabFaviconFetcher mTabFaviconFetcherC;

    private final List<Object> mTokenList = new ArrayList<>();

    private static Drawable newDrawable() {
        Bitmap image = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        Resources resources = ContextUtils.getApplicationContext().getResources();
        return new BitmapDrawable(resources, image);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    private void onModelTokenChange(Object token) {
        mTokenList.add(token);
    }

    private static PropertyModel makePropertyModel(TabFavicon tabFavicon, boolean isSelected) {
        return new PropertyModel.Builder(PROPERTY_KEYS)
                .with(TabProperties.IS_SELECTED, isSelected)
                .build();
    }

    private static PropertyModel makePropertyModel(
            TabFaviconFetcher fetcher, boolean isSelected, boolean isFetched) {
        return new PropertyModel.Builder(PROPERTY_KEYS)
                .with(TabProperties.FAVICON_FETCHER, fetcher)
                .with(TabProperties.FAVICON_FETCHED, isFetched)
                .with(TabProperties.IS_SELECTED, isSelected)
                .build();
    }

    private static PropertyModel makePropertyModel(String url, boolean isSelected) {
        return makePropertyModel(makeTabFavicon(url), isSelected);
    }

    private static TabFavicon makeTabFavicon(String url) {
        GURL gurl = new GURL(url);
        return new UrlTabFavicon(newDrawable(), gurl);
    }

    private static PropertyModel makePropertyModel(
            @StaticTabFaviconType int type, boolean isSelected) {
        ResourceTabFavicon tabFavicon = new ResourceTabFavicon(newDrawable(), type);
        return makePropertyModel(tabFavicon, isSelected);
    }

    @Test
    public void testSnapshotterFetcher() {
        Mockito.when(mRecyclerView.computeHorizontalScrollOffset()).thenReturn(0);
        ModelList modelList = new ModelList();
        PropertyModel propertyModel1 = makePropertyModel(mTabFaviconFetcherA, false, false);
        modelList.add(new ListItem(/* type= */ 0, propertyModel1));
        TabStripSnapshotter tabStripSnapshotter =
                new TabStripSnapshotter(this::onModelTokenChange, modelList, mRecyclerView);

        Mockito.verify(mRecyclerView, Mockito.times(1))
                .addOnScrollListener(mOnScrollListenerCaptor.capture());
        OnScrollListener onScrollListener = mOnScrollListenerCaptor.getValue();
        Assert.assertEquals(1, mTokenList.size());

        PropertyModel propertyModel2 = makePropertyModel(mTabFaviconFetcherA, true, true);
        modelList.add(new ListItem(/* type= */ 0, propertyModel2));
        Assert.assertEquals(2, mTokenList.size());
        Assert.assertNotEquals(mTokenList.get(0), mTokenList.get(1));

        propertyModel1.set(TabProperties.FAVICON_FETCHER, mTabFaviconFetcherC);
        Assert.assertEquals(3, mTokenList.size());
        Assert.assertNotEquals(mTokenList.get(1), mTokenList.get(2));

        propertyModel1.set(TabProperties.FAVICON_FETCHER, mTabFaviconFetcherA);
        Assert.assertEquals(4, mTokenList.size());
        Assert.assertNotEquals(mTokenList.get(2), mTokenList.get(3));

        propertyModel1.set(TabProperties.IS_SELECTED, true);
        Assert.assertEquals(5, mTokenList.size());
        Assert.assertNotEquals(mTokenList.get(3), mTokenList.get(4));

        propertyModel1.set(TabProperties.FAVICON_FETCHED, true);
        Assert.assertEquals(6, mTokenList.size());
        Assert.assertNotEquals(mTokenList.get(1), mTokenList.get(5));
        Assert.assertNotEquals(mTokenList.get(4), mTokenList.get(5));

        Mockito.when(mRecyclerView.computeHorizontalScrollOffset()).thenReturn(100);
        onScrollListener.onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_DRAGGING);
        onScrollListener.onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_SETTLING);
        Assert.assertEquals(6, mTokenList.size());

        onScrollListener.onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_IDLE);
        Assert.assertEquals(7, mTokenList.size());
        Assert.assertNotEquals(mTokenList.get(5), mTokenList.get(6));

        Mockito.when(mRecyclerView.computeHorizontalScrollOffset()).thenReturn(0);
        onScrollListener.onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_IDLE);
        Assert.assertEquals(8, mTokenList.size());
        Assert.assertEquals(mTokenList.get(5), mTokenList.get(7));

        tabStripSnapshotter.destroy();
        Mockito.verify(mRecyclerView, Mockito.times(1)).removeOnScrollListener(onScrollListener);
        propertyModel1.set(TabProperties.FAVICON_FETCHER, mTabFaviconFetcherB);
        Assert.assertEquals(8, mTokenList.size());
    }
}
