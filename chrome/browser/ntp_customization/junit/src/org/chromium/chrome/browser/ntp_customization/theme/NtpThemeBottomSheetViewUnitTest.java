// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.CHROME_DEFAULT;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.UPLOAD_AN_IMAGE;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.ContextThemeWrapper;
import android.view.View.OnClickListener;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDrawable;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;

/** Unit tests for {@link NtpThemeBottomSheetView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeBottomSheetViewUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NtpThemeListItemView mChromeDefaultSection;
    @Mock private NtpThemeListItemView mUploadAnImageSection;
    @Mock private NtpThemeListItemView mChromeColorsSection;
    @Mock private NtpThemeListItemView mThemeCollectionsSection;
    @Mock private OnClickListener mOnClickListener;
    @Mock private NtpThemeListThemeCollectionItemIconView mThemeCollectionsItemIconView;

    private NtpThemeBottomSheetView mNtpThemeBottomSheetView;
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        mNtpThemeBottomSheetView = spy(new NtpThemeBottomSheetView(mContext, null));

        when(mNtpThemeBottomSheetView.getItemBySectionType(CHROME_DEFAULT))
                .thenReturn(mChromeDefaultSection);
        when(mNtpThemeBottomSheetView.getItemBySectionType(UPLOAD_AN_IMAGE))
                .thenReturn(mUploadAnImageSection);
        when(mNtpThemeBottomSheetView.getItemBySectionType(CHROME_COLORS))
                .thenReturn(mChromeColorsSection);
        when(mNtpThemeBottomSheetView.getItemBySectionType(THEME_COLLECTIONS))
                .thenReturn(mThemeCollectionsSection);
        when(mThemeCollectionsSection.findViewById(R.id.leading_icon))
                .thenReturn(mThemeCollectionsItemIconView);
    }

    @Test
    public void testDestroy() {
        mNtpThemeBottomSheetView.destroy();

        verify(mChromeDefaultSection).destroy();
        verify(mUploadAnImageSection).destroy();
        verify(mChromeColorsSection).destroy();
        verify(mThemeCollectionsSection).destroy();
    }

    @Test
    public void testSetSectionTrailingIconVisibility() {
        mNtpThemeBottomSheetView.setSectionTrailingIconVisibility(CHROME_DEFAULT, true);
        verify(mChromeDefaultSection).setTrailingIconVisibility(eq(true));

        mNtpThemeBottomSheetView.setSectionTrailingIconVisibility(UPLOAD_AN_IMAGE, false);
        verify(mUploadAnImageSection).setTrailingIconVisibility(eq(false));
    }

    @Test
    public void testSetSectionOnClickListener() {
        mNtpThemeBottomSheetView.setSectionOnClickListener(CHROME_COLORS, mOnClickListener);
        verify(mChromeColorsSection).setOnClickListener(mOnClickListener);

        mNtpThemeBottomSheetView.setSectionOnClickListener(THEME_COLLECTIONS, mOnClickListener);
        verify(mThemeCollectionsSection).setOnClickListener(mOnClickListener);
    }

    @Test
    public void testSetLeadingIconForThemeCollections() {
        final Pair<Integer, Integer> pair =
                new Pair<>(
                        R.drawable.upload_an_image_icon_for_theme_bottom_sheet,
                        R.drawable.upload_an_image_icon_for_theme_bottom_sheet);
        mNtpThemeBottomSheetView.setLeadingIconForThemeCollections(pair);

        ArgumentCaptor<Pair<Drawable, Drawable>> captor = ArgumentCaptor.forClass(Pair.class);
        verify(mThemeCollectionsItemIconView).setImageDrawablePair(captor.capture());

        Pair<Drawable, Drawable> capturedPair = captor.getValue();
        ShadowDrawable shadowPrimary = shadowOf(capturedPair.first);
        assertEquals(
                R.drawable.upload_an_image_icon_for_theme_bottom_sheet,
                shadowPrimary.getCreatedFromResId());

        ShadowDrawable shadowSecondary = shadowOf(capturedPair.second);
        assertEquals(
                R.drawable.upload_an_image_icon_for_theme_bottom_sheet,
                shadowSecondary.getCreatedFromResId());
    }
}
