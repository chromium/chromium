// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;
import android.widget.ImageView;

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
    @Mock private View mItemView;
    @Mock private View mItemView1;
    @Mock private ImageView mCircleView;
    @Mock private ImageView mCircleView1;
    @Captor private ArgumentCaptor<View.OnClickListener> mOnClickListenerCaptor;
    @Captor private ArgumentCaptor<View.OnClickListener> mOnClickListenerCaptor1;

    private Context mContext;
    private List<NtpThemeColorInfo> mColorResources;
    private NtpChromeColorsAdapter mAdapter;
    private NtpChromeColorsAdapter.ColorViewHolder mViewHolder;
    private NtpChromeColorsAdapter.ColorViewHolder mViewHolder1;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mColorResources = NtpThemeColorUtils.createThemeColorList(mContext);
        mAdapter = new NtpChromeColorsAdapter(mContext, mColorResources, mOnItemClickCallback);
    }

    @Test
    public void testGetItemCount() {
        assertEquals(
                "Item count should match the list size",
                mColorResources.size(),
                mAdapter.getItemCount());

        mAdapter =
                new NtpChromeColorsAdapter(mContext, Collections.emptyList(), mOnItemClickCallback);
        assertEquals("Item count should be 0 for an empty list", 0, mAdapter.getItemCount());
    }

    @Test
    public void testOnBindViewHolder() {
        when(mItemView.getContext()).thenReturn(mContext);
        when(mItemView.findViewById(R.id.color_circle)).thenReturn(mCircleView);
        mViewHolder = new NtpChromeColorsAdapter.ColorViewHolder(mItemView);

        // Binds the first item view.
        int position = 0;
        mAdapter.onBindViewHolder(mViewHolder, position);

        NtpThemeColorInfo colorInfo = mColorResources.get(position);
        verify(mItemView).setActivated(eq(true));
        verify(mItemView).setOnClickListener(mOnClickListenerCaptor.capture());

        clearInvocations(mItemView);
        mOnClickListenerCaptor.getValue().onClick(mItemView);
        verify(mOnItemClickCallback).onResult(eq(colorInfo));

        // Binds the second item view.
        when(mItemView1.getContext()).thenReturn(mContext);
        when(mItemView1.findViewById(R.id.color_circle)).thenReturn(mCircleView1);
        mViewHolder1 = new NtpChromeColorsAdapter.ColorViewHolder(mItemView1);
        position = 1;

        mAdapter.onBindViewHolder(mViewHolder1, position);
        verify(mItemView1, never()).setActivated(eq(true));
        verify(mItemView1).setOnClickListener(mOnClickListenerCaptor1.capture());

        mOnClickListenerCaptor1.getValue().onClick(mItemView1);
        verify(mOnItemClickCallback).onResult(eq(mColorResources.get(position)));
    }
}
