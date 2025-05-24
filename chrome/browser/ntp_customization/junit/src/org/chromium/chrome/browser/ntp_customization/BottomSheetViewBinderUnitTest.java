// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ViewFlipper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BottomSheetViewBinder} */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private View mContentView;
    private PropertyModel mPropertyModel;
    private ViewFlipper mViewFlipper;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mContentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.ntp_customization_bottom_sheet, /* root= */ null);
        mPropertyModel = new PropertyModel(NtpCustomizationViewProperties.BOTTOM_SHEET_KEYS);
        mViewFlipper = mContentView.findViewById(R.id.ntp_customization_view_flipper);
    }

    @Test
    public void testBindOnNtpCardsBottomSheet() {
        // Adds the ntp cards bottom sheet to the view of the activity.
        View ntpCardsBottomSheet =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.ntp_customization_ntp_cards_bottom_sheet,
                                mViewFlipper,
                                true);

        // Verifies that the back back is not removed from the bottom sheet and the given listener
        // is set when BACK_PRESS_HANDLER is not null.
        PropertyModelChangeProcessor.create(
                mPropertyModel, ntpCardsBottomSheet, BottomSheetViewBinder::bind);
        View.OnClickListener backPressHandler = mock(View.OnClickListener.class);
        mPropertyModel.set(BACK_PRESS_HANDLER, backPressHandler);
        View backButton = mContentView.findViewById(R.id.back_button);
        backButton.performClick();
        verify(backPressHandler).onClick(backButton);

        // Verifies that the back button is removed from the bottom sheet when BACK_PRESS_HANDLER
        // is set to null.
        assertEquals(View.VISIBLE, backButton.getVisibility());
        mPropertyModel.set(BACK_PRESS_HANDLER, null);
        assertEquals(View.GONE, backButton.getVisibility());
    }

    @Test
    public void testBindOnFeedSettingsBottomSheet() {
        // Adds the feed settings bottom sheet to the view of the activity.
        View feedBottomSheet =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.ntp_customization_feed_bottom_sheet, mViewFlipper, true);
        PropertyModelChangeProcessor.create(
                mPropertyModel, feedBottomSheet, BottomSheetViewBinder::bind);

        // Verifies that the back back is not removed from the bottom sheet and the given listener
        // is set when the given back press handler is not null.
        PropertyModelChangeProcessor.create(
                mPropertyModel, feedBottomSheet, BottomSheetViewBinder::bind);
        View.OnClickListener backPressHandler = mock(View.OnClickListener.class);
        mPropertyModel.set(BACK_PRESS_HANDLER, backPressHandler);
        View backButton = mContentView.findViewById(R.id.back_button);
        backButton.performClick();
        verify(backPressHandler).onClick(backButton);

        // Verifies that the back button is removed from the bottom sheet when BACK_PRESS_HANDLER
        // is set to null.
        assertEquals(View.VISIBLE, backButton.getVisibility());
        mPropertyModel.set(BACK_PRESS_HANDLER, null);
        assertEquals(View.GONE, backButton.getVisibility());
    }
}
