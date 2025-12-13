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
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

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
        mContext = ApplicationProvider.getApplicationContext();
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

        // Test selected item case.
        mViewHolder.bindImpl(
                mColorInfoList.get(bindingAdaptorPosition),
                mOnClickListener,
                selectedPosition,
                bindingAdaptorPosition);
        assertTrue(mViewHolder.itemView.isActivated());

        // Test unselected item case.
        selectedPosition = 1;
        mViewHolder.bindImpl(
                mColorInfoList.get(bindingAdaptorPosition),
                mOnClickListener,
                selectedPosition,
                bindingAdaptorPosition);
        assertFalse(mViewHolder.itemView.isActivated());
    }

    @Test
    public void testBindViewHolder_setOnClickListener() {
        when(mItemView.getContext()).thenReturn(mContext);
        when(mItemView.findViewById(R.id.color_circle)).thenReturn(mCircleView);
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

        viewHolder.itemView.performClick();
        verify(mOnItemClickCallback).onResult(mColorInfoList.get(position));
    }
}
