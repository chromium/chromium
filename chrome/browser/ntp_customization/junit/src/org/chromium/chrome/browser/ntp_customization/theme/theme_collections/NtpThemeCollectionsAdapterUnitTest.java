// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.util.Pair;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionViewHolder;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link NtpThemeCollectionsAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeCollectionsAdapterUnitTest {

    private static final List<Pair<String, Integer>> FAKE_THEME_COLLECTIONS = new ArrayList<>();

    static {
        FAKE_THEME_COLLECTIONS.add(
                new Pair<>("Collection A", R.drawable.upload_an_image_icon_for_theme_bottom_sheet));
        FAKE_THEME_COLLECTIONS.add(
                new Pair<>("Collection B", R.drawable.upload_an_image_icon_for_theme_bottom_sheet));
    }

    private NtpThemeCollectionsAdapter mAdapter;
    private Activity mActivity;
    private FrameLayout mParent;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mParent = new FrameLayout(mActivity);
        mAdapter = new NtpThemeCollectionsAdapter(FAKE_THEME_COLLECTIONS);
    }

    @Test
    public void testOnCreateViewHolder() {
        ThemeCollectionViewHolder viewHolder = mAdapter.onCreateViewHolder(mParent, 0);

        assertNotNull("ViewHolder should not be null.", viewHolder);
        assertNotNull("ViewHolder's image view should not be null.", viewHolder.mImage);
        assertNotNull("ViewHolder's title view should not be null.", viewHolder.mTitle);
    }

    @Test
    public void testOnBindViewHolder() {
        ThemeCollectionViewHolder viewHolder = mAdapter.onCreateViewHolder(mParent, 0);

        // Test binding for the first item
        mAdapter.onBindViewHolder(viewHolder, 0);

        TextView titleView = viewHolder.mTitle;
        assertEquals(
                "Title should be set from the first item.",
                FAKE_THEME_COLLECTIONS.get(0).first,
                titleView.getText().toString());

        ImageView imageView = viewHolder.mImage;
        int imageResId = Shadows.shadowOf(imageView.getDrawable()).getCreatedFromResId();
        assertEquals(
                "Image resource should be set from the first item.",
                (int) FAKE_THEME_COLLECTIONS.get(0).second,
                imageResId);

        // Test binding for the second item
        mAdapter.onBindViewHolder(viewHolder, 1);

        assertEquals(
                "Title should be updated from the second item.",
                FAKE_THEME_COLLECTIONS.get(1).first,
                titleView.getText().toString());

        imageResId = Shadows.shadowOf(imageView.getDrawable()).getCreatedFromResId();
        assertEquals(
                "Image resource should be updated from the second item.",
                (int) FAKE_THEME_COLLECTIONS.get(1).second,
                imageResId);
    }

    @Test
    public void testGetItemCount() {
        assertEquals(
                "Item count should match the size of the provided list.",
                FAKE_THEME_COLLECTIONS.size(),
                mAdapter.getItemCount());
    }
}
