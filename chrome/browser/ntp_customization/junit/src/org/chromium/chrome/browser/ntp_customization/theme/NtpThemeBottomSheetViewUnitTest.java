// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.DEFAULT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;

/** Unit tests for {@link NtpThemeBottomSheetView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpThemeBottomSheetViewUnitTest {
    private static final int[] SECTION_TYPES = {
        DEFAULT, IMAGE_FROM_DISK, CHROME_COLOR, THEME_COLLECTION
    };

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OnClickListener mOnClickListener;

    private NtpThemeBottomSheetView mNtpThemeBottomSheetView;
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        mNtpThemeBottomSheetView =
                (NtpThemeBottomSheetView)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout.ntp_customization_theme_bottom_sheet_layout,
                                        null,
                                        false);
    }

    @Test
    public void testDestroy() {
        for (int sectionType : SECTION_TYPES) {
            mNtpThemeBottomSheetView.setSectionOnClickListener(sectionType, mOnClickListener);
            assertTrue(getSection(sectionType).hasOnClickListeners());
        }

        mNtpThemeBottomSheetView.destroy();

        for (int sectionType : SECTION_TYPES) {
            assertFalse(getSection(sectionType).hasOnClickListeners());
        }
    }

    @Test
    public void testUpdateSectionTrailingIcon() {
        String selected = mContext.getString(R.string.selected);
        String showMore = mContext.getString(R.string.ntp_customization_show_more);

        ImageView defaultTrailingIcon = getSection(DEFAULT).findViewById(R.id.trailing_icon);
        mNtpThemeBottomSheetView.updateSectionTrailingIcon(DEFAULT, /* visible= */ true);
        assertEquals(View.VISIBLE, defaultTrailingIcon.getVisibility());
        assertEquals(selected, defaultTrailingIcon.getContentDescription());
        mNtpThemeBottomSheetView.updateSectionTrailingIcon(DEFAULT, /* visible= */ false);
        assertEquals(View.INVISIBLE, defaultTrailingIcon.getVisibility());
        assertNull(defaultTrailingIcon.getContentDescription());

        ImageView uploadTrailingIcon = getSection(IMAGE_FROM_DISK).findViewById(R.id.trailing_icon);
        mNtpThemeBottomSheetView.updateSectionTrailingIcon(IMAGE_FROM_DISK, /* visible= */ true);
        assertEquals(selected, uploadTrailingIcon.getContentDescription());
        mNtpThemeBottomSheetView.updateSectionTrailingIcon(IMAGE_FROM_DISK, /* visible= */ false);
        assertEquals(showMore, uploadTrailingIcon.getContentDescription());
    }

    @Test
    public void testSetSectionOnClickListener() {
        mNtpThemeBottomSheetView.setSectionOnClickListener(CHROME_COLOR, mOnClickListener);
        View chromeColorsSection = getSection(CHROME_COLOR);
        chromeColorsSection.performClick();
        verify(mOnClickListener).onClick(chromeColorsSection);

        mNtpThemeBottomSheetView.setSectionOnClickListener(THEME_COLLECTION, mOnClickListener);
        View themeCollectionsSection = getSection(THEME_COLLECTION);
        themeCollectionsSection.performClick();
        verify(mOnClickListener).onClick(themeCollectionsSection);
    }

    @Test
    public void testSetLeadingIconForThemeCollections() {
        Drawable primaryDrawable =
                mContext.getDrawable(R.drawable.upload_an_image_icon_for_theme_bottom_sheet);
        Drawable secondaryDrawable =
                mContext.getDrawable(R.drawable.upload_an_image_icon_for_theme_bottom_sheet);
        final Pair<Drawable, Drawable> pair = new Pair<>(primaryDrawable, secondaryDrawable);
        mNtpThemeBottomSheetView.setLeadingIconForThemeCollections(pair);

        View iconView = getSection(THEME_COLLECTION).findViewById(R.id.leading_icon);
        ImageView primaryImage = iconView.findViewById(R.id.primary_image);
        ImageView secondaryImage = iconView.findViewById(R.id.secondary_image);
        assertEquals(primaryDrawable, primaryImage.getDrawable());
        assertEquals(secondaryDrawable, secondaryImage.getDrawable());
    }

    private NtpThemeListItemView getSection(int sectionType) {
        return mNtpThemeBottomSheetView.getItemBySectionType(sectionType);
    }
}
