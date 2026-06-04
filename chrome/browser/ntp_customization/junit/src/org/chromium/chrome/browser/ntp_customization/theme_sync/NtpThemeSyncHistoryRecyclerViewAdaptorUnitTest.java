// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.PlatformType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link NtpThemeSyncHistoryRecyclerViewAdaptor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpThemeSyncHistoryRecyclerViewAdaptorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<NtpBackgroundDataBase> mOnItemClickCallback;
    @Mock private View.OnClickListener mOnClickListener;
    @Mock private View mItemView;
    @Mock private ImageView mBackgroundView;
    @Mock private ImageView mBadgeView;
    @Mock private Drawable mDrawable;
    @Mock private NtpBackgroundDataBase mData1;
    @Mock private NtpBackgroundDataBase mData2;

    private Context mContext;
    private List<NtpBackgroundDataBase> mDataList;
    private NtpThemeSyncHistoryRecyclerViewAdaptor mAdapter;
    private NtpThemeSyncHistoryRecyclerViewAdaptor.ImageViewHolder mViewHolder;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mDataList = new ArrayList<>();
        mDataList.add(mData1);
        mDataList.add(mData2);

        when(mData1.getPlatformType()).thenReturn(PlatformType.ANDROID_LOCAL);
        when(mData1.getImageDrawable()).thenReturn(mDrawable);
        when(mData2.getPlatformType()).thenReturn(PlatformType.DESKTOP);
        when(mData2.getImageDrawable()).thenReturn(mDrawable);

        mAdapter =
                new NtpThemeSyncHistoryRecyclerViewAdaptor(
                        mContext, mDataList, mOnItemClickCallback, /* selectedPosition= */ 0);
    }

    @Test
    public void testGetItemCount() {
        assertEquals(
                "Item count should match the list size", mDataList.size(), mAdapter.getItemCount());

        mAdapter =
                new NtpThemeSyncHistoryRecyclerViewAdaptor(
                        mContext,
                        Collections.emptyList(),
                        mOnItemClickCallback,
                        /* selectedPosition= */ 0);
        assertEquals("Item count should be 0 for an empty list", 0, mAdapter.getItemCount());
    }

    @Test
    public void testOnCreateViewHolder() {
        ViewGroup parent = new FrameLayout(mContext);
        NtpThemeSyncHistoryRecyclerViewAdaptor.ImageViewHolder viewHolder =
                mAdapter.onCreateViewHolder(parent, 0);
        assertEquals(
                R.id.background_view,
                viewHolder.itemView.findViewById(R.id.background_view).getId());
    }

    @Test
    public void testBindViewHolder() {
        ViewGroup parent = new FrameLayout(mContext);
        mViewHolder = mAdapter.onCreateViewHolder(parent, /* viewType= */ 0);

        int selectedPosition = 0;
        int bindingAdaptorPosition = 0;

        // Test selected item case, ANDROID_LOCAL (no badge).
        mViewHolder.bindImpl(
                PlatformType.ANDROID_LOCAL,
                mDrawable,
                mOnClickListener,
                selectedPosition,
                bindingAdaptorPosition);
        assertTrue(mViewHolder.itemView.isActivated());
        assertTrue(mViewHolder.itemView.isSelected());
        ImageView badgeView = mViewHolder.itemView.findViewById(R.id.platform_badge);
        assertEquals(View.GONE, badgeView.getVisibility());

        // Test unselected item case, DESKTOP (has badge).
        bindingAdaptorPosition = 1;
        mViewHolder.bindImpl(
                PlatformType.DESKTOP,
                mDrawable,
                mOnClickListener,
                selectedPosition,
                bindingAdaptorPosition);
        assertFalse(mViewHolder.itemView.isActivated());
        assertFalse(mViewHolder.itemView.isSelected());
        assertEquals(View.VISIBLE, badgeView.getVisibility());

        // Test recycling: bind back to ANDROID_LOCAL and verify badge is hidden.
        mViewHolder.bindImpl(
                PlatformType.ANDROID_LOCAL,
                mDrawable,
                mOnClickListener,
                selectedPosition,
                bindingAdaptorPosition);
        assertEquals(View.GONE, badgeView.getVisibility());
    }

    @Test
    public void testBindViewHolder_setOnClickListener() {
        when(mItemView.getContext()).thenReturn(mContext);
        when(mItemView.findViewById(R.id.background_view)).thenReturn(mBackgroundView);
        when(mItemView.findViewById(R.id.platform_badge)).thenReturn(mBadgeView);
        when(mItemView.getResources()).thenReturn(mContext.getResources());
        mViewHolder = new NtpThemeSyncHistoryRecyclerViewAdaptor.ImageViewHolder(mItemView);

        // Binds the first item view.
        int position = 0;
        mAdapter.onBindViewHolder(mViewHolder, position);
        verify(mItemView).setOnClickListener(any(View.OnClickListener.class));

        clearInvocations(mItemView);
        verify(mItemView, never()).setOnClickListener(any(View.OnClickListener.class));
    }

    @Test
    public void testOnViewRecycled() {
        when(mItemView.getContext()).thenReturn(mContext);
        mViewHolder = new NtpThemeSyncHistoryRecyclerViewAdaptor.ImageViewHolder(mItemView);

        mAdapter.onViewRecycled(mViewHolder);
        verify(mItemView).setOnClickListener(null);
    }

    @Test
    public void testClickHandler() {
        ViewGroup parent = new FrameLayout(mContext);
        NtpThemeSyncHistoryRecyclerViewAdaptor.ImageViewHolder viewHolder =
                mAdapter.onCreateViewHolder(parent, /* viewType= */ 0);
        int position = 1;
        mAdapter.onBindViewHolder(viewHolder, position);

        viewHolder.itemView.performClick();
        verify(mOnItemClickCallback).onResult(mDataList.get(position));
    }

    @Test
    @SuppressWarnings("unchecked")
    public void testSetSelectedPosition() {
        // Initial selected position is 0.
        assertEquals(0, mAdapter.getSelectedPositionForTesting());

        int selectedPosition = 1;
        mAdapter.setSelectedPosition(selectedPosition, /* isFromClick= */ true);

        // Verify the new selected position and that the callback was invoked.
        assertEquals(selectedPosition, mAdapter.getSelectedPositionForTesting());
        verify(mOnItemClickCallback).onResult(mDataList.get(selectedPosition));

        // Verify the new selected position and that the callback was not invoke if the selected
        // position is not from a click event.
        clearInvocations(mOnItemClickCallback);
        mAdapter.setSelectedPosition(selectedPosition, /* isFromClick= */ false);

        assertEquals(selectedPosition, mAdapter.getSelectedPositionForTesting());
        verify(mOnItemClickCallback, never()).onResult(mDataList.get(selectedPosition));
    }

    @Test
    public void testSetSelectedPosition_invalidPosition() {
        // Set invalid selected position.
        mAdapter.setSelectedPosition(mDataList.size() + 1, /* isFromClick= */ false);
        // Verify the selected position is RecyclerView.NO_POSITION and no callback.
        assertEquals(RecyclerView.NO_POSITION, mAdapter.getSelectedPositionForTesting());
        verify(mOnItemClickCallback, never()).onResult(any());

        // Set another invalid position (negative).
        mAdapter.setSelectedPosition(-5, /* isFromClick= */ false);
        assertEquals(RecyclerView.NO_POSITION, mAdapter.getSelectedPositionForTesting());
        verify(mOnItemClickCallback, never()).onResult(any());
    }

    @Test
    public void testSetSelectedPositionImpl_unselect() {
        // Initial selected position is 0.
        assertEquals(0, mAdapter.getSelectedPositionForTesting());

        // Set selected position to NO_POSITION.
        mAdapter.setSelectedPosition(RecyclerView.NO_POSITION, /* isFromClick= */ false);

        // Verify the selected position is NO_POSITION and callback is not invoked.
        assertEquals(RecyclerView.NO_POSITION, mAdapter.getSelectedPositionForTesting());
        verify(mOnItemClickCallback, never()).onResult(any());
    }
}
