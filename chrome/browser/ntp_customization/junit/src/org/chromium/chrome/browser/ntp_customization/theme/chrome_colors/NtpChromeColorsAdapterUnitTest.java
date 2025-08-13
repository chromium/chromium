// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.core.content.ContextCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link NtpChromeColorsAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpChromeColorsAdapterUnitTest {

    private Context mContext;
    private FrameLayout mParent;
    private List<Integer> mColorResources;
    private NtpChromeColorsAdapter mAdapter;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mParent = new FrameLayout(mContext);
        mColorResources = new ArrayList<>();
        for (int i = 0; i < 20; i++) {
            mColorResources.add(R.color.default_red);
            mColorResources.add(R.color.default_green);
            mColorResources.add(R.color.default_bg_color_blue);
        }
        mAdapter = new NtpChromeColorsAdapter(mContext, mColorResources);
    }

    @Test
    public void testGetItemCount() {
        NtpChromeColorsAdapter adapter = new NtpChromeColorsAdapter(mContext, mColorResources);
        assertEquals("Item count should match the list size", 60, adapter.getItemCount());

        adapter = new NtpChromeColorsAdapter(mContext, Collections.emptyList());
        assertEquals("Item count should be 0 for an empty list", 0, adapter.getItemCount());
    }

    @Test
    public void testOnBindViewHolder() {
        NtpChromeColorsAdapter.ColorViewHolder viewHolder = mAdapter.onCreateViewHolder(mParent, 0);
        for (int i = 0; i < mColorResources.size(); i++) {
            mAdapter.onBindViewHolder(viewHolder, i);

            ImageView colorCircle = viewHolder.itemView.findViewById(R.id.color_circle);
            GradientDrawable background = (GradientDrawable) colorCircle.getBackground();

            assertEquals(
                    "Color should be set correctly for item at position " + i,
                    ContextCompat.getColor(mContext, mColorResources.get(i)),
                    background.getColor().getDefaultColor());
        }
    }
}
