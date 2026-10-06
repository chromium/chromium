// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;

import android.content.Context;
import android.view.View;
import android.widget.ImageView;
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
import org.chromium.ui.base.ViewUtils;

import java.util.List;

/** Unit tests for {@link BottomSheetListContainerView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetListContainerViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ListContainerViewDelegate mDelegate;
    private BottomSheetListContainerView mContainerView;
    private Context mContext;

    private List<Integer> mListContent;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContainerView = new BottomSheetListContainerView(mContext, null);

        mListContent = List.of(NTP_CARDS, FEED);
        when(mDelegate.getListItems()).thenReturn(mListContent);
        // Mockito would otherwise return 0 for these Integer-returning methods.
        when(mDelegate.getTrailingIcon(anyInt())).thenReturn(null);
        when(mDelegate.getTrailingIconDescriptionResId(anyInt())).thenReturn(null);
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
        View.OnClickListener listener = ViewUtils.emptyClickListener();
        for (int type : mListContent) {
            when(mDelegate.getListItemId(type)).thenReturn(100 + type);
            when(mDelegate.getListItemTitle(eq(type), any(Context.class)))
                    .thenReturn("Title " + type);
            when(mDelegate.getListItemSubtitle(eq(type), any(Context.class)))
                    .thenReturn("Subtitle " + type);
            when(mDelegate.getListener(type)).thenReturn(listener);
        }
        // Only NTP_CARDS has a trailing icon.
        when(mDelegate.getTrailingIcon(NTP_CARDS)).thenReturn(R.drawable.forward_arrow_icon);
        when(mDelegate.getTrailingIconDescriptionResId(NTP_CARDS))
                .thenReturn(R.string.ntp_customization_theme_title);

        mContainerView.renderAllListItems(mDelegate);

        // Verifies that ids, titles, subtitles, backgrounds, trailing icons and listeners are set.
        int itemListSize = mListContent.size();
        assertEquals(itemListSize, mContainerView.getChildCount());
        for (int i = 0; i < itemListSize; i++) {
            int type = mListContent.get(i);
            View listItemView = mContainerView.getChildAt(i);
            assertEquals(100 + type, listItemView.getId());
            TextView title = listItemView.findViewById(R.id.title);
            assertEquals("Title " + type, title.getText().toString());
            TextView subtitle = listItemView.findViewById(R.id.subtitle);
            assertEquals("Subtitle " + type, subtitle.getText().toString());
            assertEquals(
                    NtpCustomizationUtils.getBackground(itemListSize, i),
                    shadowOf(listItemView.getBackground()).getCreatedFromResId());
            ImageView trailingIcon = listItemView.findViewById(R.id.trailing_icon);
            if (type == NTP_CARDS) {
                assertEquals(View.VISIBLE, trailingIcon.getVisibility());
                assertEquals(
                        R.drawable.forward_arrow_icon,
                        shadowOf(trailingIcon.getDrawable()).getCreatedFromResId());
                assertEquals(
                        mContext.getString(R.string.ntp_customization_theme_title),
                        trailingIcon.getContentDescription());
            } else {
                assertEquals(View.GONE, trailingIcon.getVisibility());
            }
            assertEquals(listener, shadowOf(listItemView).getOnClickListener());
        }
    }

    @Test
    public void testRenderAllListItems_ClearsExistingViews() {
        View staleView1 = new View(mContext);
        View staleView2 = new View(mContext);
        mContainerView.addView(staleView1);
        mContainerView.addView(staleView2);
        assertEquals(
                "Container should have 2 stale views initially", 2, mContainerView.getChildCount());

        List<Integer> items = List.of(NTP_CARDS);
        when(mDelegate.getListItems()).thenReturn(items);

        mContainerView.renderAllListItems(mDelegate);

        assertEquals(
                "Container should have exactly 1 view after re-rendering",
                1,
                mContainerView.getChildCount());

        View currentChild = mContainerView.getChildAt(0);
        assertNotSame("The stale view should have been removed", staleView1, currentChild);
        assertNotSame("The stale view should have been removed", staleView2, currentChild);
    }

    @Test
    public void testRenderAllListItems_EmptyView() {
        assertEquals(0, mContainerView.getChildCount());

        List<Integer> items = List.of(NTP_CARDS, FEED);
        when(mDelegate.getListItems()).thenReturn(items);

        mContainerView.renderAllListItems(mDelegate);

        assertEquals("Container should have 2 views", 2, mContainerView.getChildCount());
    }
}
