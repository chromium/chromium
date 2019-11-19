// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.test.mock.MockResources;
import android.util.DisplayMetrics;
import android.util.SparseArray;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import org.chromium.chrome.R;

/**
 * This is the minimal {@link Resources} needed by the {@link LayoutManager} to be working properly.
 * Values Come from the xml files (e.g. dimens.xml, values.xml, ...).
 * All the dimension values are in dp.
 * For internal structures, not based on xml configurations, a dummy {@link Resources} instance is
 * provided.
 */
public class MockResourcesForLayout extends MockResources {
    private final Resources mValidResources;
    private final SparseArray<Float> mFloats = new SparseArray<Float>();
    private final SparseIntArray mIntegers = new SparseIntArray();
    private final SparseBooleanArray mBooleans = new SparseBooleanArray();
    private final SparseArray<String> mStrings = new SparseArray<String>();
    private final Drawable mDrawable = new ColorDrawable(Color.RED);

    public MockResourcesForLayout(Resources resources) {
        mValidResources = resources;
        mFloats.put(R.dimen.over_scroll_slide, 10.0f);
        mFloats.put(R.dimen.tabswitcher_border_frame_padding_left, 6.f);
        mFloats.put(R.dimen.tabswitcher_border_frame_padding_top, 50.f);
        mFloats.put(org.chromium.chrome.R.dimen.compositor_tab_title_text_size, 13.0f);
        mIntegers.put(R.color.tab_switcher_background, 0xFF111111);
        mFloats.put(R.dimen.over_scroll, 75.0f);
        mIntegers.put(R.integer.over_scroll_angle, 15);
        mFloats.put(R.dimen.even_out_scrolling, 400.0f);
        mFloats.put(R.dimen.min_spacing, 120.0f);
        mFloats.put(R.dimen.swipe_commit_distance, 120.0f);
        mFloats.put(R.dimen.toolbar_swipe_space_between_tabs, 30.0f);
        mFloats.put(R.dimen.toolbar_swipe_commit_distance, 90.0f);
        mIntegers.put(
                org.chromium.chrome.R.color.compositor_tab_title_bar_text_incognito, 0xFFffFFff);
        mIntegers.put(org.chromium.chrome.R.color.compositor_tab_title_bar_text, 0xFF555555);
        mFloats.put(R.dimen.tab_title_favicon_start_padding, 16.0f);
        mFloats.put(R.dimen.tab_title_favicon_end_padding, 7.0f);
        mFloats.put(org.chromium.chrome.R.dimen.compositor_tab_title_favicon_size, 16.0f);
        mFloats.put(R.dimen.stacked_tab_visible_size, 4.0f);
        mFloats.put(R.dimen.stack_buffer_width, 5.0f);
        mFloats.put(R.dimen.stack_buffer_height, 5.0f);
        mFloats.put(org.chromium.chrome.R.dimen.compositor_button_slop, 20.0f);
        mFloats.put(R.dimen.accessibility_tab_height, 65.f);
        mFloats.put(R.dimen.tabswitcher_border_frame_padding_left, 6.f);
        mFloats.put(R.dimen.tabswitcher_border_frame_padding_top, 50.f);
        mFloats.put(R.dimen.tabswitcher_border_frame_transparent_top, 3.f);
        mFloats.put(R.dimen.tabswitcher_border_frame_transparent_side, 2.f);
        mFloats.put(R.dimen.open_new_tab_animation_y_translation, -20.f);
        mBooleans.put(org.chromium.chrome.R.bool.compositor_tab_title_fake_bold_text, true);
        mStrings.put(R.string.tab_loading_default_title, "Loading...");
        mFloats.put(org.chromium.chrome.R.dimen.overlay_panel_bar_height, 56.f);
        mFloats.put(org.chromium.chrome.R.dimen.control_container_height, 56.f);
        mFloats.put(org.chromium.chrome.R.dimen.contextual_search_bar_banner_padding, 12.f);
        mFloats.put(org.chromium.chrome.R.dimen.contextual_search_padded_button_width, 48.f);
        mFloats.put(org.chromium.chrome.R.dimen.overlay_panel_end_buttons_width, 94.f);
        mFloats.put(org.chromium.chrome.R.dimen.toolbar_height_no_shadow, 56.f);
        mIntegers.put(R.color.modern_grey_100, 0xFFF1F3F4);
        mIntegers.put(R.color.modern_primary_color, Color.WHITE);
        mIntegers.put(R.color.default_primary_color, 0xFFF2F2F2);
        mIntegers.put(R.color.dark_primary_color, 0xFF3C4043);
    }

    @Override
    public DisplayMetrics getDisplayMetrics() {
        return mValidResources.getDisplayMetrics();
    }

    @Override
    public Configuration getConfiguration() {
        return mValidResources.getConfiguration();
    }

    @Override
    public int getDimensionPixelSize(int id) {
        return Math.round(getDimension(id));
    }

    @Override
    public int getDimensionPixelOffset(int id) {
        return getDimensionPixelSize(id);
    }

    @Override
    public float getDimension(int id) {
        final Float value = mFloats.get(id);
        return value != null ? value.floatValue() : mValidResources.getDimension(id);
    }

    @Override
    public int getInteger(int id) {
        if (mIntegers.indexOfKey(id) < 0) mValidResources.getInteger(id);
        return mIntegers.get(id);
    }

    @Override
    public boolean getBoolean(int id) {
        if (mBooleans.indexOfKey(id) < 0) mValidResources.getBoolean(id);
        return mBooleans.get(id);
    }

    @Override
    public String getString(int id) {
        final String value = mStrings.get(id);
        return value != null ? value.toString() : mValidResources.getString(id);
    }

    @Override
    public int getColor(int id) {
        return getInteger(id);
    }

    @Override
    public int getColor(int id, Theme theme) {
        return getInteger(id);
    }

    @Override
    public Drawable getDrawable(int id) {
        return mDrawable;
    }

    @Override
    public Drawable getDrawable(int id, Resources.Theme theme) {
        return mDrawable;
    }
}
