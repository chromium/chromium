// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionsItemType.SINGLE_THEME_COLLECTION_ITEM;
import static org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionsItemType.THEME_COLLECTIONS_ITEM;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionViewHolder;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link NtpThemeCollectionsAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeCollectionsAdapterUnitTest {
    private static final String THEME_COLLECTION_TITLE = "Theme Collection 1";
    private static final GURL PREVIEW_IMAGE_URL = JUnitTestGURLs.URL_1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ImageFetcher mImageFetcher;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;

    private Context mContext;
    private FrameLayout mParent;
    private View.OnClickListener mOnClickListener;
    private List<BackgroundCollection> mCollectionItems;
    private List<CollectionImage> mImageItems;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mParent = new FrameLayout(mContext);
        mOnClickListener = view -> {};

        mCollectionItems = new ArrayList<>();
        mCollectionItems.add(
                new BackgroundCollection("id1", THEME_COLLECTION_TITLE, PREVIEW_IMAGE_URL, 123));
        mCollectionItems.add(
                new BackgroundCollection("id2", "Another Collection", JUnitTestGURLs.URL_2, 456));

        mImageItems = new ArrayList<>();
        mImageItems.add(
                new CollectionImage(
                        "id1",
                        JUnitTestGURLs.URL_1,
                        PREVIEW_IMAGE_URL,
                        new ArrayList<>(),
                        JUnitTestGURLs.URL_1));
        mImageItems.add(
                new CollectionImage(
                        "id2",
                        JUnitTestGURLs.URL_2,
                        JUnitTestGURLs.URL_2,
                        new ArrayList<>(),
                        JUnitTestGURLs.URL_2));
    }

    @Test
    public void testGetItemViewType() {
        NtpThemeCollectionsAdapter adapterWithTitle =
                new NtpThemeCollectionsAdapter(
                        mCollectionItems, THEME_COLLECTIONS_ITEM, mOnClickListener, mImageFetcher);
        assertEquals(THEME_COLLECTIONS_ITEM, adapterWithTitle.getItemViewType(0));

        NtpThemeCollectionsAdapter adapterWithoutTitle =
                new NtpThemeCollectionsAdapter(
                        mImageItems, SINGLE_THEME_COLLECTION_ITEM, mOnClickListener, mImageFetcher);
        assertEquals(SINGLE_THEME_COLLECTION_ITEM, adapterWithoutTitle.getItemViewType(0));
    }

    @Test
    public void testOnCreateViewHolder() {
        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        mCollectionItems, THEME_COLLECTIONS_ITEM, mOnClickListener, mImageFetcher);
        ThemeCollectionViewHolder viewHolder =
                (ThemeCollectionViewHolder)
                        adapter.onCreateViewHolder(mParent, THEME_COLLECTIONS_ITEM);

        assertNotNull("ViewHolder should not be null.", viewHolder);
        assertNotNull("ViewHolder's view should not be null.", viewHolder.mView);
        assertNotNull("ViewHolder's image view should not be null.", viewHolder.mImage);
        assertNotNull("ViewHolder's title view should not be null.", viewHolder.mTitle);
    }

    @Test
    public void testOnBindViewHolder_themeCollectionItem() throws Exception {
        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        mCollectionItems, THEME_COLLECTIONS_ITEM, mOnClickListener, mImageFetcher);
        ThemeCollectionViewHolder viewHolder =
                (ThemeCollectionViewHolder)
                        adapter.onCreateViewHolder(mParent, THEME_COLLECTIONS_ITEM);
        Field itemViewTypeField = RecyclerView.ViewHolder.class.getDeclaredField("mItemViewType");
        itemViewTypeField.setAccessible(true);
        itemViewTypeField.set(viewHolder, THEME_COLLECTIONS_ITEM);

        adapter.onBindViewHolder(viewHolder, 0);

        assertEquals(THEME_COLLECTION_TITLE, viewHolder.mTitle.getText().toString());
        assertEquals(View.VISIBLE, viewHolder.mTitle.getVisibility());
        assertTrue(viewHolder.mView.hasOnClickListeners());
        assertFalse(viewHolder.itemView.isActivated());

        ArgumentCaptor<ImageFetcher.Params> paramsCaptor =
                ArgumentCaptor.forClass(ImageFetcher.Params.class);
        verify(mImageFetcher).fetchImage(paramsCaptor.capture(), any());
        assertEquals(PREVIEW_IMAGE_URL.getSpec(), paramsCaptor.getValue().url);
    }

    @Test
    public void testOnBindViewHolder_singleThemeCollectionItem() throws Exception {
        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        mImageItems, SINGLE_THEME_COLLECTION_ITEM, mOnClickListener, mImageFetcher);
        ThemeCollectionViewHolder viewHolder =
                (ThemeCollectionViewHolder)
                        adapter.onCreateViewHolder(mParent, SINGLE_THEME_COLLECTION_ITEM);
        Field itemViewTypeField = RecyclerView.ViewHolder.class.getDeclaredField("mItemViewType");
        itemViewTypeField.setAccessible(true);
        itemViewTypeField.set(viewHolder, SINGLE_THEME_COLLECTION_ITEM);

        adapter.onBindViewHolder(viewHolder, 0);

        assertEquals(View.GONE, viewHolder.mTitle.getVisibility());
        assertTrue(viewHolder.mView.hasOnClickListeners());
        assertFalse(viewHolder.itemView.isActivated());

        ArgumentCaptor<ImageFetcher.Params> paramsCaptor =
                ArgumentCaptor.forClass(ImageFetcher.Params.class);
        verify(mImageFetcher).fetchImage(paramsCaptor.capture(), any());
        assertEquals(PREVIEW_IMAGE_URL.getSpec(), paramsCaptor.getValue().url);
    }

    @Test
    public void testGetItemCount() {
        NtpThemeCollectionsAdapter adapterWithTitle =
                new NtpThemeCollectionsAdapter(
                        mCollectionItems, THEME_COLLECTIONS_ITEM, mOnClickListener, mImageFetcher);
        assertEquals(mCollectionItems.size(), adapterWithTitle.getItemCount());

        NtpThemeCollectionsAdapter adapterWithoutTitle =
                new NtpThemeCollectionsAdapter(
                        mImageItems, SINGLE_THEME_COLLECTION_ITEM, mOnClickListener, mImageFetcher);
        assertEquals(mImageItems.size(), adapterWithoutTitle.getItemCount());
    }

    @Test
    public void testSetItems() {
        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        Collections.emptyList(),
                        THEME_COLLECTIONS_ITEM,
                        mOnClickListener,
                        mImageFetcher);
        assertEquals(0, adapter.getItemCount());

        adapter.setItems(mCollectionItems);

        assertEquals(mCollectionItems.size(), adapter.getItemCount());
    }

    @Test
    public void testClearOnClickListeners() {
        RecyclerView recyclerView = new RecyclerView(mContext);
        recyclerView.setLayoutManager(new LinearLayoutManager(mContext));

        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        mCollectionItems, THEME_COLLECTIONS_ITEM, mOnClickListener, mImageFetcher);
        recyclerView.setAdapter(adapter);

        // Force layout to create and bind views.
        recyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(480, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY));
        recyclerView.layout(0, 0, 480, 800);

        assertTrue(
                "RecyclerView should have children after layout.",
                recyclerView.getChildCount() > 0);

        View childView = recyclerView.getChildAt(0);
        assertTrue(
                "Child view should have a click listener after binding.",
                childView.hasOnClickListeners());

        adapter.clearOnClickListeners();

        assertFalse(
                "Child view's click listener should be cleared.", childView.hasOnClickListeners());
    }

    @Test
    public void testFetchImageWithPlaceholder() throws Exception {
        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        mCollectionItems, THEME_COLLECTIONS_ITEM, mOnClickListener, mImageFetcher);
        ThemeCollectionViewHolder viewHolder =
                (ThemeCollectionViewHolder)
                        adapter.onCreateViewHolder(mParent, THEME_COLLECTIONS_ITEM);
        Field itemViewTypeField = RecyclerView.ViewHolder.class.getDeclaredField("mItemViewType");
        itemViewTypeField.setAccessible(true);
        itemViewTypeField.set(viewHolder, THEME_COLLECTIONS_ITEM);

        // Spy the image view to verify calls to setImageBitmap.
        viewHolder.mImage = spy(viewHolder.mImage);

        adapter.onBindViewHolder(viewHolder, 0);

        verify(mImageFetcher).fetchImage(any(), mCallbackCaptor.capture());
        assertEquals(PREVIEW_IMAGE_URL, viewHolder.mImage.getTag());

        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        mCallbackCaptor.getValue().onResult(bitmap);

        verify(viewHolder.mImage).setImageBitmap(bitmap);
    }

    @Test
    public void testFetchImageWithPlaceholder_viewRecycled() throws Exception {
        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        mCollectionItems, THEME_COLLECTIONS_ITEM, mOnClickListener, mImageFetcher);
        ThemeCollectionViewHolder viewHolder =
                (ThemeCollectionViewHolder)
                        adapter.onCreateViewHolder(mParent, THEME_COLLECTIONS_ITEM);
        Field itemViewTypeField = RecyclerView.ViewHolder.class.getDeclaredField("mItemViewType");
        itemViewTypeField.setAccessible(true);
        itemViewTypeField.set(viewHolder, THEME_COLLECTIONS_ITEM);

        // Spy the image view to verify calls to setImageBitmap.
        viewHolder.mImage = spy(viewHolder.mImage);

        // First bind
        adapter.onBindViewHolder(viewHolder, 0);
        verify(mImageFetcher).fetchImage(any(), mCallbackCaptor.capture());
        Callback<Bitmap> firstCallback = mCallbackCaptor.getValue();
        assertEquals(PREVIEW_IMAGE_URL, viewHolder.mImage.getTag());

        // Second bind (simulating recycling)
        adapter.onBindViewHolder(viewHolder, 1);
        verify(mImageFetcher, times(2)).fetchImage(any(), mCallbackCaptor.capture());
        assertEquals(JUnitTestGURLs.URL_2, viewHolder.mImage.getTag());

        // Now the first fetch completes. The bitmap should not be set because the tag has changed.
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        firstCallback.onResult(bitmap);

        verify(viewHolder.mImage, never()).setImageBitmap(bitmap);
    }

    @Test
    public void testSetSelection_themeCollectionItem() throws Exception {
        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        mCollectionItems, THEME_COLLECTIONS_ITEM, mOnClickListener, mImageFetcher);
        ThemeCollectionViewHolder viewHolder =
                (ThemeCollectionViewHolder)
                        adapter.onCreateViewHolder(mParent, THEME_COLLECTIONS_ITEM);
        Field itemViewTypeField = RecyclerView.ViewHolder.class.getDeclaredField("mItemViewType");
        itemViewTypeField.setAccessible(true);
        itemViewTypeField.set(viewHolder, THEME_COLLECTIONS_ITEM);

        // Initially, nothing is selected.
        adapter.onBindViewHolder(viewHolder, 0);
        assertFalse(viewHolder.itemView.isActivated());

        // Select the first item.
        adapter.setSelection(mCollectionItems.get(0).id, null);
        adapter.onBindViewHolder(viewHolder, 0);
        assertTrue(viewHolder.itemView.isActivated());

        // Bind a different item, it should not be selected.
        ThemeCollectionViewHolder viewHolder2 =
                (ThemeCollectionViewHolder)
                        adapter.onCreateViewHolder(mParent, THEME_COLLECTIONS_ITEM);
        itemViewTypeField.set(viewHolder2, THEME_COLLECTIONS_ITEM);
        adapter.onBindViewHolder(viewHolder2, 1);
        assertFalse(viewHolder2.itemView.isActivated());

        // Select the second item.
        adapter.setSelection(mCollectionItems.get(1).id, null);
        adapter.onBindViewHolder(viewHolder, 0);
        assertFalse(viewHolder.itemView.isActivated());
        adapter.onBindViewHolder(viewHolder2, 1);
        assertTrue(viewHolder2.itemView.isActivated());
    }

    @Test
    public void testSetSelection_singleThemeCollectionItem() throws Exception {
        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        mImageItems, SINGLE_THEME_COLLECTION_ITEM, mOnClickListener, mImageFetcher);
        ThemeCollectionViewHolder viewHolder =
                (ThemeCollectionViewHolder)
                        adapter.onCreateViewHolder(mParent, SINGLE_THEME_COLLECTION_ITEM);
        Field itemViewTypeField = RecyclerView.ViewHolder.class.getDeclaredField("mItemViewType");
        itemViewTypeField.setAccessible(true);
        itemViewTypeField.set(viewHolder, SINGLE_THEME_COLLECTION_ITEM);

        // Initially, nothing is selected.
        adapter.onBindViewHolder(viewHolder, 0);
        assertFalse(viewHolder.itemView.isActivated());

        // Select the first item.
        adapter.setSelection(mImageItems.get(0).collectionId, mImageItems.get(0).imageUrl);
        adapter.onBindViewHolder(viewHolder, 0);
        assertTrue(viewHolder.itemView.isActivated());

        // Select with only matching collectionId, should not be activated.
        adapter.setSelection(mImageItems.get(0).collectionId, JUnitTestGURLs.URL_2);
        adapter.onBindViewHolder(viewHolder, 0);
        assertFalse(viewHolder.itemView.isActivated());

        // Select with only matching imageUrl, should not be activated.
        adapter.setSelection("id2", mImageItems.get(0).imageUrl);
        adapter.onBindViewHolder(viewHolder, 0);
        assertFalse(viewHolder.itemView.isActivated());
    }

    @Test
    public void testOnCreateViewHolder_layoutParams() {
        // Test for THEME_COLLECTIONS_ITEM
        NtpThemeCollectionsAdapter adapterWithTitle =
                new NtpThemeCollectionsAdapter(
                        mCollectionItems, THEME_COLLECTIONS_ITEM, mOnClickListener, mImageFetcher);
        ThemeCollectionViewHolder viewHolderWithTitle =
                (ThemeCollectionViewHolder)
                        adapterWithTitle.onCreateViewHolder(mParent, THEME_COLLECTIONS_ITEM);

        ImageView imageViewWithTitle = viewHolderWithTitle.mImage;
        ConstraintLayout.LayoutParams paramsWithTitle =
                (ConstraintLayout.LayoutParams) imageViewWithTitle.getLayoutParams();

        int expectedHeight =
                mContext.getResources()
                        .getDimensionPixelSize(
                                R.dimen.ntp_customization_theme_collections_list_item_height);
        assertEquals(expectedHeight, paramsWithTitle.height);
        assertNull(paramsWithTitle.dimensionRatio);

        // Test for SINGLE_THEME_COLLECTION_ITEM
        NtpThemeCollectionsAdapter adapterWithoutTitle =
                new NtpThemeCollectionsAdapter(
                        mImageItems, SINGLE_THEME_COLLECTION_ITEM, mOnClickListener, mImageFetcher);
        ThemeCollectionViewHolder viewHolderWithoutTitle =
                (ThemeCollectionViewHolder)
                        adapterWithoutTitle.onCreateViewHolder(
                                mParent, SINGLE_THEME_COLLECTION_ITEM);

        ImageView imageViewWithoutTitle = viewHolderWithoutTitle.mImage;
        ConstraintLayout.LayoutParams paramsWithoutTitle =
                (ConstraintLayout.LayoutParams) imageViewWithoutTitle.getLayoutParams();

        assertEquals(ConstraintLayout.LayoutParams.MATCH_CONSTRAINT, paramsWithoutTitle.height);
        assertEquals("1:1", paramsWithoutTitle.dimensionRatio);
    }
}
