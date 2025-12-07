// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link BottomSheetListContainerView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetListContainerViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ListContainerViewDelegate mDelegate;
    @Mock private BottomSheetListItemView mListItemView;
    private BottomSheetListContainerView mContainerView;

    private List<Integer> mListContent;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        View view =
                LayoutInflater.from(context)
                        .inflate(R.layout.ntp_customization_main_bottom_sheet, null, false);
        mContainerView = spy(view.findViewById(R.id.ntp_customization_options_container));

        // Spies on the containerView to avoid the inflating the MaterialSwitchWithText which
        // requires complicated setting of themes.
        doReturn(mListItemView).when(mContainerView).createListItemView();

        mListContent = List.of(NTP_CARDS, FEED);
        when(mDelegate.getListItems()).thenReturn(mListContent);
    }

    @Test
    public void testDelegateInRenderAllListItems() {
        mContainerView.renderAllListItems(mDelegate);

        // Verifies that getListItems(), getTitle(), getSubtitle(), getTrailingIcon(), getListener()
        // are called on delegate.
        verify(mDelegate).getListItems();
        for (int type : mListContent) {
            verify(mDelegate).getListItemId(eq(type));
            verify(mDelegate).getListItemTitle(eq(type), any(Context.class));
            verify(mDelegate).getListItemSubtitle(eq(type), any(Context.class));
            verify(mDelegate).getTrailingIcon(eq(type));
            verify(mDelegate).getListener(eq(type));
        }
    }

    @Test
    public void testRenderAllListItems() {
        View.OnClickListener listener = view -> {};
        for (int type : mListContent) {
            when(mDelegate.getListener(type)).thenReturn(listener);
        }

        mContainerView.renderAllListItems(mDelegate);

        // Verifies that titles, subtitles, backgrounds, trailing icons are set.
        int itemListSize = mListContent.size();
        verify(mListItemView, times(itemListSize)).setId(anyInt());
        verify(mListItemView, times(itemListSize)).setTitle(any());
        verify(mListItemView, times(itemListSize)).setSubtitle(any());
        verify(mListItemView, times(itemListSize)).setBackground(anyInt());
        verify(mListItemView, times(itemListSize)).setTrailingIcon(anyInt());

        // Verifies that if the listener is not null, it is set on the listItemView.
        verify(mListItemView, times(itemListSize)).setOnClickListener(eq(listener));
    }
}
