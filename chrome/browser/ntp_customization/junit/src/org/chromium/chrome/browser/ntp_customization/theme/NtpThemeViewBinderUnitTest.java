// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.DEFAULT;

import android.content.Context;
import android.util.Pair;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link NtpThemeViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeViewBinderUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private View.OnClickListener mOnClickListener;
    @Mock private NtpThemeBottomSheetView mNtpThemeBottomSheetView;

    private Context mContext;
    private PropertyModel mModel;
    private NtpThemeBottomSheetView mView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mView =
                (NtpThemeBottomSheetView)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout.ntp_customization_theme_bottom_sheet_layout,
                                        null,
                                        false);
        mModel = new PropertyModel(NtpThemeProperty.THEME_KEYS);
    }

    @Test
    public void testLearnMoreButtonClickListener() {
        PropertyModelChangeProcessor.create(
                mModel, mView, NtpThemeViewBinder::bindThemeBottomSheet);

        mModel.set(NtpThemeProperty.LEARN_MORE_BUTTON_CLICK_LISTENER, mOnClickListener);
        ImageView learnMoreButton = mView.findViewById(R.id.learn_more_button);
        learnMoreButton.performClick();
        verify(mOnClickListener).onClick(learnMoreButton);
    }

    @Test
    public void testSectionTrailingIconVisibility() {
        PropertyModelChangeProcessor.create(
                mModel, mNtpThemeBottomSheetView, NtpThemeViewBinder::bindThemeBottomSheet);

        Pair<Integer, Boolean> pair = new Pair<>(DEFAULT, true);
        mModel.set(NtpThemeProperty.IS_SECTION_TRAILING_ICON_VISIBLE, pair);
        verify(mNtpThemeBottomSheetView).setSectionTrailingIconVisibility(DEFAULT, true);

        pair = new Pair<>(DEFAULT, false);
        mModel.set(NtpThemeProperty.IS_SECTION_TRAILING_ICON_VISIBLE, pair);
        verify(mNtpThemeBottomSheetView).setSectionTrailingIconVisibility(DEFAULT, false);
    }

    @Test
    public void testSectionOnClickListener() {
        PropertyModelChangeProcessor.create(
                mModel, mNtpThemeBottomSheetView, NtpThemeViewBinder::bindThemeBottomSheet);

        final Pair<Integer, View.OnClickListener> pair = new Pair<>(DEFAULT, mOnClickListener);
        mModel.set(NtpThemeProperty.SECTION_ON_CLICK_LISTENER, pair);
        verify(mNtpThemeBottomSheetView).setSectionOnClickListener(DEFAULT, mOnClickListener);
    }

    @Test
    public void testLeadingIconForThemeCollections() {
        PropertyModelChangeProcessor.create(
                mModel, mNtpThemeBottomSheetView, NtpThemeViewBinder::bindThemeBottomSheet);

        final Pair<Integer, Integer> pair =
                new Pair<>(
                        R.drawable.upload_an_image_icon_for_theme_bottom_sheet,
                        R.drawable.upload_an_image_icon_for_theme_bottom_sheet);
        mModel.set(NtpThemeProperty.LEADING_ICON_FOR_THEME_COLLECTIONS, pair);
        verify(mNtpThemeBottomSheetView).setLeadingIconForThemeCollections(pair);
    }
}
