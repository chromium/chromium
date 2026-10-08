// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselAdapter.ViewHolder;

import java.util.List;

/** Unit tests for {@link SafetyPromoCarouselAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoCarouselAdapterUnitTest {
    private static final List<SafetyPromoItem> TEST_ITEMS =
            List.of(
                    SafetyPromoItem.PASSWORD_MANAGER,
                    SafetyPromoItem.ENHANCED_SAFE_BROWSING,
                    SafetyPromoItem.INCOGNITO);

    private Context mContext;
    private SafetyPromoCarouselAdapter mAdapter;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mAdapter =
                new SafetyPromoCarouselAdapter(
                        TEST_ITEMS, R.layout.safety_promo_carousel_portrait_illustration);
    }

    @Test
    public void testGetItemCount() {
        assertEquals(3, mAdapter.getItemCount());
    }

    @Test
    public void testCreateAndBindViewHolder() {
        FrameLayout parent = new FrameLayout(mContext);
        ViewHolder holder = mAdapter.onCreateViewHolder(parent, 0);
        assertNotNull(holder);
        assertNotNull(holder.mImageView);
        assertEquals(R.id.carousel_illustration, holder.itemView.getId());

        mAdapter.onBindViewHolder(holder, 0);
        assertNotNull(holder.mImageView.getDrawable());
    }

    @Test
    public void testCreateAndBindViewHolder_landscape() {
        SafetyPromoCarouselAdapter adapter =
                new SafetyPromoCarouselAdapter(
                        TEST_ITEMS, R.layout.safety_promo_carousel_landscape_illustration);
        FrameLayout parent = new FrameLayout(mContext);
        ViewHolder holder = adapter.onCreateViewHolder(parent, 0);
        assertNotNull(holder);
        assertNotNull(holder.mImageView);
        assertEquals(R.id.carousel_landscape_illustration_container, holder.itemView.getId());

        adapter.onBindViewHolder(holder, 0);
        assertNotNull(holder.mImageView.getDrawable());
    }
}
