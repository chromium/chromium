// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link NtpThemeViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeViewBinderUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private View.OnClickListener mOnClickListener;
    @Mock private NtpThemeBottomSheetView mNtpThemeBottomSheetView;

    private Activity mActivity;
    private PropertyModel mModel;
    private NtpThemeBottomSheetView mView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mView =
                (NtpThemeBottomSheetView)
                        LayoutInflater.from(mActivity)
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

        Pair<Integer, Boolean> pair = new Pair<>(NTPThemeBottomSheetSection.CHROME_DEFAULT, true);
        mModel.set(NtpThemeProperty.IS_SECTION_TRAILING_ICON_VISIBLE, pair);
        verify(mNtpThemeBottomSheetView)
                .setSectionTrailingIconVisibility(NTPThemeBottomSheetSection.CHROME_DEFAULT, true);

        pair = new Pair<>(NTPThemeBottomSheetSection.CHROME_DEFAULT, false);
        mModel.set(NtpThemeProperty.IS_SECTION_TRAILING_ICON_VISIBLE, pair);
        verify(mNtpThemeBottomSheetView)
                .setSectionTrailingIconVisibility(NTPThemeBottomSheetSection.CHROME_DEFAULT, false);
    }

    @Test
    public void testSectionOnClickListener() {
        PropertyModelChangeProcessor.create(
                mModel, mNtpThemeBottomSheetView, NtpThemeViewBinder::bindThemeBottomSheet);

        final Pair<Integer, View.OnClickListener> pair =
                new Pair<>(NTPThemeBottomSheetSection.CHROME_DEFAULT, mOnClickListener);
        mModel.set(NtpThemeProperty.SECTION_ON_CLICK_LISTENER, pair);
        verify(mNtpThemeBottomSheetView)
                .setSectionOnClickListener(
                        NTPThemeBottomSheetSection.CHROME_DEFAULT, mOnClickListener);
    }
}
