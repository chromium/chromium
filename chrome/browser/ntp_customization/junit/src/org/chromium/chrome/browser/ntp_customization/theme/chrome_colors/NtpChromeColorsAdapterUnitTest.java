// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
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

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link NtpChromeColorsAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpChromeColorsAdapterUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<NtpThemeColorInfo> mOnItemClickCallback;
    @Mock private View.OnClickListener mOnClickListener;
    @Mock private View mItemView;
    @Mock private ImageView mCircleView;

    private Context mContext;
    private List<NtpThemeColorInfo> mColorInfoList;
    private NtpChromeColorsAdapter mAdapter;
    private NtpChromeColorsAdapter.ColorViewHolder mViewHolder;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mColorInfoList = NtpThemeColorUtils.createThemeColorListForTesting(mContext);
        mAdapter =
                new NtpChromeColorsAdapter(
                        mContext, mColorInfoList, mOnItemClickCallback, /* selectedPosition= */ 0);
    }

    @Test
    public void testGetItemCount() {
        assertEquals(
                "Item count should match the list size",
                mColorInfoList.size(),
                mAdapter.getItemCount());

        mAdapter =
                new NtpChromeColorsAdapter(
                        mContext,
                        Collections.emptyList(),
                        mOnItemClickCallback,
                        /* selectedPosition= */ 0);
        assertEquals("Item count should be 0 for an empty list", 0, mAdapter.getItemCount());
    }

    @Test
    public void testOnCreateViewHolder() {
        ViewGroup parent = new FrameLayout(mContext);
        NtpChromeColorsAdapter.ColorViewHolder viewHolder = mAdapter.onCreateViewHolder(parent, 0);
        assertEquals(
                R.id.color_circle, viewHolder.itemView.findViewById(R.id.color_circle).getId());
    }

    @Test
    public void testBindViewHolder() {
        ViewGroup parent = new FrameLayout(mContext);
        mViewHolder = mAdapter.onCreateViewHolder(parent, /* viewType= */ 0);

        int selectedPosition = 0;
        int bindingAdaptorPosition = 0;
        NtpThemeColorInfo colorInfo = mColorInfoList.get(bindingAdaptorPosition);
        String contentDescription =
                mContext.getString(NtpThemeColorUtils.getNtpColorThemeStringResId(colorInfo.id));

        // Test selected item case.
        mViewHolder.bindImpl(
                mColorInfoList.get(bindingAdaptorPosition),
                mOnClickListener,
                selectedPosition,
                bindingAdaptorPosition);
        assertTrue(mViewHolder.itemView.isActivated());
        assertTrue(mViewHolder.itemView.isSelected());
        assertEquals(contentDescription, mViewHolder.itemView.getContentDescription());

        // Test unselected item case.
        selectedPosition = 1;
        mViewHolder.bindImpl(
                mColorInfoList.get(bindingAdaptorPosition),
                mOnClickListener,
                selectedPosition,
                bindingAdaptorPosition);
        assertFalse(mViewHolder.itemView.isActivated());
        assertFalse(mViewHolder.itemView.isSelected());
        assertEquals(contentDescription, mViewHolder.itemView.getContentDescription());
    }

    @Test
    public void testBindViewHolder_setOnClickListener() {
        when(mItemView.getContext()).thenReturn(mContext);
        when(mItemView.findViewById(R.id.color_circle)).thenReturn(mCircleView);
        when(mItemView.getResources()).thenReturn(mContext.getResources());
        mViewHolder = new NtpChromeColorsAdapter.ColorViewHolder(mItemView);

        // Binds the first item view.
        int position = 0;
        mAdapter.onBindViewHolder(mViewHolder, position);
        verify(mItemView).setOnClickListener(any(View.OnClickListener.class));

        clearInvocations(mItemView);
        verify(mItemView, never()).setOnClickListener(any(View.OnClickListener.class));
    }

    @Test
    public void testClickHandler() {
        ViewGroup parent = new FrameLayout(mContext);
        NtpChromeColorsAdapter.ColorViewHolder viewHolder =
                mAdapter.onCreateViewHolder(parent, /* viewType= */ 0);
        int position = 1;
        mAdapter.onBindViewHolder(viewHolder, position);
        NtpThemeColorInfo colorInfo = mColorInfoList.get(position);
        String contentDescription =
                mContext.getString(NtpThemeColorUtils.getNtpColorThemeStringResId(colorInfo.id));

        viewHolder.itemView.performClick();
        verify(mOnItemClickCallback).onResult(mColorInfoList.get(position));
        assertEquals(contentDescription, viewHolder.itemView.getContentDescription());
    }

    @Test
    public void testSetSelectedPosition() {
        // Initial selected position is 0.
        assertEquals(0, mAdapter.getSelectedPositionForTesting());

        int selectedPosition = 2;
        mAdapter.setSelectedPosition(selectedPosition);

        // Verify the new selected position and that the callback was invoked.
        assertEquals(selectedPosition, mAdapter.getSelectedPositionForTesting());
        verify(mOnItemClickCallback).onResult(mColorInfoList.get(selectedPosition));
    }

    @Test
    public void testSetSelectedPosition_invalidPosition() {
        // Set invalid selected position.
        mAdapter.setSelectedPosition(mColorInfoList.size() + 1);
        // Verify the selected position is RecyclerView.NO_POSITION and no callback.
        assertEquals(RecyclerView.NO_POSITION, mAdapter.getSelectedPositionForTesting());
        verify(mOnItemClickCallback, never()).onResult(any());

        // Set another invalid position (negative).
        mAdapter.setSelectedPosition(-5);
        assertEquals(RecyclerView.NO_POSITION, mAdapter.getSelectedPositionForTesting());
        verify(mOnItemClickCallback, never()).onResult(any());
    }

    @Test
    public void testSetSelectedPositionImpl_unselect() {
        // Initial selected position is 0.
        assertEquals(0, mAdapter.getSelectedPositionForTesting());

        // Set selected position to NO_POSITION.
        mAdapter.setSelectedPosition(RecyclerView.NO_POSITION);

        // Verify the selected position is NO_POSITION and callback is not invoked.
        assertEquals(RecyclerView.NO_POSITION, mAdapter.getSelectedPositionForTesting());
        verify(mOnItemClickCallback, never()).onResult(any());
    }
}
