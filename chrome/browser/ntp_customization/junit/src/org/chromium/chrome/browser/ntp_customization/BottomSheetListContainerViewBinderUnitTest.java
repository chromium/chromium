// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MAIN_BOTTOM_SHEET_NTP_CARDS_SECTION_SUBTITLE_RES_ID;

import android.content.Context;

import androidx.test.filters.SmallTest;

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

/** Unit tests for {@link BottomSheetListContainerViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetListContainerViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetListContainerView mMainBottomSheetListContainerView;
    @Mock private BottomSheetListItemView mMainBottomSheetListItem;
    @Mock private Context mContext;

    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mPropertyModel = new PropertyModel(NtpCustomizationViewProperties.LIST_CONTAINER_KEYS);
    }

    @Test
    @SmallTest
    public void testBind() {
        PropertyModelChangeProcessor.create(
                mPropertyModel,
                mMainBottomSheetListContainerView,
                BottomSheetListContainerViewBinder::bind);

        // Verifies if the delegate is not null, it should be bound to the containerView.
        ListContainerViewDelegate delegate = mock(ListContainerViewDelegate.class);
        mPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, delegate);
        verify(mMainBottomSheetListContainerView).renderAllListItems(eq(delegate));

        // Verifies the delegate is null, the containerView should be destroyed.
        mPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, null);
        verify(mMainBottomSheetListContainerView).destroy();

        // Verifies the feed section subtitle of the main bottom sheet will get updated timely.
        when(mMainBottomSheetListContainerView.findViewById(R.id.feed_settings))
                .thenReturn(mMainBottomSheetListItem);
        when(mMainBottomSheetListContainerView.getContext()).thenReturn(mContext);
        when(mContext.getString(R.string.text_on)).thenReturn("On");
        when(mContext.getString(R.string.text_off)).thenReturn("Off");
        mPropertyModel.set(MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE, R.string.text_on);
        verify(mMainBottomSheetListItem).setSubtitle(eq("On"));
        mPropertyModel.set(MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE, R.string.text_off);
        verify(mMainBottomSheetListItem).setSubtitle(eq("Off"));

        // Verifies the mvt section subtitle of the main bottom sheet will get updated timely.
        clearInvocations(mMainBottomSheetListItem);
        when(mMainBottomSheetListContainerView.findViewById(R.id.mvt_settings))
                .thenReturn(mMainBottomSheetListItem);

        mPropertyModel.set(MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE, R.string.text_on);
        verify(mMainBottomSheetListItem).setSubtitle(eq("On"));
        mPropertyModel.set(MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE, R.string.text_off);
        verify(mMainBottomSheetListItem).setSubtitle(eq("Off"));

        // Verifies the ntp cards section subtitle of the main bottom sheet will get updated.
        clearInvocations(mMainBottomSheetListItem);
        when(mMainBottomSheetListContainerView.findViewById(R.id.ntp_cards))
                .thenReturn(mMainBottomSheetListItem);

        mPropertyModel.set(MAIN_BOTTOM_SHEET_NTP_CARDS_SECTION_SUBTITLE_RES_ID, R.string.text_on);
        verify(mMainBottomSheetListItem).setSubtitle(eq("On"));
        mPropertyModel.set(MAIN_BOTTOM_SHEET_NTP_CARDS_SECTION_SUBTITLE_RES_ID, R.string.text_off);
        verify(mMainBottomSheetListItem).setSubtitle(eq("Off"));
    }
}
