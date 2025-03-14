// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LAYOUT_TO_DISPLAY;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.NTP_CARDS_BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.NTP_CARDS_OPTION_CLICK_LISTENER;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ViewFlipper;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link NtpCustomizationCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCustomizationViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private View mContentView;
    private ViewFlipper mViewFlipperView;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.ntp_customization_bottom_sheet, /* root= */ null);
        activity.setContentView(mContentView);

        mPropertyModel = new PropertyModel(NtpCustomizationViewProperties.ALL_KEYS);
        mViewFlipperView = mContentView.findViewById(R.id.ntp_customization_view_flipper);
        PropertyModelChangeProcessor.create(
                mPropertyModel, mViewFlipperView, NtpCustomizationViewBinder::bind);
    }

    @Test
    @SmallTest
    public void testSetNtpCardsOptionClickListener() {
        // Adds the main bottom sheet to mViewFlipperView.
        LayoutInflater.from(mContext)
                .inflate(R.layout.ntp_customization_main_bottom_sheet, mViewFlipperView, true);
        View.OnClickListener ntpCardsClickListener = mock(View.OnClickListener.class);

        mPropertyModel.set(NTP_CARDS_OPTION_CLICK_LISTENER, ntpCardsClickListener);
        View ntpCards = mContentView.findViewById(R.id.new_tab_page_cards_list_item_container);

        assertNotNull(ntpCards);
        ntpCards.performClick();
        verify(ntpCardsClickListener).onClick(ntpCards);
    }

    @Test
    @SmallTest
    public void testSetNtpCardsBackPressHandler() {
        // Adds the ntp_cards bottom sheet to mViewFlipperView.
        LayoutInflater.from(mContext)
                .inflate(R.layout.ntp_customization_ntp_cards_bottom_sheet, mViewFlipperView, true);
        View.OnClickListener backPressHandler = mock(View.OnClickListener.class);

        mPropertyModel.set(NTP_CARDS_BACK_PRESS_HANDLER, backPressHandler);
        View backButton = mContentView.findViewById(R.id.ntp_cards_back_button);

        assertNotNull(backButton);
        backButton.performClick();
        verify(backPressHandler).onClick(backButton);
    }

    @Test
    @SmallTest
    public void testSetLayoutToDisplay() {
        ViewFlipper viewFlipperMock = mock(ViewFlipper.class);
        PropertyModelChangeProcessor.create(
                mPropertyModel, viewFlipperMock, NtpCustomizationViewBinder::bind);
        mPropertyModel.set(LAYOUT_TO_DISPLAY, 10);

        verify(viewFlipperMock).setDisplayedChild(10);
    }
}
