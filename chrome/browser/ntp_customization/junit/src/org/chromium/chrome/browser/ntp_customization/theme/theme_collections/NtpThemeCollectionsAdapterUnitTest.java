// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.spy;

import static org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionsItemType.SINGLE_THEME_COLLECTION_ITEM;
import static org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionsItemType.THEME_COLLECTIONS_ITEM;

import android.content.Context;
import android.util.Pair;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionViewHolder;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link NtpThemeCollectionsAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeCollectionsAdapterUnitTest {

    private static final int FAKE_IMAGE_RES_ID =
            R.drawable.upload_an_image_icon_for_theme_bottom_sheet;
    private static final String THEME_COLLECTION_TITLE = "Theme Collection 1";
    private static final String NEW_THEME_COLLECTION_TITLE = "Theme Collection 2";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private FrameLayout mParent;
    private View.OnClickListener mOnClickListener;
    private List<Pair<String, Integer>> mCollectionItemsWithTitle;
    private List<Integer> mCollectionItemsWithoutTitle;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mParent = new FrameLayout(mContext);

        mOnClickListener = view -> {};

        mCollectionItemsWithTitle = new ArrayList<>();
        mCollectionItemsWithTitle.add(new Pair<>(THEME_COLLECTION_TITLE, FAKE_IMAGE_RES_ID));
        mCollectionItemsWithTitle.add(new Pair<>(NEW_THEME_COLLECTION_TITLE, FAKE_IMAGE_RES_ID));

        mCollectionItemsWithoutTitle = new ArrayList<>();
        mCollectionItemsWithoutTitle.add(FAKE_IMAGE_RES_ID);
        mCollectionItemsWithoutTitle.add(FAKE_IMAGE_RES_ID);
    }

    @Test
    public void testGetItemViewType() {
        NtpThemeCollectionsAdapter adapterWithTitle =
                new NtpThemeCollectionsAdapter(
                        mCollectionItemsWithTitle, THEME_COLLECTIONS_ITEM, mOnClickListener);
        assertEquals(THEME_COLLECTIONS_ITEM, adapterWithTitle.getItemViewType(0));

        NtpThemeCollectionsAdapter adapterWithoutTitle =
                new NtpThemeCollectionsAdapter(
                        mCollectionItemsWithoutTitle,
                        SINGLE_THEME_COLLECTION_ITEM,
                        mOnClickListener);
        assertEquals(SINGLE_THEME_COLLECTION_ITEM, adapterWithoutTitle.getItemViewType(0));
    }

    @Test
    public void testOnCreateViewHolder() {
        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        mCollectionItemsWithTitle, THEME_COLLECTIONS_ITEM, mOnClickListener);
        ThemeCollectionViewHolder viewHolder =
                (ThemeCollectionViewHolder)
                        adapter.onCreateViewHolder(mParent, THEME_COLLECTIONS_ITEM);

        assertNotNull("ViewHolder should not be null.", viewHolder);
        assertNotNull("ViewHolder's view should not be null.", viewHolder.mView);
        assertNotNull("ViewHolder's image view should not be null.", viewHolder.mImage);
        assertNotNull("ViewHolder's title view should not be null.", viewHolder.mTitle);
    }

    @Test
    public void testOnBindViewHolder_withTitle() throws Exception {
        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        mCollectionItemsWithTitle, THEME_COLLECTIONS_ITEM, mOnClickListener);
        ThemeCollectionViewHolder viewHolder =
                (ThemeCollectionViewHolder)
                        adapter.onCreateViewHolder(mParent, THEME_COLLECTIONS_ITEM);
        Field itemViewTypeField = RecyclerView.ViewHolder.class.getDeclaredField("mItemViewType");
        itemViewTypeField.setAccessible(true);
        itemViewTypeField.set(viewHolder, THEME_COLLECTIONS_ITEM);

        adapter.onBindViewHolder(viewHolder, 0);

        assertEquals(THEME_COLLECTION_TITLE, viewHolder.mTitle.getText().toString());
        assertEquals(View.VISIBLE, viewHolder.mTitle.getVisibility());
        assertEquals(
                FAKE_IMAGE_RES_ID,
                Shadows.shadowOf(viewHolder.mImage.getDrawable()).getCreatedFromResId());
        assertTrue(viewHolder.mView.hasOnClickListeners());
    }

    @Test
    public void testOnBindViewHolder_withoutTitle() throws Exception {
        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        mCollectionItemsWithoutTitle,
                        SINGLE_THEME_COLLECTION_ITEM,
                        mOnClickListener);
        ThemeCollectionViewHolder viewHolder =
                (ThemeCollectionViewHolder)
                        adapter.onCreateViewHolder(mParent, SINGLE_THEME_COLLECTION_ITEM);
        Field itemViewTypeField = RecyclerView.ViewHolder.class.getDeclaredField("mItemViewType");
        itemViewTypeField.setAccessible(true);
        itemViewTypeField.set(viewHolder, SINGLE_THEME_COLLECTION_ITEM);

        adapter.onBindViewHolder(viewHolder, 0);

        assertEquals(View.GONE, viewHolder.mTitle.getVisibility());
        assertEquals(
                FAKE_IMAGE_RES_ID,
                Shadows.shadowOf(viewHolder.mImage.getDrawable()).getCreatedFromResId());
        assertTrue(viewHolder.mView.hasOnClickListeners());
    }

    @Test
    public void testGetItemCount() {
        NtpThemeCollectionsAdapter adapterWithTitle =
                new NtpThemeCollectionsAdapter(
                        mCollectionItemsWithTitle, THEME_COLLECTIONS_ITEM, mOnClickListener);
        assertEquals(mCollectionItemsWithTitle.size(), adapterWithTitle.getItemCount());

        NtpThemeCollectionsAdapter adapterWithoutTitle =
                new NtpThemeCollectionsAdapter(
                        mCollectionItemsWithoutTitle,
                        SINGLE_THEME_COLLECTION_ITEM,
                        mOnClickListener);
        assertEquals(mCollectionItemsWithoutTitle.size(), adapterWithoutTitle.getItemCount());
    }

    @Test
    public void testSetItems() {
        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        Collections.emptyList(), THEME_COLLECTIONS_ITEM, mOnClickListener);
        NtpThemeCollectionsAdapter spyAdapter = spy(adapter);
        assertEquals(0, spyAdapter.getItemCount());

        spyAdapter.setItems(mCollectionItemsWithTitle);

        assertEquals(mCollectionItemsWithTitle.size(), spyAdapter.getItemCount());
    }

    @Test
    public void testClearOnClickListeners() {
        RecyclerView recyclerView = new RecyclerView(mContext);
        recyclerView.setLayoutManager(new LinearLayoutManager(mContext));

        NtpThemeCollectionsAdapter adapter =
                new NtpThemeCollectionsAdapter(
                        mCollectionItemsWithTitle, THEME_COLLECTIONS_ITEM, mOnClickListener);
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
}
