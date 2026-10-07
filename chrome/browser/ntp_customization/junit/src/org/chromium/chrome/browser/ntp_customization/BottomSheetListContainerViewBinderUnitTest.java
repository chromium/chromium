// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MVT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MAIN_BOTTOM_SHEET_NTP_CARDS_SECTION_SUBTITLE_RES_ID;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/** Unit tests for {@link BottomSheetListContainerViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetListContainerViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ListContainerViewDelegate mDelegate;

    private Context mContext;
    private BottomSheetListContainerView mMainBottomSheetListContainerView;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mMainBottomSheetListContainerView = new BottomSheetListContainerView(mContext, null);
        mPropertyModel = new PropertyModel(NtpCustomizationViewProperties.LIST_CONTAINER_KEYS);

        when(mDelegate.getListItems()).thenReturn(List.of(FEED, MVT, NTP_CARDS));
        when(mDelegate.getListItemId(FEED)).thenReturn(R.id.feed_settings);
        when(mDelegate.getListItemId(MVT)).thenReturn(R.id.mvt_settings);
        when(mDelegate.getListItemId(NTP_CARDS)).thenReturn(R.id.ntp_cards);
        when(mDelegate.getListItemTitle(anyInt(), any())).thenReturn("Title");
        when(mDelegate.getListItemSubtitle(anyInt(), any())).thenReturn("Subtitle");
        when(mDelegate.getTrailingIcon(anyInt())).thenReturn(null);
        when(mDelegate.getTrailingIconDescriptionResId(anyInt())).thenReturn(null);
    }

    @Test
    public void testBind() {
        PropertyModelChangeProcessor.create(
                mPropertyModel,
                mMainBottomSheetListContainerView,
                BottomSheetListContainerViewBinder::bind);

        // Verifies if the delegate is not null, it should be bound to the containerView.
        mPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, mDelegate);
        assertEquals(3, mMainBottomSheetListContainerView.getChildCount());

        // Verifies the delegate is null, the containerView should be destroyed.
        mPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, null);
        assertEquals(0, mMainBottomSheetListContainerView.getChildCount());

        mPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, mDelegate);

        // Verifies the feed section subtitle of the main bottom sheet will get updated timely.
        mPropertyModel.set(MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE, R.string.text_on);
        assertEquals("On", getSubtitle(R.id.feed_settings));
        mPropertyModel.set(MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE, R.string.text_off);
        assertEquals("Off", getSubtitle(R.id.feed_settings));

        // Verifies the mvt section subtitle of the main bottom sheet will get updated timely.
        mPropertyModel.set(MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE, R.string.text_on);
        assertEquals("On", getSubtitle(R.id.mvt_settings));
        mPropertyModel.set(MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE, R.string.text_off);
        assertEquals("Off", getSubtitle(R.id.mvt_settings));

        // Verifies the ntp cards section subtitle of the main bottom sheet will get updated.
        mPropertyModel.set(MAIN_BOTTOM_SHEET_NTP_CARDS_SECTION_SUBTITLE_RES_ID, R.string.text_on);
        assertEquals("On", getSubtitle(R.id.ntp_cards));
        mPropertyModel.set(MAIN_BOTTOM_SHEET_NTP_CARDS_SECTION_SUBTITLE_RES_ID, R.string.text_off);
        assertEquals("Off", getSubtitle(R.id.ntp_cards));
    }

    private String getSubtitle(int itemId) {
        View item = mMainBottomSheetListContainerView.findViewById(itemId);
        assertNotNull(item);
        return ((TextView) item.findViewById(R.id.subtitle)).getText().toString();
    }
}
